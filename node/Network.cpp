/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
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
#include "Node.hpp"

#include "../version.h"

namespace ZeroTier {

const ZeroTier::MulticastGroup Network::BROADCAST(ZeroTier::MAC(0xffffffffffffULL),0);

Network::Network(const RuntimeEnvironment *renv,uint64_t nwid,void *uptr) :
	RR(renv),
	_uPtr(uptr),
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

	// These files are no longer used, so clean them.
	RR->node->dataStoreDelete(mcdbn);

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
	}

	if (!_portInitialized) {
		ZT_VirtualNetworkConfig ctmp;
		_externalConfig(&ctmp);
		_portError = RR->node->configureVirtualNetworkPort(_id,&_uPtr,ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_UP,&ctmp);
		_portInitialized = true;
	}
}

Network::~Network()
{
	ZT_VirtualNetworkConfig ctmp;
	_externalConfig(&ctmp);

	char n[128];
	if (_destroyed) {
		RR->node->configureVirtualNetworkPort(_id,&_uPtr,ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY,&ctmp);
		Utils::snprintf(n,sizeof(n),"networks.d/%.16llx.conf",_id);
		RR->node->dataStoreDelete(n);
	} else {
		RR->node->configureVirtualNetworkPort(_id,&_uPtr,ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DOWN,&ctmp);
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

bool Network::tryAnnounceMulticastGroupsTo(const SharedPtr<Peer> &peer)
{
	Mutex::Lock _l(_lock);
	if (
	    (_isAllowed(peer)) ||
	    (peer->address() == this->controller()) ||
	    (RR->topology->isRoot(peer->identity()))
	   ) {
		_announceMulticastGroupsTo(peer->address(),_allMulticastGroups());
		return true;
	}
	return false;
}

bool Network::applyConfiguration(const NetworkConfig &conf)
{
	if (_destroyed) // sanity check
		return false;
	try {
		if ((conf.networkId() == _id)&&(conf.issuedTo() == RR->identity.address())) {
			ZT_VirtualNetworkConfig ctmp;
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
			_portError = RR->node->configureVirtualNetworkPort(_id,&_uPtr,(portInitialized) ? ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE : ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_UP,&ctmp);
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
		NetworkConfig newConfig;
		newConfig.fromDictionary(conf); // throws if invalid
		{
			Mutex::Lock _l(_lock);
			if (_config == newConfig)
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
	if (_config)
		outp.append((uint64_t)_config.revision());
	else outp.append((uint64_t)0);
	RR->sw->send(outp,true,0);
}

void Network::clean()
{
	const uint64_t now = RR->node->now();
	Mutex::Lock _l(_lock);

	if (_destroyed)
		return;

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
		ZT_VirtualNetworkConfig ctmp;
		_externalConfig(&ctmp);
		_portError = RR->node->configureVirtualNetworkPort(_id,&_uPtr,ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE,&ctmp);
	}
}

void Network::destroy()
{
	Mutex::Lock _l(_lock);
	_enabled = false;
	_destroyed = true;
}

ZT_VirtualNetworkStatus Network::_status() const
{
	// assumes _lock is locked
	if (_portError)
		return ZT_NETWORK_STATUS_PORT_ERROR;
	switch(_netconfFailure) {
		case NETCONF_FAILURE_ACCESS_DENIED:
			return ZT_NETWORK_STATUS_ACCESS_DENIED;
		case NETCONF_FAILURE_NOT_FOUND:
			return ZT_NETWORK_STATUS_NOT_FOUND;
		case NETCONF_FAILURE_NONE:
			return ((_config) ? ZT_NETWORK_STATUS_OK : ZT_NETWORK_STATUS_REQUESTING_CONFIGURATION);
		default:
			return ZT_NETWORK_STATUS_PORT_ERROR;
	}
}

void Network::_externalConfig(ZT_VirtualNetworkConfig *ec) const
{
	// assumes _lock is locked
	ec->nwid = _id;
	ec->mac = _mac.toInt();
	if (_config)
		Utils::scopy(ec->name,sizeof(ec->name),_config.name());
	else ec->name[0] = (char)0;
	ec->status = _status();
	ec->type = (_config) ? (_config.isPrivate() ? ZT_NETWORK_TYPE_PRIVATE : ZT_NETWORK_TYPE_PUBLIC) : ZT_NETWORK_TYPE_PRIVATE;
	ec->mtu = ZT_IF_MTU;
	ec->dhcp = 0;
	std::vector<Address> ab(_config.activeBridges());
	ec->bridge = ((_config.allowPassiveBridging())||(std::find(ab.begin(),ab.end(),RR->identity.address()) != ab.end())) ? 1 : 0;
	ec->broadcastEnabled = (_config) ? (_config.enableBroadcast() ? 1 : 0) : 0;
	ec->portError = _portError;
	ec->enabled = (_enabled) ? 1 : 0;
	ec->netconfRevision = (_config) ? (unsigned long)_config.revision() : 0;

	ec->multicastSubscriptionCount = std::min((unsigned int)_myMulticastGroups.size(),(unsigned int)ZT_MAX_NETWORK_MULTICAST_SUBSCRIPTIONS);
	for(unsigned int i=0;i<ec->multicastSubscriptionCount;++i) {
		ec->multicastSubscriptions[i].mac = _myMulticastGroups[i].mac().toInt();
		ec->multicastSubscriptions[i].adi = _myMulticastGroups[i].adi();
	}

	std::vector<InetAddress> sips(_config.staticIps());
	ec->assignedAddressCount = 0;
	for(unsigned long i=0;i<ZT_MAX_ZT_ASSIGNED_ADDRESSES;++i) {
		if (i < sips.size()) {
			memcpy(&(ec->assignedAddresses[i]),&(sips[i]),sizeof(struct sockaddr_storage));
			++ec->assignedAddressCount;
		} else memset(&(ec->assignedAddresses[i]),0,sizeof(struct sockaddr_storage));
	}
}

bool Network::_isAllowed(const SharedPtr<Peer> &peer) const
{
	// Assumes _lock is locked
	try {
		if (!_config)
			return false;
		if (_config.isPublic())
			return true;
		return ((_config.com())&&(peer->networkMembershipCertificatesAgree(_id,_config.com())));
	} catch (std::exception &exc) {
		TRACE("isAllowed() check failed for peer %s: unexpected exception: %s",peer->address().toString().c_str(),exc.what());
	} catch ( ... ) {
		TRACE("isAllowed() check failed for peer %s: unexpected exception: unknown exception",peer->address().toString().c_str());
	}
	return false; // default position on any failure
}

class _GetPeersThatNeedMulticastAnnouncement
{
public:
	_GetPeersThatNeedMulticastAnnouncement(const RuntimeEnvironment *renv,Network *nw) :
		_now(renv->node->now()),
		_controller(nw->controller()),
		_network(nw),
		_rootAddresses(renv->topology->rootAddresses())
	{}
	inline void operator()(Topology &t,const SharedPtr<Peer> &p)
	{
		if (
		    (_network->_isAllowed(p)) ||
		    (p->address() == _controller) ||
		    (std::find(_rootAddresses.begin(),_rootAddresses.end(),p->address()) != _rootAddresses.end())
		   ) {
			peers.push_back(p->address());
		}
	}
	std::vector<Address> peers;
private:
	uint64_t _now;
	Address _controller;
	Network *_network;
	std::vector<Address> _rootAddresses;
};
void Network::_announceMulticastGroups()
{
	// Assumes _lock is locked

	_GetPeersThatNeedMulticastAnnouncement gpfunc(RR,this);
	RR->topology->eachPeer<_GetPeersThatNeedMulticastAnnouncement &>(gpfunc);

	std::vector<MulticastGroup> allMulticastGroups(_allMulticastGroups());
	for(std::vector<Address>::const_iterator pa(gpfunc.peers.begin());pa!=gpfunc.peers.end();++pa)
		_announceMulticastGroupsTo(*pa,allMulticastGroups);
}

void Network::_announceMulticastGroupsTo(const Address &peerAddress,const std::vector<MulticastGroup> &allMulticastGroups) const
{
	// Assumes _lock is locked

	// We push COMs ahead of MULTICAST_LIKE since they're used for access control -- a COM is a public
	// credential so "over-sharing" isn't really an issue (and we only do so with roots).
	if ((_config)&&(_config.com())&&(!_config.isPublic())) {
		Packet outp(peerAddress,RR->identity.address(),Packet::VERB_NETWORK_MEMBERSHIP_CERTIFICATE);
		_config.com().serialize(outp);
		RR->sw->send(outp,true,0);
	}

	{
		Packet outp(peerAddress,RR->identity.address(),Packet::VERB_MULTICAST_LIKE);

		for(std::vector<MulticastGroup>::const_iterator mg(allMulticastGroups.begin());mg!=allMulticastGroups.end();++mg) {
			if ((outp.size() + 18) >= ZT_UDP_DEFAULT_PAYLOAD_MTU) {
				RR->sw->send(outp,true,0);
				outp.reset(peerAddress,RR->identity.address(),Packet::VERB_MULTICAST_LIKE);
			}

			// network ID, MAC, ADI
			outp.append((uint64_t)_id);
			mg->mac().appendTo(outp);
			outp.append((uint32_t)mg->adi());
		}

		if (outp.size() > ZT_PROTO_MIN_PACKET_LENGTH)
			RR->sw->send(outp,true,0);
	}
}

std::vector<MulticastGroup> Network::_allMulticastGroups() const
{
	// Assumes _lock is locked

	std::vector<MulticastGroup> mgs;
	mgs.reserve(_myMulticastGroups.size() + _multicastGroupsBehindMe.size() + 1);
	mgs.insert(mgs.end(),_myMulticastGroups.begin(),_myMulticastGroups.end());
	_multicastGroupsBehindMe.appendKeys(mgs);
	if ((_config)&&(_config.enableBroadcast()))
		mgs.push_back(Network::BROADCAST);
	std::sort(mgs.begin(),mgs.end());
	mgs.erase(std::unique(mgs.begin(),mgs.end()),mgs.end());

	return mgs;
}

} // namespace ZeroTier
