/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2017  ZeroTier, Inc.  https://www.zerotier.com/
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
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

#include "../version.h"

#include "Constants.hpp"
#include "Node.hpp"
#include "RuntimeEnvironment.hpp"
#include "NetworkController.hpp"
#include "Switch.hpp"
#include "Multicaster.hpp"
#include "Topology.hpp"
#include "Buffer.hpp"
#include "Packet.hpp"
#include "Address.hpp"
#include "Identity.hpp"
#include "SelfAwareness.hpp"
#include "Cluster.hpp"

const struct sockaddr_storage ZT_SOCKADDR_NULL = {0};

namespace ZeroTier {

/****************************************************************************/
/* Public Node interface (C++, exposed via CAPI bindings)                   */
/****************************************************************************/

Node::Node(void *uptr,void *tptr,const struct ZT_Node_Callbacks *callbacks,uint64_t now) :
	_RR(this),
	RR(&_RR),
	_uPtr(uptr),
	_now(now),
	_lastPingCheck(0),
	_lastHousekeepingRun(0)
{
	if (callbacks->version != 0)
		throw std::runtime_error("callbacks struct version mismatch");
	memcpy(&_cb,callbacks,sizeof(ZT_Node_Callbacks));

	Utils::getSecureRandom((void *)_prngState,sizeof(_prngState));

	_online = false;

	memset(_expectingRepliesToBucketPtr,0,sizeof(_expectingRepliesToBucketPtr));
	memset(_expectingRepliesTo,0,sizeof(_expectingRepliesTo));
	memset(_lastIdentityVerification,0,sizeof(_lastIdentityVerification));

	std::string idtmp(dataStoreGet(tptr,"identity.secret"));
	if ((!idtmp.length())||(!RR->identity.fromString(idtmp))||(!RR->identity.hasPrivate())) {
		TRACE("identity.secret not found, generating...");
		RR->identity.generate();
		idtmp = RR->identity.toString(true);
		if (!dataStorePut(tptr,"identity.secret",idtmp,true))
			throw std::runtime_error("unable to write identity.secret");
	}
	RR->publicIdentityStr = RR->identity.toString(false);
	RR->secretIdentityStr = RR->identity.toString(true);
	idtmp = dataStoreGet(tptr,"identity.public");
	if (idtmp != RR->publicIdentityStr) {
		if (!dataStorePut(tptr,"identity.public",RR->publicIdentityStr,false))
			throw std::runtime_error("unable to write identity.public");
	}

	try {
		RR->sw = new Switch(RR);
		RR->mc = new Multicaster(RR);
		RR->topology = new Topology(RR,tptr);
		RR->sa = new SelfAwareness(RR);
	} catch ( ... ) {
		delete RR->sa;
		delete RR->topology;
		delete RR->mc;
		delete RR->sw;
		throw;
	}

	postEvent(tptr,ZT_EVENT_UP);
}

Node::~Node()
{
	Mutex::Lock _l(_networks_m);

	_networks.clear(); // ensure that networks are destroyed before shutdow

	delete RR->sa;
	delete RR->topology;
	delete RR->mc;
	delete RR->sw;

#ifdef ZT_ENABLE_CLUSTER
	delete RR->cluster;
#endif
}

ZT_ResultCode Node::processWirePacket(
	void *tptr,
	uint64_t now,
	const struct sockaddr_storage *localAddress,
	const struct sockaddr_storage *remoteAddress,
	const void *packetData,
	unsigned int packetLength,
	volatile uint64_t *nextBackgroundTaskDeadline)
{
	_now = now;
	RR->sw->onRemotePacket(tptr,*(reinterpret_cast<const InetAddress *>(localAddress)),*(reinterpret_cast<const InetAddress *>(remoteAddress)),packetData,packetLength);
	return ZT_RESULT_OK;
}

ZT_ResultCode Node::processVirtualNetworkFrame(
	void *tptr,
	uint64_t now,
	uint64_t nwid,
	uint64_t sourceMac,
	uint64_t destMac,
	unsigned int etherType,
	unsigned int vlanId,
	const void *frameData,
	unsigned int frameLength,
	volatile uint64_t *nextBackgroundTaskDeadline)
{
	_now = now;
	SharedPtr<Network> nw(this->network(nwid));
	if (nw) {
		RR->sw->onLocalEthernet(tptr,nw,MAC(sourceMac),MAC(destMac),etherType,vlanId,frameData,frameLength);
		return ZT_RESULT_OK;
	} else return ZT_RESULT_ERROR_NETWORK_NOT_FOUND;
}

// Closure used to ping upstream and active/online peers
class _PingPeersThatNeedPing
{
public:
	_PingPeersThatNeedPing(const RuntimeEnvironment *renv,void *tPtr,Hashtable< Address,std::vector<InetAddress> > &upstreamsToContact,uint64_t now) :
		lastReceiveFromUpstream(0),
		RR(renv),
		_tPtr(tPtr),
		_upstreamsToContact(upstreamsToContact),
		_now(now),
		_bestCurrentUpstream(RR->topology->getUpstreamPeer())
	{
	}

	uint64_t lastReceiveFromUpstream; // tracks last time we got a packet from an 'upstream' peer like a root or a relay

	inline void operator()(Topology &t,const SharedPtr<Peer> &p)
	{
		const std::vector<InetAddress> *const upstreamStableEndpoints = _upstreamsToContact.get(p->address());
		if (upstreamStableEndpoints) {
			bool contacted = false;

			// Upstreams must be pinged constantly over both IPv4 and IPv6 to allow
			// them to perform three way handshake introductions for both stacks.

			if (!p->doPingAndKeepalive(_tPtr,_now,AF_INET)) {
				for(unsigned long k=0,ptr=(unsigned long)RR->node->prng();k<(unsigned long)upstreamStableEndpoints->size();++k) {
					const InetAddress &addr = (*upstreamStableEndpoints)[ptr++ % upstreamStableEndpoints->size()];
					if (addr.ss_family == AF_INET) {
						p->sendHELLO(_tPtr,InetAddress(),addr,_now,0);
						contacted = true;
						break;
					}
				}
			} else contacted = true;
			if (!p->doPingAndKeepalive(_tPtr,_now,AF_INET6)) {
				for(unsigned long k=0,ptr=(unsigned long)RR->node->prng();k<(unsigned long)upstreamStableEndpoints->size();++k) {
					const InetAddress &addr = (*upstreamStableEndpoints)[ptr++ % upstreamStableEndpoints->size()];
					if (addr.ss_family == AF_INET6) {
						p->sendHELLO(_tPtr,InetAddress(),addr,_now,0);
						contacted = true;
						break;
					}
				}
			} else contacted = true;

			if ((!contacted)&&(_bestCurrentUpstream)) {
				const SharedPtr<Path> up(_bestCurrentUpstream->getBestPath(_now,true));
				if (up)
					p->sendHELLO(_tPtr,up->localAddress(),up->address(),_now,up->nextOutgoingCounter());
			}

			lastReceiveFromUpstream = std::max(p->lastReceive(),lastReceiveFromUpstream);
			_upstreamsToContact.erase(p->address()); // erase from upstreams to contact so that we can WHOIS those that remain
		} else if (p->isActive(_now)) {
			p->doPingAndKeepalive(_tPtr,_now,-1);
		}
	}

private:
	const RuntimeEnvironment *RR;
	void *_tPtr;
	Hashtable< Address,std::vector<InetAddress> > &_upstreamsToContact;
	const uint64_t _now;
	const SharedPtr<Peer> _bestCurrentUpstream;
};

ZT_ResultCode Node::processBackgroundTasks(void *tptr,uint64_t now,volatile uint64_t *nextBackgroundTaskDeadline)
{
	_now = now;
	Mutex::Lock bl(_backgroundTasksLock);

	unsigned long timeUntilNextPingCheck = ZT_PING_CHECK_INVERVAL;
	const uint64_t timeSinceLastPingCheck = now - _lastPingCheck;
	if (timeSinceLastPingCheck >= ZT_PING_CHECK_INVERVAL) {
		try {
			_lastPingCheck = now;

			// Get networks that need config without leaving mutex locked
			std::vector< SharedPtr<Network> > needConfig;
			{
				Mutex::Lock _l(_networks_m);
				for(std::vector< std::pair< uint64_t,SharedPtr<Network> > >::const_iterator n(_networks.begin());n!=_networks.end();++n) {
					if (((now - n->second->lastConfigUpdate()) >= ZT_NETWORK_AUTOCONF_DELAY)||(!n->second->hasConfig()))
						needConfig.push_back(n->second);
					n->second->sendUpdatesToMembers(tptr);
				}
			}
			for(std::vector< SharedPtr<Network> >::const_iterator n(needConfig.begin());n!=needConfig.end();++n)
				(*n)->requestConfiguration(tptr);

			// Do pings and keepalives
			Hashtable< Address,std::vector<InetAddress> > upstreamsToContact;
			RR->topology->getUpstreamsToContact(upstreamsToContact);
			_PingPeersThatNeedPing pfunc(RR,tptr,upstreamsToContact,now);
			RR->topology->eachPeer<_PingPeersThatNeedPing &>(pfunc);

			// Run WHOIS to create Peer for any upstreams we could not contact (including pending moon seeds)
			Hashtable< Address,std::vector<InetAddress> >::Iterator i(upstreamsToContact);
			Address *upstreamAddress = (Address *)0;
			std::vector<InetAddress> *upstreamStableEndpoints = (std::vector<InetAddress> *)0;
			while (i.next(upstreamAddress,upstreamStableEndpoints))
				RR->sw->requestWhois(tptr,*upstreamAddress);

			// Update online status, post status change as event
			const bool oldOnline = _online;
			_online = (((now - pfunc.lastReceiveFromUpstream) < ZT_PEER_ACTIVITY_TIMEOUT)||(RR->topology->amRoot()));
			if (oldOnline != _online)
				postEvent(tptr,_online ? ZT_EVENT_ONLINE : ZT_EVENT_OFFLINE);
		} catch ( ... ) {
			return ZT_RESULT_FATAL_ERROR_INTERNAL;
		}
	} else {
		timeUntilNextPingCheck -= (unsigned long)timeSinceLastPingCheck;
	}

	if ((now - _lastHousekeepingRun) >= ZT_HOUSEKEEPING_PERIOD) {
		try {
			_lastHousekeepingRun = now;
			RR->topology->clean(now);
			RR->sa->clean(now);
			RR->mc->clean(now);
		} catch ( ... ) {
			return ZT_RESULT_FATAL_ERROR_INTERNAL;
		}
	}

	try {
#ifdef ZT_ENABLE_CLUSTER
		// If clustering is enabled we have to call cluster->doPeriodicTasks() very often, so we override normal timer deadline behavior
		if (RR->cluster) {
			RR->sw->doTimerTasks(tptr,now);
			RR->cluster->doPeriodicTasks();
			*nextBackgroundTaskDeadline = now + ZT_CLUSTER_PERIODIC_TASK_PERIOD; // this is really short so just tick at this rate
		} else {
#endif
			*nextBackgroundTaskDeadline = now + (uint64_t)std::max(std::min(timeUntilNextPingCheck,RR->sw->doTimerTasks(tptr,now)),(unsigned long)ZT_CORE_TIMER_TASK_GRANULARITY);
#ifdef ZT_ENABLE_CLUSTER
		}
#endif
	} catch ( ... ) {
		return ZT_RESULT_FATAL_ERROR_INTERNAL;
	}

	return ZT_RESULT_OK;
}

ZT_ResultCode Node::join(uint64_t nwid,void *uptr,void *tptr)
{
	Mutex::Lock _l(_networks_m);
	SharedPtr<Network> nw = _network(nwid);
	if(!nw) {
		const std::pair< uint64_t,SharedPtr<Network> > nn(nwid,SharedPtr<Network>(new Network(RR,tptr,nwid,uptr)));
		_networks.insert(std::upper_bound(_networks.begin(),_networks.end(),nn),nn);
	}
	return ZT_RESULT_OK;
}

ZT_ResultCode Node::leave(uint64_t nwid,void **uptr,void *tptr)
{
	ZT_VirtualNetworkConfig ctmp;
	std::vector< std::pair< uint64_t,SharedPtr<Network> > > newn;
	void **nUserPtr = (void **)0;
	Mutex::Lock _l(_networks_m);

	for(std::vector< std::pair< uint64_t,SharedPtr<Network> > >::const_iterator n(_networks.begin());n!=_networks.end();++n) {
		if (n->first != nwid) {
			newn.push_back(*n);
		} else {
			if (uptr)
				*uptr = *n->second->userPtr();
			n->second->externalConfig(&ctmp);
			n->second->destroy();
			nUserPtr = n->second->userPtr();
		}
	}
	_networks.swap(newn);
 
	if (nUserPtr)
		RR->node->configureVirtualNetworkPort(tptr,nwid,nUserPtr,ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY,&ctmp);

	return ZT_RESULT_OK;
}

ZT_ResultCode Node::multicastSubscribe(void *tptr,uint64_t nwid,uint64_t multicastGroup,unsigned long multicastAdi)
{
	SharedPtr<Network> nw(this->network(nwid));
	if (nw) {
		nw->multicastSubscribe(tptr,MulticastGroup(MAC(multicastGroup),(uint32_t)(multicastAdi & 0xffffffff)));
		return ZT_RESULT_OK;
	} else return ZT_RESULT_ERROR_NETWORK_NOT_FOUND;
}

ZT_ResultCode Node::multicastUnsubscribe(uint64_t nwid,uint64_t multicastGroup,unsigned long multicastAdi)
{
	SharedPtr<Network> nw(this->network(nwid));
	if (nw) {
		nw->multicastUnsubscribe(MulticastGroup(MAC(multicastGroup),(uint32_t)(multicastAdi & 0xffffffff)));
		return ZT_RESULT_OK;
	} else return ZT_RESULT_ERROR_NETWORK_NOT_FOUND;
}

ZT_ResultCode Node::orbit(void *tptr,uint64_t moonWorldId,uint64_t moonSeed)
{
	RR->topology->addMoon(tptr,moonWorldId,Address(moonSeed));
	return ZT_RESULT_OK;
}

ZT_ResultCode Node::deorbit(void *tptr,uint64_t moonWorldId)
{
	RR->topology->removeMoon(tptr,moonWorldId);
	return ZT_RESULT_OK;
}

uint64_t Node::address() const
{
	return RR->identity.address().toInt();
}

void Node::status(ZT_NodeStatus *status) const
{
	status->address = RR->identity.address().toInt();
	status->publicIdentity = RR->publicIdentityStr.c_str();
	status->secretIdentity = RR->secretIdentityStr.c_str();
	status->online = _online ? 1 : 0;
}

ZT_PeerList *Node::peers() const
{
	std::vector< std::pair< Address,SharedPtr<Peer> > > peers(RR->topology->allPeers());
	std::sort(peers.begin(),peers.end());

	char *buf = (char *)::malloc(sizeof(ZT_PeerList) + (sizeof(ZT_Peer) * peers.size()));
	if (!buf)
		return (ZT_PeerList *)0;
	ZT_PeerList *pl = (ZT_PeerList *)buf;
	pl->peers = (ZT_Peer *)(buf + sizeof(ZT_PeerList));

	pl->peerCount = 0;
	for(std::vector< std::pair< Address,SharedPtr<Peer> > >::iterator pi(peers.begin());pi!=peers.end();++pi) {
		ZT_Peer *p = &(pl->peers[pl->peerCount++]);
		p->address = pi->second->address().toInt();
		if (pi->second->remoteVersionKnown()) {
			p->versionMajor = pi->second->remoteVersionMajor();
			p->versionMinor = pi->second->remoteVersionMinor();
			p->versionRev = pi->second->remoteVersionRevision();
		} else {
			p->versionMajor = -1;
			p->versionMinor = -1;
			p->versionRev = -1;
		}
		p->latency = pi->second->latency();
		p->role = RR->topology->role(pi->second->identity().address());

		std::vector< SharedPtr<Path> > paths(pi->second->paths(_now));
		SharedPtr<Path> bestp(pi->second->getBestPath(_now,false));
		p->pathCount = 0;
		for(std::vector< SharedPtr<Path> >::iterator path(paths.begin());path!=paths.end();++path) {
			memcpy(&(p->paths[p->pathCount].address),&((*path)->address()),sizeof(struct sockaddr_storage));
			p->paths[p->pathCount].lastSend = (*path)->lastOut();
			p->paths[p->pathCount].lastReceive = (*path)->lastIn();
			p->paths[p->pathCount].trustedPathId = RR->topology->getOutboundPathTrust((*path)->address());
			p->paths[p->pathCount].linkQuality = (int)(*path)->linkQuality();
			p->paths[p->pathCount].expired = 0;
			p->paths[p->pathCount].preferred = ((*path) == bestp) ? 1 : 0;
			++p->pathCount;
		}
	}

	return pl;
}

ZT_VirtualNetworkConfig *Node::networkConfig(uint64_t nwid) const
{
	Mutex::Lock _l(_networks_m);
	SharedPtr<Network> nw = _network(nwid);
	if(nw) {
		ZT_VirtualNetworkConfig *nc = (ZT_VirtualNetworkConfig *)::malloc(sizeof(ZT_VirtualNetworkConfig));
		nw->externalConfig(nc);
		return nc;
	}
	return (ZT_VirtualNetworkConfig *)0;
}

ZT_VirtualNetworkList *Node::networks() const
{
	Mutex::Lock _l(_networks_m);

	char *buf = (char *)::malloc(sizeof(ZT_VirtualNetworkList) + (sizeof(ZT_VirtualNetworkConfig) * _networks.size()));
	if (!buf)
		return (ZT_VirtualNetworkList *)0;
	ZT_VirtualNetworkList *nl = (ZT_VirtualNetworkList *)buf;
	nl->networks = (ZT_VirtualNetworkConfig *)(buf + sizeof(ZT_VirtualNetworkList));

	nl->networkCount = 0;
	for(std::vector< std::pair< uint64_t,SharedPtr<Network> > >::const_iterator n(_networks.begin());n!=_networks.end();++n)
		n->second->externalConfig(&(nl->networks[nl->networkCount++]));

	return nl;
}

void Node::freeQueryResult(void *qr)
{
	if (qr)
		::free(qr);
}

int Node::addLocalInterfaceAddress(const struct sockaddr_storage *addr)
{
	if (Path::isAddressValidForPath(*(reinterpret_cast<const InetAddress *>(addr)))) {
		Mutex::Lock _l(_directPaths_m);
		if (std::find(_directPaths.begin(),_directPaths.end(),*(reinterpret_cast<const InetAddress *>(addr))) == _directPaths.end()) {
			_directPaths.push_back(*(reinterpret_cast<const InetAddress *>(addr)));
			return 1;
		}
	}
	return 0;
}

void Node::clearLocalInterfaceAddresses()
{
	Mutex::Lock _l(_directPaths_m);
	_directPaths.clear();
}

int Node::sendUserMessage(void *tptr,uint64_t dest,uint64_t typeId,const void *data,unsigned int len)
{
	try {
		if (RR->identity.address().toInt() != dest) {
			Packet outp(Address(dest),RR->identity.address(),Packet::VERB_USER_MESSAGE);
			outp.append(typeId);
			outp.append(data,len);
			outp.compress();
			RR->sw->send(tptr,outp,true);
			return 1;
		}
	} catch ( ... ) {}
	return 0;
}

void Node::setNetconfMaster(void *networkControllerInstance)
{
	RR->localNetworkController = reinterpret_cast<NetworkController *>(networkControllerInstance);
	if (networkControllerInstance)
		RR->localNetworkController->init(RR->identity,this);
}

ZT_ResultCode Node::clusterInit(
	unsigned int myId,
	const struct sockaddr_storage *zeroTierPhysicalEndpoints,
	unsigned int numZeroTierPhysicalEndpoints,
	int x,
	int y,
	int z,
	void (*sendFunction)(void *,unsigned int,const void *,unsigned int),
	void *sendFunctionArg,
	int (*addressToLocationFunction)(void *,const struct sockaddr_storage *,int *,int *,int *),
	void *addressToLocationFunctionArg)
{
#ifdef ZT_ENABLE_CLUSTER
	if (RR->cluster)
		return ZT_RESULT_ERROR_BAD_PARAMETER;

	std::vector<InetAddress> eps;
	for(unsigned int i=0;i<numZeroTierPhysicalEndpoints;++i)
		eps.push_back(InetAddress(zeroTierPhysicalEndpoints[i]));
	std::sort(eps.begin(),eps.end());
	RR->cluster = new Cluster(RR,myId,eps,x,y,z,sendFunction,sendFunctionArg,addressToLocationFunction,addressToLocationFunctionArg);

	return ZT_RESULT_OK;
#else
	return ZT_RESULT_ERROR_UNSUPPORTED_OPERATION;
#endif
}

ZT_ResultCode Node::clusterAddMember(unsigned int memberId)
{
#ifdef ZT_ENABLE_CLUSTER
	if (!RR->cluster)
		return ZT_RESULT_ERROR_BAD_PARAMETER;
	RR->cluster->addMember((uint16_t)memberId);
	return ZT_RESULT_OK;
#else
	return ZT_RESULT_ERROR_UNSUPPORTED_OPERATION;
#endif
}

void Node::clusterRemoveMember(unsigned int memberId)
{
#ifdef ZT_ENABLE_CLUSTER
	if (RR->cluster)
		RR->cluster->removeMember((uint16_t)memberId);
#endif
}

void Node::clusterHandleIncomingMessage(const void *msg,unsigned int len)
{
#ifdef ZT_ENABLE_CLUSTER
	if (RR->cluster)
		RR->cluster->handleIncomingStateMessage(msg,len);
#endif
}

void Node::clusterStatus(ZT_ClusterStatus *cs)
{
	if (!cs)
		return;
#ifdef ZT_ENABLE_CLUSTER
	if (RR->cluster)
		RR->cluster->status(*cs);
	else
#endif
	memset(cs,0,sizeof(ZT_ClusterStatus));
}

/****************************************************************************/
/* Node methods used only within node/                                      */
/****************************************************************************/

std::string Node::dataStoreGet(void *tPtr,const char *name)
{
	char buf[1024];
	std::string r;
	unsigned long olen = 0;
	do {
		long n = _cb.dataStoreGetFunction(reinterpret_cast<ZT_Node *>(this),_uPtr,tPtr,name,buf,sizeof(buf),(unsigned long)r.length(),&olen);
		if (n <= 0)
			return std::string();
		r.append(buf,n);
	} while (r.length() < olen);
	return r;
}

bool Node::shouldUsePathForZeroTierTraffic(void *tPtr,const Address &ztaddr,const InetAddress &localAddress,const InetAddress &remoteAddress)
{
	if (!Path::isAddressValidForPath(remoteAddress))
		return false;

	if (RR->topology->isProhibitedEndpoint(ztaddr,remoteAddress))
		return false;

	{
		Mutex::Lock _l(_networks_m);
		for(std::vector< std::pair< uint64_t, SharedPtr<Network> > >::const_iterator i=_networks.begin();i!=_networks.end();++i) {
			if (i->second->hasConfig()) {
				for(unsigned int k=0;k<i->second->config().staticIpCount;++k) {
					if (i->second->config().staticIps[k].containsAddress(remoteAddress))
						return false;
				}
			}
		}
	}

	return ( (_cb.pathCheckFunction) ? (_cb.pathCheckFunction(reinterpret_cast<ZT_Node *>(this),_uPtr,tPtr,ztaddr.toInt(),reinterpret_cast<const struct sockaddr_storage *>(&localAddress),reinterpret_cast<const struct sockaddr_storage *>(&remoteAddress)) != 0) : true);
}

#ifdef ZT_TRACE
void Node::postTrace(const char *module,unsigned int line,const char *fmt,...)
{
	static Mutex traceLock;

	va_list ap;
	char tmp1[1024],tmp2[1024],tmp3[256];

	Mutex::Lock _l(traceLock);

	time_t now = (time_t)(_now / 1000ULL);
#ifdef __WINDOWS__
	ctime_s(tmp3,sizeof(tmp3),&now);
	char *nowstr = tmp3;
#else
	char *nowstr = ctime_r(&now,tmp3);
#endif
	unsigned long nowstrlen = (unsigned long)strlen(nowstr);
	if (nowstr[nowstrlen-1] == '\n')
		nowstr[--nowstrlen] = (char)0;
	if (nowstr[nowstrlen-1] == '\r')
		nowstr[--nowstrlen] = (char)0;

	va_start(ap,fmt);
	vsnprintf(tmp2,sizeof(tmp2),fmt,ap);
	va_end(ap);
	tmp2[sizeof(tmp2)-1] = (char)0;

	Utils::snprintf(tmp1,sizeof(tmp1),"[%s] %s:%u %s",nowstr,module,line,tmp2);
	postEvent((void *)0,ZT_EVENT_TRACE,tmp1);
}
#endif // ZT_TRACE

uint64_t Node::prng()
{
	// https://en.wikipedia.org/wiki/Xorshift#xorshift.2B
	uint64_t x = _prngState[0];
	const uint64_t y = _prngState[1];
	_prngState[0] = y;
	x ^= x << 23;
	const uint64_t z = x ^ y ^ (x >> 17) ^ (y >> 26);
	_prngState[1] = z;
	return z + y;
}

void Node::setTrustedPaths(const struct sockaddr_storage *networks,const uint64_t *ids,unsigned int count)
{
	RR->topology->setTrustedPaths(reinterpret_cast<const InetAddress *>(networks),ids,count);
}

World Node::planet() const
{
	return RR->topology->planet();
}

std::vector<World> Node::moons() const
{
	return RR->topology->moons();
}

void Node::ncSendConfig(uint64_t nwid,uint64_t requestPacketId,const Address &destination,const NetworkConfig &nc,bool sendLegacyFormatConfig)
{
	if (destination == RR->identity.address()) {
		SharedPtr<Network> n(network(nwid));
		if (!n) return;
		n->setConfiguration((void *)0,nc,true);
	} else {
		Dictionary<ZT_NETWORKCONFIG_DICT_CAPACITY> *dconf = new Dictionary<ZT_NETWORKCONFIG_DICT_CAPACITY>();
		try {
			if (nc.toDictionary(*dconf,sendLegacyFormatConfig)) {
				uint64_t configUpdateId = prng();
				if (!configUpdateId) ++configUpdateId;

				const unsigned int totalSize = dconf->sizeBytes();
				unsigned int chunkIndex = 0;
				while (chunkIndex < totalSize) {
					const unsigned int chunkLen = std::min(totalSize - chunkIndex,(unsigned int)(ZT_UDP_DEFAULT_PAYLOAD_MTU - (ZT_PACKET_IDX_PAYLOAD + 256)));
					Packet outp(destination,RR->identity.address(),(requestPacketId) ? Packet::VERB_OK : Packet::VERB_NETWORK_CONFIG);
					if (requestPacketId) {
						outp.append((unsigned char)Packet::VERB_NETWORK_CONFIG_REQUEST);
						outp.append(requestPacketId);
					}

					const unsigned int sigStart = outp.size();
					outp.append(nwid);
					outp.append((uint16_t)chunkLen);
					outp.append((const void *)(dconf->data() + chunkIndex),chunkLen);

					outp.append((uint8_t)0); // no flags
					outp.append((uint64_t)configUpdateId);
					outp.append((uint32_t)totalSize);
					outp.append((uint32_t)chunkIndex);

					C25519::Signature sig(RR->identity.sign(reinterpret_cast<const uint8_t *>(outp.data()) + sigStart,outp.size() - sigStart));
					outp.append((uint8_t)1);
					outp.append((uint16_t)ZT_C25519_SIGNATURE_LEN);
					outp.append(sig.data,ZT_C25519_SIGNATURE_LEN);

					outp.compress();
					RR->sw->send((void *)0,outp,true);
					chunkIndex += chunkLen;
				}
			}
			delete dconf;
		} catch ( ... ) {
			delete dconf;
			throw;
		}
	}
}

void Node::ncSendRevocation(const Address &destination,const Revocation &rev)
{
	if (destination == RR->identity.address()) {
		SharedPtr<Network> n(network(rev.networkId()));
		if (!n) return;
		n->addCredential((void *)0,RR->identity.address(),rev);
	} else {
		Packet outp(destination,RR->identity.address(),Packet::VERB_NETWORK_CREDENTIALS);
		outp.append((uint8_t)0x00);
		outp.append((uint16_t)0);
		outp.append((uint16_t)0);
		outp.append((uint16_t)1);
		rev.serialize(outp);
		outp.append((uint16_t)0);
		RR->sw->send((void *)0,outp,true);
	}
}

void Node::ncSendError(uint64_t nwid,uint64_t requestPacketId,const Address &destination,NetworkController::ErrorCode errorCode)
{
	if (destination == RR->identity.address()) {
		SharedPtr<Network> n(network(nwid));
		if (!n) return;
		switch(errorCode) {
			case NetworkController::NC_ERROR_OBJECT_NOT_FOUND:
			case NetworkController::NC_ERROR_INTERNAL_SERVER_ERROR:
				n->setNotFound();
				break;
			case NetworkController::NC_ERROR_ACCESS_DENIED:
				n->setAccessDenied();
				break;

			default: break;
		}
	} else if (requestPacketId) {
		Packet outp(destination,RR->identity.address(),Packet::VERB_ERROR);
		outp.append((unsigned char)Packet::VERB_NETWORK_CONFIG_REQUEST);
		outp.append(requestPacketId);
		switch(errorCode) {
			//case NetworkController::NC_ERROR_OBJECT_NOT_FOUND:
			//case NetworkController::NC_ERROR_INTERNAL_SERVER_ERROR:
			default:
				outp.append((unsigned char)Packet::ERROR_OBJ_NOT_FOUND);
				break;
			case NetworkController::NC_ERROR_ACCESS_DENIED:
				outp.append((unsigned char)Packet::ERROR_NETWORK_ACCESS_DENIED_);
				break;
		}
		outp.append(nwid);
		RR->sw->send((void *)0,outp,true);
	} // else we can't send an ERROR() in response to nothing, so discard
}

} // namespace ZeroTier

/****************************************************************************/
/* CAPI bindings                                                            */
/****************************************************************************/

extern "C" {

enum ZT_ResultCode ZT_Node_new(ZT_Node **node,void *uptr,void *tptr,const struct ZT_Node_Callbacks *callbacks,uint64_t now)
{
	*node = (ZT_Node *)0;
	try {
		*node = reinterpret_cast<ZT_Node *>(new ZeroTier::Node(uptr,tptr,callbacks,now));
		return ZT_RESULT_OK;
	} catch (std::bad_alloc &exc) {
		return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
	} catch (std::runtime_error &exc) {
		return ZT_RESULT_FATAL_ERROR_DATA_STORE_FAILED;
	} catch ( ... ) {
		return ZT_RESULT_FATAL_ERROR_INTERNAL;
	}
}

void ZT_Node_delete(ZT_Node *node)
{
	try {
		delete (reinterpret_cast<ZeroTier::Node *>(node));
	} catch ( ... ) {}
}

enum ZT_ResultCode ZT_Node_processWirePacket(
	ZT_Node *node,
	void *tptr,
	uint64_t now,
	const struct sockaddr_storage *localAddress,
	const struct sockaddr_storage *remoteAddress,
	const void *packetData,
	unsigned int packetLength,
	volatile uint64_t *nextBackgroundTaskDeadline)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->processWirePacket(tptr,now,localAddress,remoteAddress,packetData,packetLength,nextBackgroundTaskDeadline);
	} catch (std::bad_alloc &exc) {
		return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
	} catch ( ... ) {
		return ZT_RESULT_OK; // "OK" since invalid packets are simply dropped, but the system is still up
	}
}

enum ZT_ResultCode ZT_Node_processVirtualNetworkFrame(
	ZT_Node *node,
	void *tptr,
	uint64_t now,
	uint64_t nwid,
	uint64_t sourceMac,
	uint64_t destMac,
	unsigned int etherType,
	unsigned int vlanId,
	const void *frameData,
	unsigned int frameLength,
	volatile uint64_t *nextBackgroundTaskDeadline)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->processVirtualNetworkFrame(tptr,now,nwid,sourceMac,destMac,etherType,vlanId,frameData,frameLength,nextBackgroundTaskDeadline);
	} catch (std::bad_alloc &exc) {
		return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
	} catch ( ... ) {
		return ZT_RESULT_FATAL_ERROR_INTERNAL;
	}
}

enum ZT_ResultCode ZT_Node_processBackgroundTasks(ZT_Node *node,void *tptr,uint64_t now,volatile uint64_t *nextBackgroundTaskDeadline)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->processBackgroundTasks(tptr,now,nextBackgroundTaskDeadline);
	} catch (std::bad_alloc &exc) {
		return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
	} catch ( ... ) {
		return ZT_RESULT_FATAL_ERROR_INTERNAL;
	}
}

enum ZT_ResultCode ZT_Node_join(ZT_Node *node,uint64_t nwid,void *uptr,void *tptr)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->join(nwid,uptr,tptr);
	} catch (std::bad_alloc &exc) {
		return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
	} catch ( ... ) {
		return ZT_RESULT_FATAL_ERROR_INTERNAL;
	}
}

enum ZT_ResultCode ZT_Node_leave(ZT_Node *node,uint64_t nwid,void **uptr,void *tptr)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->leave(nwid,uptr,tptr);
	} catch (std::bad_alloc &exc) {
		return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
	} catch ( ... ) {
		return ZT_RESULT_FATAL_ERROR_INTERNAL;
	}
}

enum ZT_ResultCode ZT_Node_multicastSubscribe(ZT_Node *node,void *tptr,uint64_t nwid,uint64_t multicastGroup,unsigned long multicastAdi)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->multicastSubscribe(tptr,nwid,multicastGroup,multicastAdi);
	} catch (std::bad_alloc &exc) {
		return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
	} catch ( ... ) {
		return ZT_RESULT_FATAL_ERROR_INTERNAL;
	}
}

enum ZT_ResultCode ZT_Node_multicastUnsubscribe(ZT_Node *node,uint64_t nwid,uint64_t multicastGroup,unsigned long multicastAdi)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->multicastUnsubscribe(nwid,multicastGroup,multicastAdi);
	} catch (std::bad_alloc &exc) {
		return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
	} catch ( ... ) {
		return ZT_RESULT_FATAL_ERROR_INTERNAL;
	}
}

enum ZT_ResultCode ZT_Node_orbit(ZT_Node *node,void *tptr,uint64_t moonWorldId,uint64_t moonSeed)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->orbit(tptr,moonWorldId,moonSeed);
	} catch ( ... ) {
		return ZT_RESULT_FATAL_ERROR_INTERNAL;
	}
}

ZT_ResultCode ZT_Node_deorbit(ZT_Node *node,void *tptr,uint64_t moonWorldId)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->deorbit(tptr,moonWorldId);
	} catch ( ... ) {
		return ZT_RESULT_FATAL_ERROR_INTERNAL;
	}
}

uint64_t ZT_Node_address(ZT_Node *node)
{
	return reinterpret_cast<ZeroTier::Node *>(node)->address();
}

void ZT_Node_status(ZT_Node *node,ZT_NodeStatus *status)
{
	try {
		reinterpret_cast<ZeroTier::Node *>(node)->status(status);
	} catch ( ... ) {}
}

ZT_PeerList *ZT_Node_peers(ZT_Node *node)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->peers();
	} catch ( ... ) {
		return (ZT_PeerList *)0;
	}
}

ZT_VirtualNetworkConfig *ZT_Node_networkConfig(ZT_Node *node,uint64_t nwid)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->networkConfig(nwid);
	} catch ( ... ) {
		return (ZT_VirtualNetworkConfig *)0;
	}
}

ZT_VirtualNetworkList *ZT_Node_networks(ZT_Node *node)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->networks();
	} catch ( ... ) {
		return (ZT_VirtualNetworkList *)0;
	}
}

void ZT_Node_freeQueryResult(ZT_Node *node,void *qr)
{
	try {
		reinterpret_cast<ZeroTier::Node *>(node)->freeQueryResult(qr);
	} catch ( ... ) {}
}

int ZT_Node_addLocalInterfaceAddress(ZT_Node *node,const struct sockaddr_storage *addr)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->addLocalInterfaceAddress(addr);
	} catch ( ... ) {
		return 0;
	}
}

void ZT_Node_clearLocalInterfaceAddresses(ZT_Node *node)
{
	try {
		reinterpret_cast<ZeroTier::Node *>(node)->clearLocalInterfaceAddresses();
	} catch ( ... ) {}
}

int ZT_Node_sendUserMessage(ZT_Node *node,void *tptr,uint64_t dest,uint64_t typeId,const void *data,unsigned int len)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->sendUserMessage(tptr,dest,typeId,data,len);
	} catch ( ... ) {
		return 0;
	}
}

void ZT_Node_setNetconfMaster(ZT_Node *node,void *networkControllerInstance)
{
	try {
		reinterpret_cast<ZeroTier::Node *>(node)->setNetconfMaster(networkControllerInstance);
	} catch ( ... ) {}
}

enum ZT_ResultCode ZT_Node_clusterInit(
	ZT_Node *node,
	unsigned int myId,
	const struct sockaddr_storage *zeroTierPhysicalEndpoints,
	unsigned int numZeroTierPhysicalEndpoints,
	int x,
	int y,
	int z,
	void (*sendFunction)(void *,unsigned int,const void *,unsigned int),
	void *sendFunctionArg,
	int (*addressToLocationFunction)(void *,const struct sockaddr_storage *,int *,int *,int *),
	void *addressToLocationFunctionArg)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->clusterInit(myId,zeroTierPhysicalEndpoints,numZeroTierPhysicalEndpoints,x,y,z,sendFunction,sendFunctionArg,addressToLocationFunction,addressToLocationFunctionArg);
	} catch ( ... ) {
		return ZT_RESULT_FATAL_ERROR_INTERNAL;
	}
}

enum ZT_ResultCode ZT_Node_clusterAddMember(ZT_Node *node,unsigned int memberId)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->clusterAddMember(memberId);
	} catch ( ... ) {
		return ZT_RESULT_FATAL_ERROR_INTERNAL;
	}
}

void ZT_Node_clusterRemoveMember(ZT_Node *node,unsigned int memberId)
{
	try {
		reinterpret_cast<ZeroTier::Node *>(node)->clusterRemoveMember(memberId);
	} catch ( ... ) {}
}

void ZT_Node_clusterHandleIncomingMessage(ZT_Node *node,const void *msg,unsigned int len)
{
	try {
		reinterpret_cast<ZeroTier::Node *>(node)->clusterHandleIncomingMessage(msg,len);
	} catch ( ... ) {}
}

void ZT_Node_clusterStatus(ZT_Node *node,ZT_ClusterStatus *cs)
{
	try {
		reinterpret_cast<ZeroTier::Node *>(node)->clusterStatus(cs);
	} catch ( ... ) {}
}

void ZT_Node_setTrustedPaths(ZT_Node *node,const struct sockaddr_storage *networks,const uint64_t *ids,unsigned int count)
{
	try {
		reinterpret_cast<ZeroTier::Node *>(node)->setTrustedPaths(networks,ids,count);
	} catch ( ... ) {}
}

void ZT_version(int *major,int *minor,int *revision)
{
	if (major) *major = ZEROTIER_ONE_VERSION_MAJOR;
	if (minor) *minor = ZEROTIER_ONE_VERSION_MINOR;
	if (revision) *revision = ZEROTIER_ONE_VERSION_REVISION;
}

} // extern "C"
