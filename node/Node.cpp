/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

#include "../version.h"

#include "Constants.hpp"
#include "SharedPtr.hpp"
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
#include "Network.hpp"
#include "Trace.hpp"

namespace ZeroTier {

/****************************************************************************/
/* Public Node interface (C++, exposed via CAPI bindings)                   */
/****************************************************************************/

Node::Node(void *uptr,void *tptr,const struct ZT_Node_Callbacks *callbacks,int64_t now) :
	_RR(this),
	RR(&_RR),
	_uPtr(uptr),
	_networks(8),
	_now(now),
	_lastPingCheck(0),
	_lastGratuitousPingCheck(0),
	_lastHousekeepingRun(0),
	_lastMemoizedTraceSettings(0)
{
	if (callbacks->version != 0)
		throw ZT_EXCEPTION_INVALID_ARGUMENT;
	memcpy(&_cb,callbacks,sizeof(ZT_Node_Callbacks));

	// Initialize non-cryptographic PRNG from a good random source
	Utils::getSecureRandom((void *)_prngState,sizeof(_prngState));

	_online = false;

	memset(_expectingRepliesToBucketPtr,0,sizeof(_expectingRepliesToBucketPtr));
	memset(_expectingRepliesTo,0,sizeof(_expectingRepliesTo));
	memset(_lastIdentityVerification,0,sizeof(_lastIdentityVerification));
	memset((void *)(&_stats),0,sizeof(_stats));

	uint64_t idtmp[2];
	idtmp[0] = 0; idtmp[1] = 0;
	char tmp[2048];
	int n = stateObjectGet(tptr,ZT_STATE_OBJECT_IDENTITY_SECRET,idtmp,tmp,sizeof(tmp) - 1);
	if (n > 0) {
		tmp[n] = (char)0;
		if (RR->identity.fromString(tmp)) {
			RR->identity.toString(false,RR->publicIdentityStr);
			RR->identity.toString(true,RR->secretIdentityStr);
		} else {
			n = -1;
		}
	}

	if (n <= 0) {
		RR->identity.generate();
		RR->identity.toString(false,RR->publicIdentityStr);
		RR->identity.toString(true,RR->secretIdentityStr);
		idtmp[0] = RR->identity.address().toInt(); idtmp[1] = 0;
		stateObjectPut(tptr,ZT_STATE_OBJECT_IDENTITY_SECRET,idtmp,RR->secretIdentityStr,(unsigned int)strlen(RR->secretIdentityStr));
		stateObjectPut(tptr,ZT_STATE_OBJECT_IDENTITY_PUBLIC,idtmp,RR->publicIdentityStr,(unsigned int)strlen(RR->publicIdentityStr));
	} else {
		idtmp[0] = RR->identity.address().toInt(); idtmp[1] = 0;
		n = stateObjectGet(tptr,ZT_STATE_OBJECT_IDENTITY_PUBLIC,idtmp,tmp,sizeof(tmp) - 1);
		if ((n > 0)&&(n < (int)sizeof(RR->publicIdentityStr))&&(n < (int)sizeof(tmp))) {
			if (memcmp(tmp,RR->publicIdentityStr,n))
				stateObjectPut(tptr,ZT_STATE_OBJECT_IDENTITY_PUBLIC,idtmp,RR->publicIdentityStr,(unsigned int)strlen(RR->publicIdentityStr));
		}
	}

	char *m = (char *)0;
	try {
		const unsigned long ts = sizeof(Trace) + (((sizeof(Trace) & 0xf) != 0) ? (16 - (sizeof(Trace) & 0xf)) : 0);
		const unsigned long sws = sizeof(Switch) + (((sizeof(Switch) & 0xf) != 0) ? (16 - (sizeof(Switch) & 0xf)) : 0);
		const unsigned long mcs = sizeof(Multicaster) + (((sizeof(Multicaster) & 0xf) != 0) ? (16 - (sizeof(Multicaster) & 0xf)) : 0);
		const unsigned long topologys = sizeof(Topology) + (((sizeof(Topology) & 0xf) != 0) ? (16 - (sizeof(Topology) & 0xf)) : 0);
		const unsigned long sas = sizeof(SelfAwareness) + (((sizeof(SelfAwareness) & 0xf) != 0) ? (16 - (sizeof(SelfAwareness) & 0xf)) : 0);
		const unsigned long bc = sizeof(Bond) + (((sizeof(Bond) & 0xf) != 0) ? (16 - (sizeof(Bond) & 0xf)) : 0);

		m = reinterpret_cast<char *>(::malloc(16 + ts + sws + mcs + topologys + sas + bc));
		if (!m)
			throw std::bad_alloc();
		RR->rtmem = m;
		while (((uintptr_t)m & 0xf) != 0) ++m;

		RR->t = new (m) Trace(RR);
		m += ts;
		RR->sw = new (m) Switch(RR);
		m += sws;
		RR->mc = new (m) Multicaster(RR);
		m += mcs;
		RR->topology = new (m) Topology(RR,tptr);
		m += topologys;
		RR->sa = new (m) SelfAwareness(RR);
		m += sas;
		RR->bc = new (m) Bond(RR);
	} catch ( ... ) {
		if (RR->sa) RR->sa->~SelfAwareness();
		if (RR->topology) RR->topology->~Topology();
		if (RR->mc) RR->mc->~Multicaster();
		if (RR->sw) RR->sw->~Switch();
		if (RR->t) RR->t->~Trace();
		if (RR->bc) RR->bc->~Bond();
		::free(m);
		throw;
	}

	postEvent(tptr,ZT_EVENT_UP);
}

Node::~Node()
{
	{
		Mutex::Lock _l(_networks_m);
		_networks.clear(); // destroy all networks before shutdown
	}
	if (RR->sa) RR->sa->~SelfAwareness();
	if (RR->topology) RR->topology->~Topology();
	if (RR->mc) RR->mc->~Multicaster();
	if (RR->sw) RR->sw->~Switch();
	if (RR->t) RR->t->~Trace();
	if (RR->bc) RR->bc->~Bond();
	::free(RR->rtmem);
}

ZT_ResultCode Node::processWirePacket(
	void *tptr,
	int64_t now,
	int64_t localSocket,
	const struct sockaddr_storage *remoteAddress,
	const void *packetData,
	unsigned int packetLength,
	volatile int64_t *nextBackgroundTaskDeadline)
{
	_now = now;
	RR->sw->onRemotePacket(tptr,localSocket,*(reinterpret_cast<const InetAddress *>(remoteAddress)),packetData,packetLength);
	return ZT_RESULT_OK;
}

ZT_ResultCode Node::processVirtualNetworkFrame(
	void *tptr,
	int64_t now,
	uint64_t nwid,
	uint64_t sourceMac,
	uint64_t destMac,
	unsigned int etherType,
	unsigned int vlanId,
	const void *frameData,
	unsigned int frameLength,
	volatile int64_t *nextBackgroundTaskDeadline)
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
	_PingPeersThatNeedPing(const RuntimeEnvironment *renv,void *tPtr,Hashtable< Address,std::vector<InetAddress> > &alwaysContact,int64_t now) :
		RR(renv),
		_tPtr(tPtr),
		_alwaysContact(alwaysContact),
		_now(now),
		_bestCurrentUpstream(RR->topology->getUpstreamPeer())
	{
	}

	inline void operator()(Topology &t,const SharedPtr<Peer> &p)
	{
		const std::vector<InetAddress> *const alwaysContactEndpoints = _alwaysContact.get(p->address());
		if (alwaysContactEndpoints) {
			const unsigned int sent = p->doPingAndKeepalive(_tPtr,_now);
			bool contacted = (sent != 0);

			if ((sent & 0x1) == 0) { // bit 0x1 == IPv4 sent
				for(unsigned long k=0,ptr=(unsigned long)RR->node->prng();k<(unsigned long)alwaysContactEndpoints->size();++k) {
					const InetAddress &addr = (*alwaysContactEndpoints)[ptr++ % alwaysContactEndpoints->size()];
					if (addr.ss_family == AF_INET) {
						p->sendHELLO(_tPtr,-1,addr,_now);
						contacted = true;
						break;
					}
				}
			}

			if ((sent & 0x2) == 0) { // bit 0x2 == IPv6 sent
				for(unsigned long k=0,ptr=(unsigned long)RR->node->prng();k<(unsigned long)alwaysContactEndpoints->size();++k) {
					const InetAddress &addr = (*alwaysContactEndpoints)[ptr++ % alwaysContactEndpoints->size()];
					if (addr.ss_family == AF_INET6) {
						p->sendHELLO(_tPtr,-1,addr,_now);
						contacted = true;
						break;
					}
				}
			}

			if ((!contacted)&&(_bestCurrentUpstream)) {
				const SharedPtr<Path> up(_bestCurrentUpstream->getAppropriatePath(_now,true));
				if (up)
					p->sendHELLO(_tPtr,up->localSocket(),up->address(),_now);
			}

			_alwaysContact.erase(p->address()); // after this we'll WHOIS all upstreams that remain
		} else if (p->isActive(_now)) {
			p->doPingAndKeepalive(_tPtr,_now);
		}
	}

private:
	const RuntimeEnvironment *RR;
	void *_tPtr;
	Hashtable< Address,std::vector<InetAddress> > &_alwaysContact;
	const int64_t _now;
	const SharedPtr<Peer> _bestCurrentUpstream;
};

ZT_ResultCode Node::processBackgroundTasks(void *tptr,int64_t now,volatile int64_t *nextBackgroundTaskDeadline)
{
	_now = now;
	Mutex::Lock bl(_backgroundTasksLock);

	// Process background bond tasks
	unsigned long bondCheckInterval = ZT_PING_CHECK_INVERVAL;
	if (RR->bc->inUse()) {
		bondCheckInterval = std::max(RR->bc->minReqMonitorInterval(), ZT_CORE_TIMER_TASK_GRANULARITY);
		if ((now - _lastGratuitousPingCheck) >= ZT_CORE_TIMER_TASK_GRANULARITY) {
			_lastGratuitousPingCheck = now;
			RR->bc->processBackgroundTasks(tptr, now);
		}
	}

	unsigned long timeUntilNextPingCheck = ZT_PING_CHECK_INVERVAL;
	const int64_t timeSinceLastPingCheck = now - _lastPingCheck;
	if (timeSinceLastPingCheck >= timeUntilNextPingCheck) {
		try {
			_lastPingCheck = now;

			// Get designated VL1 upstreams
			Hashtable< Address,std::vector<InetAddress> > alwaysContact;
			RR->topology->getUpstreamsToContact(alwaysContact);

			// Uncomment to dump stats
			/*
			for(unsigned int i=0;i<32;i++) {
				if (_stats.inVerbCounts[i] > 0)
					printf("%.2x\t%12lld %lld\n",i,(unsigned long long)_stats.inVerbCounts[i],(unsigned long long)_stats.inVerbBytes[i]);
			}
			printf("\n");
			*/

			// Check last receive time on designated upstreams to see if we seem to be online
			int64_t lastReceivedFromUpstream = 0;
			{
				Hashtable< Address,std::vector<InetAddress> >::Iterator i(alwaysContact);
				Address *upstreamAddress = (Address *)0;
				std::vector<InetAddress> *upstreamStableEndpoints = (std::vector<InetAddress> *)0;
				while (i.next(upstreamAddress,upstreamStableEndpoints)) {
					SharedPtr<Peer> p(RR->topology->getPeerNoCache(*upstreamAddress));
					if (p)
						lastReceivedFromUpstream = std::max(p->lastReceive(),lastReceivedFromUpstream);
				}
			}

			// Clean up any old local controller auth memorizations.
			{
				_localControllerAuthorizations_m.lock();
				Hashtable< _LocalControllerAuth,int64_t >::Iterator i(_localControllerAuthorizations);
				_LocalControllerAuth *k = (_LocalControllerAuth *)0;
				int64_t *v = (int64_t *)0;
				while (i.next(k,v)) {
					if ((*v - now) > (ZT_NETWORK_AUTOCONF_DELAY * 3))
						_localControllerAuthorizations.erase(*k);
				}
				_localControllerAuthorizations_m.unlock();
			}

			// Get peers we should stay connected to according to network configs
			// Also get networks and whether they need config so we only have to do one pass over networks
			std::vector< std::pair< SharedPtr<Network>,bool > > networkConfigNeeded;
			{
				Mutex::Lock l(_networks_m);
				Hashtable< uint64_t,SharedPtr<Network> >::Iterator i(_networks);
				uint64_t *nwid = (uint64_t *)0;
				SharedPtr<Network> *network = (SharedPtr<Network> *)0;
				while (i.next(nwid,network)) {
					(*network)->config().alwaysContactAddresses(alwaysContact);
					networkConfigNeeded.push_back( std::pair< SharedPtr<Network>,bool >(*network,(((now - (*network)->lastConfigUpdate()) >= ZT_NETWORK_AUTOCONF_DELAY)||(!(*network)->hasConfig()))) );
				}
			}

			// Ping active peers, upstreams, and others that we should always contact
			_PingPeersThatNeedPing pfunc(RR,tptr,alwaysContact,now);
			RR->topology->eachPeer<_PingPeersThatNeedPing &>(pfunc);

			// Run WHOIS to create Peer for alwaysContact addresses that could not be contacted
			{
				Hashtable< Address,std::vector<InetAddress> >::Iterator i(alwaysContact);
				Address *upstreamAddress = (Address *)0;
				std::vector<InetAddress> *upstreamStableEndpoints = (std::vector<InetAddress> *)0;
				while (i.next(upstreamAddress,upstreamStableEndpoints))
					RR->sw->requestWhois(tptr,now,*upstreamAddress);
			}

			// Refresh network config or broadcast network updates to members as needed
			for(std::vector< std::pair< SharedPtr<Network>,bool > >::const_iterator n(networkConfigNeeded.begin());n!=networkConfigNeeded.end();++n) {
				if (n->second)
					n->first->requestConfiguration(tptr);
				n->first->sendUpdatesToMembers(tptr);
			}

			// Update online status, post status change as event
			const bool oldOnline = _online;
			_online = (((now - lastReceivedFromUpstream) < ZT_PEER_ACTIVITY_TIMEOUT)||(RR->topology->amUpstream()));
			if (oldOnline != _online)
				postEvent(tptr,_online ? ZT_EVENT_ONLINE : ZT_EVENT_OFFLINE);
		} catch ( ... ) {
			return ZT_RESULT_FATAL_ERROR_INTERNAL;
		}
	} else {
		timeUntilNextPingCheck -= (unsigned long)timeSinceLastPingCheck;
	}

	if ((now - _lastMemoizedTraceSettings) >= (ZT_HOUSEKEEPING_PERIOD / 4)) {
		_lastMemoizedTraceSettings = now;
		RR->t->updateMemoizedSettings();
	}

	if ((now - _lastHousekeepingRun) >= ZT_HOUSEKEEPING_PERIOD) {
		_lastHousekeepingRun = now;
		try {
			RR->topology->doPeriodicTasks(tptr,now);
			RR->sa->clean(now);
			RR->mc->clean(now);
		} catch ( ... ) {
			return ZT_RESULT_FATAL_ERROR_INTERNAL;
		}
	}

	try {
		*nextBackgroundTaskDeadline = now + (int64_t)std::max(std::min(bondCheckInterval,std::min(timeUntilNextPingCheck,RR->sw->doTimerTasks(tptr,now))),(unsigned long)ZT_CORE_TIMER_TASK_GRANULARITY);
	} catch ( ... ) {
		return ZT_RESULT_FATAL_ERROR_INTERNAL;
	}

	return ZT_RESULT_OK;
}

ZT_ResultCode Node::join(uint64_t nwid,void *uptr,void *tptr)
{
	Mutex::Lock _l(_networks_m);
	SharedPtr<Network> &nw = _networks[nwid];
	if (!nw)
		nw = SharedPtr<Network>(new Network(RR,tptr,nwid,uptr,(const NetworkConfig *)0));
	return ZT_RESULT_OK;
}

ZT_ResultCode Node::leave(uint64_t nwid,void **uptr,void *tptr)
{
	ZT_VirtualNetworkConfig ctmp;
	void **nUserPtr = (void **)0;
	{
		Mutex::Lock _l(_networks_m);
		SharedPtr<Network> *nw = _networks.get(nwid);
		RR->sw->removeNetworkQoSControlBlock(nwid);
		if (!nw)
			return ZT_RESULT_OK;
		if (uptr)
			*uptr = (*nw)->userPtr();
		(*nw)->externalConfig(&ctmp);
		(*nw)->destroy();
		nUserPtr = (*nw)->userPtr();
	}

	if (nUserPtr)
		RR->node->configureVirtualNetworkPort(tptr,nwid,nUserPtr,ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY,&ctmp);

	{
		Mutex::Lock _l(_networks_m);
		_networks.erase(nwid);
	}

	uint64_t tmp[2];
	tmp[0] = nwid; tmp[1] = 0;
	RR->node->stateObjectDelete(tptr,ZT_STATE_OBJECT_NETWORK_CONFIG,tmp);

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
	status->publicIdentity = RR->publicIdentityStr;
	status->secretIdentity = RR->secretIdentityStr;
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
		p->isBonded = 0;
		if (pi->second->remoteVersionKnown()) {
			p->versionMajor = pi->second->remoteVersionMajor();
			p->versionMinor = pi->second->remoteVersionMinor();
			p->versionRev = pi->second->remoteVersionRevision();
		} else {
			p->versionMajor = -1;
			p->versionMinor = -1;
			p->versionRev = -1;
		}
		p->latency = pi->second->latency(_now);
		if (p->latency >= 0xffff)
			p->latency = -1;
		p->role = RR->topology->role(pi->second->identity().address());

		std::vector< SharedPtr<Path> > paths(pi->second->paths(_now));
		SharedPtr<Path> bestp(pi->second->getAppropriatePath(_now,false));
		p->pathCount = 0;
		for(std::vector< SharedPtr<Path> >::iterator path(paths.begin());path!=paths.end();++path) {
			memcpy(&(p->paths[p->pathCount].address),&((*path)->address()),sizeof(struct sockaddr_storage));
			p->paths[p->pathCount].localSocket = (*path)->localSocket();
			p->paths[p->pathCount].lastSend = (*path)->lastOut();
			p->paths[p->pathCount].lastReceive = (*path)->lastIn();
			p->paths[p->pathCount].trustedPathId = RR->topology->getOutboundPathTrust((*path)->address());
			p->paths[p->pathCount].expired = 0;
			p->paths[p->pathCount].preferred = ((*path) == bestp) ? 1 : 0;
			p->paths[p->pathCount].scope = (*path)->ipScope();
			++p->pathCount;
		}
		if (pi->second->bond()) {
			p->isBonded = pi->second->bond();
			p->bondingPolicy = pi->second->bond()->policy();
			p->isHealthy = pi->second->bond()->isHealthy();
			p->numAliveLinks = pi->second->bond()->getNumAliveLinks();
			p->numTotalLinks = pi->second->bond()->getNumTotalLinks();
		}
	}

	return pl;
}

ZT_VirtualNetworkConfig *Node::networkConfig(uint64_t nwid) const
{
	Mutex::Lock _l(_networks_m);
	const SharedPtr<Network> *nw = _networks.get(nwid);
	if (nw) {
		ZT_VirtualNetworkConfig *nc = (ZT_VirtualNetworkConfig *)::malloc(sizeof(ZT_VirtualNetworkConfig));
		(*nw)->externalConfig(nc);
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
	Hashtable< uint64_t,SharedPtr<Network> >::Iterator i(*const_cast< Hashtable< uint64_t,SharedPtr<Network> > *>(&_networks));
	uint64_t *k = (uint64_t *)0;
	SharedPtr<Network> *v = (SharedPtr<Network> *)0;
	while (i.next(k,v))
		(*v)->externalConfig(&(nl->networks[nl->networkCount++]));

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

/****************************************************************************/
/* Node methods used only within node/                                      */
/****************************************************************************/

bool Node::shouldUsePathForZeroTierTraffic(void *tPtr,const Address &ztaddr,const int64_t localSocket,const InetAddress &remoteAddress)
{
	if (!Path::isAddressValidForPath(remoteAddress))
		return false;

	if (RR->topology->isProhibitedEndpoint(ztaddr,remoteAddress))
		return false;

	{
		Mutex::Lock _l(_networks_m);
		Hashtable< uint64_t,SharedPtr<Network> >::Iterator i(_networks);
		uint64_t *k = (uint64_t *)0;
		SharedPtr<Network> *v = (SharedPtr<Network> *)0;
		while (i.next(k,v)) {
			if ((*v)->hasConfig()) {
				for(unsigned int k=0;k<(*v)->config().staticIpCount;++k) {
					if ((*v)->config().staticIps[k].containsAddress(remoteAddress))
						return false;
				}
			}
		}
	}

	return ( (_cb.pathCheckFunction) ? (_cb.pathCheckFunction(reinterpret_cast<ZT_Node *>(this),_uPtr,tPtr,ztaddr.toInt(),localSocket,reinterpret_cast<const struct sockaddr_storage *>(&remoteAddress)) != 0) : true);
}

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

ZT_ResultCode Node::setPhysicalPathConfiguration(const struct sockaddr_storage *pathNetwork, const ZT_PhysicalPathConfiguration *pathConfig)
{
	RR->topology->setPhysicalPathConfiguration(pathNetwork,pathConfig);
	return ZT_RESULT_OK;
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
	_localControllerAuthorizations_m.lock();
	_localControllerAuthorizations[_LocalControllerAuth(nwid,destination)] = now();
	_localControllerAuthorizations_m.unlock();

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
					const unsigned int chunkLen = std::min(totalSize - chunkIndex,(unsigned int)(ZT_PROTO_MAX_PACKET_LENGTH - (ZT_PACKET_IDX_PAYLOAD + 256)));
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

void Node::ncSendError(uint64_t nwid,uint64_t requestPacketId,const Address &destination,NetworkController::ErrorCode errorCode, const void *errorData, unsigned int errorDataSize)
{
	if (destination == RR->identity.address()) {
		SharedPtr<Network> n(network(nwid));
		if (!n) return;
		switch(errorCode) {
			case NetworkController::NC_ERROR_OBJECT_NOT_FOUND:
			case NetworkController::NC_ERROR_INTERNAL_SERVER_ERROR:
				n->setNotFound(nullptr);
				break;
			case NetworkController::NC_ERROR_ACCESS_DENIED:
				n->setAccessDenied(nullptr);
				break;
			case NetworkController::NC_ERROR_AUTHENTICATION_REQUIRED: {
				fprintf(stderr, "\n\nGot auth required\n\n");

				break;
			} 

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
			case NetworkController::NC_ERROR_AUTHENTICATION_REQUIRED:
				outp.append((unsigned char)Packet::ERROR_NETWORK_AUTHENTICATION_REQUIRED);
				break;
		}

		outp.append(nwid);

		if ((errorData)&&(errorDataSize > 0)&&(errorDataSize <= 0xffff)) {
			outp.append((uint16_t)errorDataSize);
			outp.append(errorData, errorDataSize);
		}

		RR->sw->send((void *)0,outp,true);
	} // else we can't send an ERROR() in response to nothing, so discard
}

} // namespace ZeroTier

/****************************************************************************/
/* CAPI bindings                                                            */
/****************************************************************************/

extern "C" {

enum ZT_ResultCode ZT_Node_new(ZT_Node **node,void *uptr,void *tptr,const struct ZT_Node_Callbacks *callbacks,int64_t now)
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
	int64_t now,
	int64_t localSocket,
	const struct sockaddr_storage *remoteAddress,
	const void *packetData,
	unsigned int packetLength,
	volatile int64_t *nextBackgroundTaskDeadline)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->processWirePacket(tptr,now,localSocket,remoteAddress,packetData,packetLength,nextBackgroundTaskDeadline);
	} catch (std::bad_alloc &exc) {
		return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
	} catch ( ... ) {
		return ZT_RESULT_OK; // "OK" since invalid packets are simply dropped, but the system is still up
	}
}

enum ZT_ResultCode ZT_Node_processVirtualNetworkFrame(
	ZT_Node *node,
	void *tptr,
	int64_t now,
	uint64_t nwid,
	uint64_t sourceMac,
	uint64_t destMac,
	unsigned int etherType,
	unsigned int vlanId,
	const void *frameData,
	unsigned int frameLength,
	volatile int64_t *nextBackgroundTaskDeadline)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->processVirtualNetworkFrame(tptr,now,nwid,sourceMac,destMac,etherType,vlanId,frameData,frameLength,nextBackgroundTaskDeadline);
	} catch (std::bad_alloc &exc) {
		return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
	} catch ( ... ) {
		return ZT_RESULT_FATAL_ERROR_INTERNAL;
	}
}

enum ZT_ResultCode ZT_Node_processBackgroundTasks(ZT_Node *node,void *tptr,int64_t now,volatile int64_t *nextBackgroundTaskDeadline)
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

enum ZT_ResultCode ZT_Node_deorbit(ZT_Node *node,void *tptr,uint64_t moonWorldId)
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

enum ZT_ResultCode ZT_Node_setPhysicalPathConfiguration(ZT_Node *node,const struct sockaddr_storage *pathNetwork,const ZT_PhysicalPathConfiguration *pathConfig)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->setPhysicalPathConfiguration(pathNetwork,pathConfig);
	} catch ( ... ) {
		return ZT_RESULT_FATAL_ERROR_INTERNAL;
	}
}

void ZT_version(int *major,int *minor,int *revision)
{
	if (major) *major = ZEROTIER_ONE_VERSION_MAJOR;
	if (minor) *minor = ZEROTIER_ONE_VERSION_MINOR;
	if (revision) *revision = ZEROTIER_ONE_VERSION_REVISION;
}

} // extern "C"
