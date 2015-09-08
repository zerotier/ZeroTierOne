/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "Constants.hpp"
#include "Network.hpp"
#include "RuntimeEnvironment.hpp"
#include "Switch.hpp"
#include "Packet.hpp"
#include "Buffer.hpp"
#include "NetworkController.hpp"

#include "../version.h"

namespace ZeroTier {

const ZeroTier::MulticastGroup Network::BROADCAST(ZeroTier::MAC(0xffffffffffffULL),0);

Network::Network(const RuntimeEnvironment *renv,uint64_t nwid) :
	RR(renv),
	_id(nwid),
	_mac(renv->identity.address(),nwid),
	_enabled(true),
	_portInitialized(false),
	_lastConfigUpdate(0),
	_destroyed(false),
	_netconfFailure(NETCONF_FAILURE_NONE),
	_portError(0)
{
	char confn[128],mcdbn[128];
	Utils::snprintf(confn,sizeof(confn),"networks.d/%.16llx.conf",_id);
	Utils::snprintf(mcdbn,sizeof(mcdbn),"networks.d/%.16llx.mcerts",_id);

	if (_id == ZT_TEST_NETWORK_ID) {
		applyConfiguration(NetworkConfig::createTestNetworkConfig(RR->identity.address()));

		// Save a one-byte CR to persist membership in the test network
		RR->node->dataStorePut(confn,"\n",1,false);
	} else {
		bool gotConf = false;
		try {
			std::string conf(RR->node->dataStoreGet(confn));
			if (conf.length()) {
				setConfiguration(Dictionary(conf),false);
				_lastConfigUpdate = 0; // we still want to re-request a new config from the network
				gotConf = true;
			}
		} catch ( ... ) {} // ignore invalids, we'll re-request

		if (!gotConf) {
			// Save a one-byte CR to persist membership while we request a real netconf
			RR->node->dataStorePut(confn,"\n",1,false);
		}

		try {
			std::string mcdb(RR->node->dataStoreGet(mcdbn));
			if (mcdb.length() > 6) {
				const char *p = mcdb.data();
				const char *e = p + mcdb.length();
				if (!memcmp("ZTMCD0",p,6)) {
					p += 6;
					while (p != e) {
						CertificateOfMembership com;
						com.deserialize2(p,e);
						if (!com)
							break;
						_certInfo[com.issuedTo()].com = com;
					}
				}
			}
		} catch ( ... ) {} // ignore invalid MCDB, we'll re-learn from peers
	}

	if (!_portInitialized) {
		ZT1_VirtualNetworkConfig ctmp;
		_externalConfig(&ctmp);
		_portError = RR->node->configureVirtualNetworkPort(_id,ZT1_VIRTUAL_NETWORK_CONFIG_OPERATION_UP,&ctmp);
		_portInitialized = true;
	}
}

Network::~Network()
{
	ZT1_VirtualNetworkConfig ctmp;
	_externalConfig(&ctmp);

	char n[128];
	if (_destroyed) {
		RR->node->configureVirtualNetworkPort(_id,ZT1_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY,&ctmp);

		Utils::snprintf(n,sizeof(n),"networks.d/%.16llx.conf",_id);
		RR->node->dataStoreDelete(n);
		Utils::snprintf(n,sizeof(n),"networks.d/%.16llx.mcerts",_id);
		RR->node->dataStoreDelete(n);
	} else {
		RR->node->configureVirtualNetworkPort(_id,ZT1_VIRTUAL_NETWORK_CONFIG_OPERATION_DOWN,&ctmp);

		clean();

		Utils::snprintf(n,sizeof(n),"networks.d/%.16llx.mcerts",_id);

		Mutex::Lock _l(_lock);
		if ((!_config)||(_config->isPublic())||(_certInfo.empty())) {
			RR->node->dataStoreDelete(n);
		} else {
			std::string buf("ZTMCD0");
			Hashtable< Address,_RemoteMemberCertificateInfo >::Iterator i(_certInfo);
			Address *a = (Address *)0;
			_RemoteMemberCertificateInfo *ci = (_RemoteMemberCertificateInfo *)0;
			while (i.next(a,ci)) {
				if (ci->com)
					ci->com.serialize2(buf);
			}
			RR->node->dataStorePut(n,buf,true);
		}
	}
}

bool Network::subscribedToMulticastGroup(const MulticastGroup &mg,bool includeBridgedGroups) const
{
	Mutex::Lock _l(_lock);
	if (std::binary_search(_myMulticastGroups.begin(),_myMulticastGroups.end(),mg))
		return true;
	else if (includeBridgedGroups)
		return _multicastGroupsBehindMe.contains(mg);
	else return false;
}

void Network::multicastSubscribe(const MulticastGroup &mg)
{
	{
		Mutex::Lock _l(_lock);
		if (std::binary_search(_myMulticastGroups.begin(),_myMulticastGroups.end(),mg))
			return;
		_myMulticastGroups.push_back(mg);
		std::sort(_myMulticastGroups.begin(),_myMulticastGroups.end());
	}
	_announceMulticastGroups();
}

void Network::multicastUnsubscribe(const MulticastGroup &mg)
{
	Mutex::Lock _l(_lock);
	std::vector<MulticastGroup> nmg;
	for(std::vector<MulticastGroup>::const_iterator i(_myMulticastGroups.begin());i!=_myMulticastGroups.end();++i) {
		if (*i != mg)
			nmg.push_back(*i);
	}
	if (nmg.size() != _myMulticastGroups.size())
		_myMulticastGroups.swap(nmg);
}

bool Network::applyConfiguration(const SharedPtr<NetworkConfig> &conf)
{
	if (_destroyed) // sanity check
		return false;
	try {
		if ((conf->networkId() == _id)&&(conf->issuedTo() == RR->identity.address())) {
			ZT1_VirtualNetworkConfig ctmp;
			bool portInitialized;
			{
				Mutex::Lock _l(_lock);
				_config = conf;
				_lastConfigUpdate = RR->node->now();
				_netconfFailure = NETCONF_FAILURE_NONE;
				_externalConfig(&ctmp);
				portInitialized = _portInitialized;
				_portInitialized = true;
			}
			_portError = RR->node->configureVirtualNetworkPort(_id,(portInitialized) ? ZT1_VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE : ZT1_VIRTUAL_NETWORK_CONFIG_OPERATION_UP,&ctmp);
			return true;
		} else {
			TRACE("ignored invalid configuration for network %.16llx (configuration contains mismatched network ID or issued-to address)",(unsigned long long)_id);
		}
	} catch (std::exception &exc) {
		TRACE("ignored invalid configuration for network %.16llx (%s)",(unsigned long long)_id,exc.what());
	} catch ( ... ) {
		TRACE("ignored invalid configuration for network %.16llx (unknown exception)",(unsigned long long)_id);
	}
	return false;
}

int Network::setConfiguration(const Dictionary &conf,bool saveToDisk)
{
	try {
		const SharedPtr<NetworkConfig> newConfig(new NetworkConfig(conf)); // throws if invalid
		{
			Mutex::Lock _l(_lock);
			if ((_config)&&(*_config == *newConfig))
				return 1; // OK config, but duplicate of what we already have
		}
		if (applyConfiguration(newConfig)) {
			if (saveToDisk) {
				char n[128];
				Utils::snprintf(n,sizeof(n),"networks.d/%.16llx.conf",_id);
				RR->node->dataStorePut(n,conf.toString(),true);
			}
			return 2; // OK and configuration has changed
		}
	} catch ( ... ) {
		TRACE("ignored invalid configuration for network %.16llx (dictionary decode failed)",(unsigned long long)_id);
	}
	return 0;
}

void Network::requestConfiguration()
{
	if (_id == ZT_TEST_NETWORK_ID) // pseudo-network-ID, uses locally generated static config
		return;

	if (controller() == RR->identity.address()) {
		if (RR->localNetworkController) {
			SharedPtr<NetworkConfig> nconf(config2());
			Dictionary newconf;
			switch(RR->localNetworkController->doNetworkConfigRequest(InetAddress(),RR->identity,RR->identity,_id,Dictionary(),newconf)) {
				case NetworkController::NETCONF_QUERY_OK:
					this->setConfiguration(newconf,true);
					return;
				case NetworkController::NETCONF_QUERY_OBJECT_NOT_FOUND:
					this->setNotFound();
					return;
				case NetworkController::NETCONF_QUERY_ACCESS_DENIED:
					this->setAccessDenied();
					return;
				default:
					return;
			}
		} else {
			this->setNotFound();
			return;
		}
	}

	TRACE("requesting netconf for network %.16llx from controller %s",(unsigned long long)_id,controller().toString().c_str());

	// TODO: in the future we will include things like join tokens here, etc.
	Dictionary metaData;
	metaData.setHex(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_MAJOR_VERSION,ZEROTIER_ONE_VERSION_MAJOR);
	metaData.setHex(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_MINOR_VERSION,ZEROTIER_ONE_VERSION_MINOR);
	metaData.setHex(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_REVISION,ZEROTIER_ONE_VERSION_REVISION);
	std::string mds(metaData.toString());

	Packet outp(controller(),RR->identity.address(),Packet::VERB_NETWORK_CONFIG_REQUEST);
	outp.append((uint64_t)_id);
	outp.append((uint16_t)mds.length());
	outp.append((const void *)mds.data(),(unsigned int)mds.length());
	{
		Mutex::Lock _l(_lock);
		if (_config)
			outp.append((uint64_t)_config->revision());
		else outp.append((uint64_t)0);
	}
	RR->sw->send(outp,true,0);
}

bool Network::validateAndAddMembershipCertificate(const CertificateOfMembership &cert)
{
	if (!cert) // sanity check
		return false;

	Mutex::Lock _l(_lock);

	{
		const _RemoteMemberCertificateInfo *ci = _certInfo.get(cert.issuedTo());
		if ((ci)&&(ci->com == cert))
			return true; // we already have it
	}

	// Check signature, log and return if cert is invalid
	if (cert.signedBy() != controller()) {
		TRACE("rejected network membership certificate for %.16llx signed by %s: signer not a controller of this network",(unsigned long long)_id,cert.signedBy().toString().c_str());
		return false; // invalid signer
	}

	if (cert.signedBy() == RR->identity.address()) {

		// We are the controller: RR->identity.address() == controller() == cert.signedBy()
		// So, verify that we signed th cert ourself
		if (!cert.verify(RR->identity)) {
			TRACE("rejected network membership certificate for %.16llx self signed by %s: signature check failed",(unsigned long long)_id,cert.signedBy().toString().c_str());
			return false; // invalid signature
		}

	} else {

		SharedPtr<Peer> signer(RR->topology->getPeer(cert.signedBy()));

		if (!signer) {
			// This would be rather odd, since this is our controller... could happen
			// if we get packets before we've gotten config.
			RR->sw->requestWhois(cert.signedBy());
			return false; // signer unknown
		}

		if (!cert.verify(signer->identity())) {
			TRACE("rejected network membership certificate for %.16llx signed by %s: signature check failed",(unsigned long long)_id,cert.signedBy().toString().c_str());
			return false; // invalid signature
		}
	}

	// If we made it past authentication, add or update cert in our cert info store
	_certInfo[cert.issuedTo()].com = cert;

	return true;
}

bool Network::peerNeedsOurMembershipCertificate(const Address &to,uint64_t now)
{
	Mutex::Lock _l(_lock);
	if ((_config)&&(!_config->isPublic())&&(_config->com())) {
		_RemoteMemberCertificateInfo &ci = _certInfo[to];
		if ((now - ci.lastPushed) > (ZT_NETWORK_AUTOCONF_DELAY / 2)) {
			ci.lastPushed = now;
			return true;
		}
	}
	return false;
}

void Network::clean()
{
	const uint64_t now = RR->node->now();
	Mutex::Lock _l(_lock);

	if (_destroyed)
		return;

	if ((_config)&&(_config->isPublic())) {
		// Open (public) networks do not track certs or cert pushes at all.
		_certInfo.clear();
	} else if (_config) {
		// Clean obsolete entries from private network cert info table
		Hashtable< Address,_RemoteMemberCertificateInfo >::Iterator i(_certInfo);
		Address *a = (Address *)0;
		_RemoteMemberCertificateInfo *ci = (_RemoteMemberCertificateInfo *)0;
		const uint64_t forgetIfBefore = now - (ZT_PEER_ACTIVITY_TIMEOUT * 16); // arbitrary reasonable cutoff
		while (i.next(a,ci)) {
			if ((ci->lastPushed < forgetIfBefore)&&(!ci->com.agreesWith(_config->com())))
				_certInfo.erase(*a);
		}
	}

	// Clean learned multicast groups if we haven't heard from them in a while
	{
		Hashtable< MulticastGroup,uint64_t >::Iterator i(_multicastGroupsBehindMe);
		MulticastGroup *mg = (MulticastGroup *)0;
		uint64_t *ts = (uint64_t *)0;
		while (i.next(mg,ts)) {
			if ((now - *ts) > (ZT_MULTICAST_LIKE_EXPIRE * 2))
				_multicastGroupsBehindMe.erase(*mg);
		}
	}
}

void Network::learnBridgeRoute(const MAC &mac,const Address &addr)
{
	Mutex::Lock _l(_lock);
	_remoteBridgeRoutes[mac] = addr;

	// Anti-DOS circuit breaker to prevent nodes from spamming us with absurd numbers of bridge routes
	while (_remoteBridgeRoutes.size() > ZT_MAX_BRIDGE_ROUTES) {
		Hashtable< Address,unsigned long > counts;
		Address maxAddr;
		unsigned long maxCount = 0;

		MAC *m = (MAC *)0;
		Address *a = (Address *)0;

		// Find the address responsible for the most entries
		{
			Hashtable<MAC,Address>::Iterator i(_remoteBridgeRoutes);
			while (i.next(m,a)) {
				const unsigned long c = ++counts[*a];
				if (c > maxCount) {
					maxCount = c;
					maxAddr = *a;
				}
			}
		}

		// Kill this address from our table, since it's most likely spamming us
		{
			Hashtable<MAC,Address>::Iterator i(_remoteBridgeRoutes);
			while (i.next(m,a)) {
				if (*a == maxAddr)
					_remoteBridgeRoutes.erase(*m);
			}
		}
	}
}

void Network::learnBridgedMulticastGroup(const MulticastGroup &mg,uint64_t now)
{
	Mutex::Lock _l(_lock);
	const unsigned long tmp = (unsigned long)_multicastGroupsBehindMe.size();
	_multicastGroupsBehindMe.set(mg,now);
	if (tmp != _multicastGroupsBehindMe.size())
		_announceMulticastGroups();
}

void Network::setEnabled(bool enabled)
{
	Mutex::Lock _l(_lock);
	if (_enabled != enabled) {
		_enabled = enabled;
		ZT1_VirtualNetworkConfig ctmp;
		_externalConfig(&ctmp);
		_portError = RR->node->configureVirtualNetworkPort(_id,ZT1_VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE,&ctmp);
	}
}

void Network::destroy()
{
	Mutex::Lock _l(_lock);
	_enabled = false;
	_destroyed = true;
}

ZT1_VirtualNetworkStatus Network::_status() const
{
	// assumes _lock is locked
	if (_portError)
		return ZT1_NETWORK_STATUS_PORT_ERROR;
	switch(_netconfFailure) {
		case NETCONF_FAILURE_ACCESS_DENIED:
			return ZT1_NETWORK_STATUS_ACCESS_DENIED;
		case NETCONF_FAILURE_NOT_FOUND:
			return ZT1_NETWORK_STATUS_NOT_FOUND;
		case NETCONF_FAILURE_NONE:
			return ((_config) ? ZT1_NETWORK_STATUS_OK : ZT1_NETWORK_STATUS_REQUESTING_CONFIGURATION);
		default:
			return ZT1_NETWORK_STATUS_PORT_ERROR;
	}
}

void Network::_externalConfig(ZT1_VirtualNetworkConfig *ec) const
{
	// assumes _lock is locked
	ec->nwid = _id;
	ec->mac = _mac.toInt();
	if (_config)
		Utils::scopy(ec->name,sizeof(ec->name),_config->name().c_str());
	else ec->name[0] = (char)0;
	ec->status = _status();
	ec->type = (_config) ? (_config->isPrivate() ? ZT1_NETWORK_TYPE_PRIVATE : ZT1_NETWORK_TYPE_PUBLIC) : ZT1_NETWORK_TYPE_PRIVATE;
	ec->mtu = ZT_IF_MTU;
	ec->dhcp = 0;
	ec->bridge = (_config) ? ((_config->allowPassiveBridging() || (std::find(_config->activeBridges().begin(),_config->activeBridges().end(),RR->identity.address()) != _config->activeBridges().end())) ? 1 : 0) : 0;
	ec->broadcastEnabled = (_config) ? (_config->enableBroadcast() ? 1 : 0) : 0;
	ec->portError = _portError;
	ec->enabled = (_enabled) ? 1 : 0;
	ec->netconfRevision = (_config) ? (unsigned long)_config->revision() : 0;

	ec->multicastSubscriptionCount = std::min((unsigned int)_myMulticastGroups.size(),(unsigned int)ZT1_MAX_NETWORK_MULTICAST_SUBSCRIPTIONS);
	for(unsigned int i=0;i<ec->multicastSubscriptionCount;++i) {
		ec->multicastSubscriptions[i].mac = _myMulticastGroups[i].mac().toInt();
		ec->multicastSubscriptions[i].adi = _myMulticastGroups[i].adi();
	}

	if (_config) {
		ec->assignedAddressCount = (unsigned int)_config->staticIps().size();
		for(unsigned long i=0;i<ZT1_MAX_ZT_ASSIGNED_ADDRESSES;++i) {
			if (i < _config->staticIps().size())
				memcpy(&(ec->assignedAddresses[i]),&(_config->staticIps()[i]),sizeof(struct sockaddr_storage));
		}
	} else ec->assignedAddressCount = 0;
}

bool Network::_isAllowed(const Address &peer) const
{
	// Assumes _lock is locked
	try {
		if (!_config)
			return false;
		if (_config->isPublic())
			return true;
		const _RemoteMemberCertificateInfo *ci = _certInfo.get(peer);
		if (!ci)
			return false;
		return _config->com().agreesWith(ci->com);
	} catch (std::exception &exc) {
		TRACE("isAllowed() check failed for peer %s: unexpected exception: %s",peer.toString().c_str(),exc.what());
	} catch ( ... ) {
		TRACE("isAllowed() check failed for peer %s: unexpected exception: unknown exception",peer.toString().c_str());
	}
	return false; // default position on any failure
}

std::vector<MulticastGroup> Network::_allMulticastGroups() const
{
	// Assumes _lock is locked
	std::vector<MulticastGroup> mgs;
	mgs.reserve(_myMulticastGroups.size() + _multicastGroupsBehindMe.size() + 1);
	mgs.insert(mgs.end(),_myMulticastGroups.begin(),_myMulticastGroups.end());
	_multicastGroupsBehindMe.appendKeys(mgs);
	if ((_config)&&(_config->enableBroadcast()))
		mgs.push_back(Network::BROADCAST);
	std::sort(mgs.begin(),mgs.end());
	mgs.erase(std::unique(mgs.begin(),mgs.end()),mgs.end());
	return mgs;
}

// Used in Network::_announceMulticastGroups()
class _AnnounceMulticastGroupsToPeersWithActiveDirectPaths
{
public:
	_AnnounceMulticastGroupsToPeersWithActiveDirectPaths(const RuntimeEnvironment *renv,Network *nw) :
		RR(renv),
		_now(renv->node->now()),
		_network(nw),
		_rootAddresses(renv->topology->rootAddresses()),
		_allMulticastGroups(nw->_allMulticastGroups())
	{}

	inline void operator()(Topology &t,const SharedPtr<Peer> &p)
	{
		if ( ( (p->hasActiveDirectPath(_now)) && ( (_network->_isAllowed(p->address())) || (p->address() == _network->controller()) ) ) || (std::find(_rootAddresses.begin(),_rootAddresses.end(),p->address()) != _rootAddresses.end()) ) {
			Packet outp(p->address(),RR->identity.address(),Packet::VERB_MULTICAST_LIKE);

			for(std::vector<MulticastGroup>::iterator mg(_allMulticastGroups.begin());mg!=_allMulticastGroups.end();++mg) {
				if ((outp.size() + 18) >= ZT_UDP_DEFAULT_PAYLOAD_MTU) {
					outp.armor(p->key(),true);
					p->send(RR,outp.data(),outp.size(),_now);
					outp.reset(p->address(),RR->identity.address(),Packet::VERB_MULTICAST_LIKE);
				}

				// network ID, MAC, ADI
				outp.append((uint64_t)_network->id());
				mg->mac().appendTo(outp);
				outp.append((uint32_t)mg->adi());
			}

			if (outp.size() > ZT_PROTO_MIN_PACKET_LENGTH) {
				outp.armor(p->key(),true);
				p->send(RR,outp.data(),outp.size(),_now);
			}
		}
	}

private:
	const RuntimeEnvironment *RR;
	uint64_t _now;
	Network *_network;
	std::vector<Address> _rootAddresses;
	std::vector<MulticastGroup> _allMulticastGroups;
};

void Network::_announceMulticastGroups()
{
	// Assumes _lock is locked
	_AnnounceMulticastGroupsToPeersWithActiveDirectPaths afunc(RR,this);
	RR->topology->eachPeer<_AnnounceMulticastGroupsToPeersWithActiveDirectPaths &>(afunc);
}

} // namespace ZeroTier
