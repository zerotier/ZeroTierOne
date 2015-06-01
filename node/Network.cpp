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
						_membershipCertificates.insert(std::pair< Address,CertificateOfMembership >(com.issuedTo(),com));
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

		std::string buf("ZTMCD0");
		Utils::snprintf(n,sizeof(n),"networks.d/%.16llx.mcerts",_id);
		Mutex::Lock _l(_lock);

		if ((!_config)||(_config->isPublic())||(_membershipCertificates.size() == 0)) {
			RR->node->dataStoreDelete(n);
			return;
		}

		for(std::map<Address,CertificateOfMembership>::iterator c(_membershipCertificates.begin());c!=_membershipCertificates.end();++c)
			c->second.serialize2(buf);

		RR->node->dataStorePut(n,buf,true);
	}
}

bool Network::subscribedToMulticastGroup(const MulticastGroup &mg,bool includeBridgedGroups) const
{
	Mutex::Lock _l(_lock);
	if (std::binary_search(_myMulticastGroups.begin(),_myMulticastGroups.end(),mg))
		return true;
	else if (includeBridgedGroups)
		return (_multicastGroupsBehindMe.find(mg) != _multicastGroupsBehindMe.end());
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
			switch(RR->localNetworkController->doNetworkConfigRequest(InetAddress(),RR->identity,RR->identity,_id,Dictionary(),(nconf) ? nconf->revision() : (uint64_t)0,newconf)) {
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
	Packet outp(controller(),RR->identity.address(),Packet::VERB_NETWORK_CONFIG_REQUEST);
	outp.append((uint64_t)_id);
	outp.append((uint16_t)0); // no meta-data
	{
		Mutex::Lock _l(_lock);
		if (_config)
			outp.append((uint64_t)_config->revision());
		else outp.append((uint64_t)0);
	}
	RR->sw->send(outp,true);
}

void Network::addMembershipCertificate(const CertificateOfMembership &cert,bool forceAccept)
{
	if (!cert) // sanity check
		return;

	Mutex::Lock _l(_lock);
	CertificateOfMembership &old = _membershipCertificates[cert.issuedTo()];

	// Nothing to do if the cert hasn't changed -- we get duplicates due to zealous cert pushing
	if (old == cert)
		return;

	// Check signature, log and return if cert is invalid
	if (!forceAccept) {
		if (cert.signedBy() != controller()) {
			TRACE("rejected network membership certificate for %.16llx signed by %s: signer not a controller of this network",(unsigned long long)_id,cert.signedBy().toString().c_str());
			return;
		}

		SharedPtr<Peer> signer(RR->topology->getPeer(cert.signedBy()));

		if (!signer) {
			// This would be rather odd, since this is our controller... could happen
			// if we get packets before we've gotten config.
			RR->sw->requestWhois(cert.signedBy());
			return;
		}

		if (!cert.verify(signer->identity())) {
			TRACE("rejected network membership certificate for %.16llx signed by %s: signature check failed",(unsigned long long)_id,cert.signedBy().toString().c_str());
			return;
		}
	}

	// If we made it past authentication, update cert
	if (cert.revision() != old.revision())
		old = cert;
}

bool Network::peerNeedsOurMembershipCertificate(const Address &to,uint64_t now)
{
	Mutex::Lock _l(_lock);
	if ((_config)&&(!_config->isPublic())&&(_config->com())) {
		uint64_t &lastPushed = _lastPushedMembershipCertificate[to];
		if ((now - lastPushed) > (ZT_NETWORK_AUTOCONF_DELAY / 2)) {
			lastPushed = now;
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
		_membershipCertificates.clear();
		_lastPushedMembershipCertificate.clear();
	} else if (_config) {
		// Clean certificates that are no longer valid from the cache.
		for(std::map<Address,CertificateOfMembership>::iterator c=(_membershipCertificates.begin());c!=_membershipCertificates.end();) {
			if (_config->com().agreesWith(c->second))
				++c;
			else _membershipCertificates.erase(c++);
		}

		// Clean entries from the last pushed tracking map if they're so old as
		// to be no longer relevant.
		uint64_t forgetIfBefore = now - (ZT_PEER_ACTIVITY_TIMEOUT * 16); // arbitrary reasonable cutoff
		for(std::map<Address,uint64_t>::iterator lp(_lastPushedMembershipCertificate.begin());lp!=_lastPushedMembershipCertificate.end();) {
			if (lp->second < forgetIfBefore)
				_lastPushedMembershipCertificate.erase(lp++);
			else ++lp;
		}
	}

	// Clean learned multicast groups if we haven't heard from them in a while
	for(std::map<MulticastGroup,uint64_t>::iterator mg(_multicastGroupsBehindMe.begin());mg!=_multicastGroupsBehindMe.end();) {
		if ((now - mg->second) > (ZT_MULTICAST_LIKE_EXPIRE * 2))
			_multicastGroupsBehindMe.erase(mg++);
		else ++mg;
	}
}

bool Network::updateAndCheckMulticastBalance(const MulticastGroup &mg,unsigned int bytes)
{
	const uint64_t now = RR->node->now();
	Mutex::Lock _l(_lock);
	if (!_config)
		return false;
	std::map< MulticastGroup,BandwidthAccount >::iterator bal(_multicastRateAccounts.find(mg));
	if (bal == _multicastRateAccounts.end()) {
		NetworkConfig::MulticastRate r(_config->multicastRate(mg));
		bal = _multicastRateAccounts.insert(std::pair< MulticastGroup,BandwidthAccount >(mg,BandwidthAccount(r.preload,r.maxBalance,r.accrual,now))).first;
	}
	return bal->second.deduct(bytes,now);
}

void Network::learnBridgeRoute(const MAC &mac,const Address &addr)
{
	Mutex::Lock _l(_lock);
	_remoteBridgeRoutes[mac] = addr;

	// If _remoteBridgeRoutes exceeds sanity limit, trim worst offenders until below -- denial of service circuit breaker
	while (_remoteBridgeRoutes.size() > ZT_MAX_BRIDGE_ROUTES) {
		std::map<Address,unsigned long> counts;
		Address maxAddr;
		unsigned long maxCount = 0;
		for(std::map<MAC,Address>::iterator br(_remoteBridgeRoutes.begin());br!=_remoteBridgeRoutes.end();++br) {
			unsigned long c = ++counts[br->second];
			if (c > maxCount) {
				maxCount = c;
				maxAddr = br->second;
			}
		}
		for(std::map<MAC,Address>::iterator br(_remoteBridgeRoutes.begin());br!=_remoteBridgeRoutes.end();) {
			if (br->second == maxAddr)
				_remoteBridgeRoutes.erase(br++);
			else ++br;
		}
	}
}

void Network::learnBridgedMulticastGroup(const MulticastGroup &mg,uint64_t now)
{
	Mutex::Lock _l(_lock);
	unsigned long tmp = (unsigned long)_multicastGroupsBehindMe.size();
	_multicastGroupsBehindMe[mg] = now;
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

		std::map<Address,CertificateOfMembership>::const_iterator pc(_membershipCertificates.find(peer));
		if (pc == _membershipCertificates.end())
			return false; // no certificate on file

		return _config->com().agreesWith(pc->second); // is other cert valid against ours?
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
	for(std::map< MulticastGroup,uint64_t >::const_iterator i(_multicastGroupsBehindMe.begin());i!=_multicastGroupsBehindMe.end();++i)
		mgs.push_back(i->first);
	if ((_config)&&(_config->enableBroadcast()))
		mgs.push_back(Network::BROADCAST);
	std::sort(mgs.begin(),mgs.end());
	std::unique(mgs.begin(),mgs.end());
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
		_supernodeAddresses(renv->topology->supernodeAddresses()),
		_allMulticastGroups(nw->_allMulticastGroups())
	{}

	inline void operator()(Topology &t,const SharedPtr<Peer> &p)
	{
		if ( ( (p->hasActiveDirectPath(_now)) && (_network->_isAllowed(p->address())) ) || (std::find(_supernodeAddresses.begin(),_supernodeAddresses.end(),p->address()) != _supernodeAddresses.end()) ) {
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
	std::vector<Address> _supernodeAddresses;
	std::vector<MulticastGroup> _allMulticastGroups;
};

void Network::_announceMulticastGroups()
{
	// Assumes _lock is locked
	_AnnounceMulticastGroupsToPeersWithActiveDirectPaths afunc(RR,this);
	RR->topology->eachPeer<_AnnounceMulticastGroupsToPeersWithActiveDirectPaths &>(afunc);
}

} // namespace ZeroTier
