/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
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
#include "ScopedPtr.hpp"
#include "Locator.hpp"

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
	_lastPing(0),
	_lastHousekeepingRun(0),
	_lastNetworkHousekeepingRun(0),
	_lastDynamicRootUpdate(0),
	_online(false)
{
	memcpy(&_cb,callbacks,sizeof(ZT_Node_Callbacks));

	memset(_expectingRepliesToBucketPtr,0,sizeof(_expectingRepliesToBucketPtr));
	memset(_expectingRepliesTo,0,sizeof(_expectingRepliesTo));
	memset(_lastIdentityVerification,0,sizeof(_lastIdentityVerification));

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
		RR->identity.generate(Identity::C25519);
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

		m = reinterpret_cast<char *>(::malloc(16 + ts + sws + mcs + topologys + sas));
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
		RR->topology = new (m) Topology(RR,RR->identity);
		m += topologys;
		RR->sa = new (m) SelfAwareness(RR);
	} catch ( ... ) {
		if (RR->sa) RR->sa->~SelfAwareness();
		if (RR->topology) RR->topology->~Topology();
		if (RR->mc) RR->mc->~Multicaster();
		if (RR->sw) RR->sw->~Switch();
		if (RR->t) RR->t->~Trace();
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
	} else {
		return ZT_RESULT_ERROR_NETWORK_NOT_FOUND;
	}
}

// This is passed as the argument to the DNS request handler and
// aggregates results.
struct _processBackgroundTasks_dnsResultAccumulator
{
	_processBackgroundTasks_dnsResultAccumulator(const Str &n) : dnsName(n) {}
	Str dnsName;
	std::vector<Str> txtRecords;
};

static const ZT_DNSRecordType s_txtRecordType[1] = { ZT_DNS_RECORD_TXT };

struct _processBackgroundTasks_eachRootName
{
	ZT_Node_Callbacks *cb;
	Node *n;
	void *uPtr;
	void *tPtr;
	bool updateAll;

	ZT_ALWAYS_INLINE bool operator()(const Str &dnsName,const Locator &loc)
	{
		if ((strchr(dnsName.c_str(),'.'))&&((updateAll)||(!loc))) {
			_processBackgroundTasks_dnsResultAccumulator *dnsReq = new _processBackgroundTasks_dnsResultAccumulator(dnsName);
			cb->dnsResolver(reinterpret_cast<ZT_Node *>(n),uPtr,tPtr,s_txtRecordType,1,dnsName.c_str(),(uintptr_t)dnsReq);
		}
		return true;
	}
};

struct _processBackgroundTasks_ping_eachRoot
{
	Hashtable< void *,bool > roots;
	int64_t now;
	void *tPtr;
	bool online;

	ZT_ALWAYS_INLINE bool operator()(const SharedPtr<Peer> &peer,const std::vector<InetAddress> &addrs)
	{
		unsigned int v4SendCount = 0,v6SendCount = 0;
		peer->ping(tPtr,now,v4SendCount,v6SendCount);
		for(std::vector<InetAddress>::const_iterator a(addrs.begin());a!=addrs.end();++a) {
			if ( ((a->isV4())&&(v4SendCount == 0)) || ((a->isV6())&&(v6SendCount == 0)) )
				peer->sendHELLO(tPtr,-1,*a,now);
		}
		if (!online)
			online = ((now - peer->lastReceive()) <= ((ZT_PEER_PING_PERIOD * 2) + 5000));
		roots.set((void *)peer.ptr(),true);
		return true;
	}
};

struct _processBackgroundTasks_ping_eachPeer
{
	int64_t now;
	void *tPtr;
	Hashtable< void *,bool > *roots;

	ZT_ALWAYS_INLINE bool operator()(const SharedPtr<Peer> &peer)
	{
		if (!roots->contains((void *)peer.ptr())) {
			unsigned int v4SendCount = 0,v6SendCount = 0;
			peer->ping(tPtr,now,v4SendCount,v6SendCount);
		}
		return true;
	}
};

ZT_ResultCode Node::processBackgroundTasks(void *tptr,int64_t now,volatile int64_t *nextBackgroundTaskDeadline)
{
	_now = now;
	Mutex::Lock bl(_backgroundTasksLock);

	// Initialize these on first call so these things happen just a few seconds after
	// startup, since right at startup things are likely to not be ready to communicate
	// at all yet.
	if (_lastNetworkHousekeepingRun <= 0)
		_lastNetworkHousekeepingRun = now - (ZT_NETWORK_HOUSEKEEPING_PERIOD / 3);
	if (_lastHousekeepingRun <= 0)
		_lastHousekeepingRun = now;

	if ((now - _lastPing) >= ZT_PEER_PING_PERIOD) {
		_lastPing = now;
		try {
			// Periodically refresh locators for dynamic roots from their DNS names.
			if (_cb.dnsResolver) {
				_processBackgroundTasks_eachRootName cr;
				cr.cb = &_cb;
				cr.n = this;
				cr.uPtr = _uPtr;
				cr.tPtr = tptr;
				if ((now - _lastDynamicRootUpdate) >= ZT_DYNAMIC_ROOT_UPDATE_PERIOD) {
					_lastDynamicRootUpdate = now;
					cr.updateAll = true;
				} else {
					cr.updateAll = false;
				}
				RR->topology->eachRootName(cr);
			}

			// Ping each root explicitly no matter what
			_processBackgroundTasks_ping_eachRoot rf;
			rf.now = now;
			rf.tPtr = tptr;
			rf.online = false;
			RR->topology->eachRoot(rf);

			// Ping peers that are active and we want to keep alive
			_processBackgroundTasks_ping_eachPeer pf;
			pf.now = now;
			pf.tPtr = tptr;
			pf.roots = &rf.roots;
			RR->topology->eachPeer(pf);

			// Update online status based on whether we can reach a root
			if (rf.online != _online) {
				_online = rf.online;
				postEvent(tptr,_online ? ZT_EVENT_ONLINE : ZT_EVENT_OFFLINE);
			}
		} catch ( ... ) {
			return ZT_RESULT_FATAL_ERROR_INTERNAL;
		}
	}

	if ((now - _lastNetworkHousekeepingRun) >= ZT_NETWORK_HOUSEKEEPING_PERIOD) {
		_lastHousekeepingRun = now;
		{
			Mutex::Lock l(_networks_m);
			Hashtable< uint64_t,SharedPtr<Network> >::Iterator i(_networks);
			uint64_t *nwid = (uint64_t *)0;
			SharedPtr<Network> *network = (SharedPtr<Network> *)0;
			while (i.next(nwid,network)) {
				(*network)->doPeriodicTasks(tptr,now);
			}
		}
		RR->t->updateMemoizedSettings();
	}

	if ((now - _lastHousekeepingRun) >= ZT_HOUSEKEEPING_PERIOD) {
		_lastHousekeepingRun = now;
		try {
			// Clean up any old local controller auth memoizations. This is an
			// optimization for network controllers to know whether to accept
			// or trust nodes without doing an extra cert check.
			{
				_localControllerAuthorizations_m.lock();
				Hashtable< _LocalControllerAuth,int64_t >::Iterator i(_localControllerAuthorizations);
				_LocalControllerAuth *k = (_LocalControllerAuth *)0;
				int64_t *v = (int64_t *)0;
				while (i.next(k,v)) {
					if ((*v - now) > (ZT_NETWORK_AUTOCONF_DELAY * 3)) {
						_localControllerAuthorizations.erase(*k);
					}
				}
				_localControllerAuthorizations_m.unlock();
			}

			RR->topology->doPeriodicTasks(now);
			RR->sa->clean(now);
			RR->mc->clean(now);
		} catch ( ... ) {
			return ZT_RESULT_FATAL_ERROR_INTERNAL;
		}
	}

	try {
		*nextBackgroundTaskDeadline = now + (int64_t)std::max(std::min((unsigned long)ZT_MAX_TIMER_TASK_INTERVAL,RR->sw->doTimerTasks(tptr,now)),(unsigned long)ZT_MIN_TIMER_TASK_INTERVAL);
	} catch ( ... ) {
		return ZT_RESULT_FATAL_ERROR_INTERNAL;
	}

	return ZT_RESULT_OK;
}

void Node::processDNSResult(
	void *tptr,
	uintptr_t dnsRequestID,
	const char *name,
	enum ZT_DNSRecordType recordType,
	const void *result,
	unsigned int resultLength,
	int resultIsString)
{
	if (dnsRequestID) {
		_processBackgroundTasks_dnsResultAccumulator *const acc = reinterpret_cast<_processBackgroundTasks_dnsResultAccumulator *>(dnsRequestID);
		if (recordType == ZT_DNS_RECORD_TXT) {
			if (result)
				acc->txtRecords.emplace_back(reinterpret_cast<const char *>(result));
		} else if (recordType == ZT_DNS_RECORD__END_OF_RESULTS) {
			Locator loc;
			if (loc.decodeTxtRecords(acc->dnsName,acc->txtRecords.begin(),acc->txtRecords.end())) {
				RR->topology->setRoot(acc->dnsName,loc);
				delete acc;
			}
		}
	}
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

ZT_RootList *Node::listRoots(int64_t now)
{
	return RR->topology->apiRoots(now);
}

enum ZT_ResultCode Node::setRoot(const char *name,const void *locator,unsigned int locatorSize)
{
	try {
		Locator loc;
		if ((locator)&&(locatorSize > 0)&&(locatorSize < 65535)) {
			ScopedPtr< Buffer<65536> > locbuf(new Buffer<65536>());
			locbuf->append(locator,locatorSize);
			loc.deserialize(*locbuf,0);
			if (!loc.verify())
				return ZT_RESULT_ERROR_BAD_PARAMETER;
		}
		Str n;
		if ((!name)||(strlen(name) == 0)) {
			if (!loc)
				return ZT_RESULT_ERROR_BAD_PARAMETER; /* no name and no locator */
			char tmp[16];
			loc.id().address().toString(tmp);
			n = tmp;
		} else {
			n = name;
		}
		return RR->topology->setRoot(n,loc) ? ZT_RESULT_OK : ZT_RESULT_OK_IGNORED;
	} catch ( ... ) {
		return ZT_RESULT_ERROR_BAD_PARAMETER;
	}
}

enum ZT_ResultCode Node::removeRoot(const char *name)
{
	try {
		if (name)
			RR->topology->removeRoot(Str(name));
	} catch ( ... ) {}
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

struct _sortPeerPtrsByAddress { inline bool operator()(const SharedPtr<Peer> &a,const SharedPtr<Peer> &b) const { return (a->address() < b->address()); } };

ZT_PeerList *Node::peers() const
{
	std::vector< SharedPtr<Peer> > peers;
	RR->topology->getAllPeers(peers);
	std::sort(peers.begin(),peers.end(),_sortPeerPtrsByAddress());

	char *buf = (char *)::malloc(sizeof(ZT_PeerList) + (sizeof(ZT_Peer) * peers.size()));
	if (!buf)
		return (ZT_PeerList *)0;
	ZT_PeerList *pl = (ZT_PeerList *)buf;
	pl->peers = (ZT_Peer *)(buf + sizeof(ZT_PeerList));

	pl->peerCount = 0;
	for(std::vector< SharedPtr<Peer> >::iterator pi(peers.begin());pi!=peers.end();++pi) {
		ZT_Peer *p = &(pl->peers[pl->peerCount++]);
		p->address = (*pi)->address().toInt();
		p->hadAggregateLink = 0;
		if ((*pi)->remoteVersionKnown()) {
			p->versionMajor = (*pi)->remoteVersionMajor();
			p->versionMinor = (*pi)->remoteVersionMinor();
			p->versionRev = (*pi)->remoteVersionRevision();
		} else {
			p->versionMajor = -1;
			p->versionMinor = -1;
			p->versionRev = -1;
		}
		p->latency = (*pi)->latency(_now);
		if (p->latency >= 0xffff)
			p->latency = -1;
		p->role = RR->topology->isRoot((*pi)->identity()) ? ZT_PEER_ROLE_PLANET : ZT_PEER_ROLE_LEAF;

		const int64_t now = _now;
		std::vector< SharedPtr<Path> > paths((*pi)->paths(_now));
		SharedPtr<Path> bestp((*pi)->getAppropriatePath(_now,false));
		p->hadAggregateLink |= (*pi)->hasAggregateLink();
		p->pathCount = 0;
		for(std::vector< SharedPtr<Path> >::iterator path(paths.begin());path!=paths.end();++path) {
			memcpy(&(p->paths[p->pathCount].address),&((*path)->address()),sizeof(struct sockaddr_storage));
			p->paths[p->pathCount].lastSend = (*path)->lastOut();
			p->paths[p->pathCount].lastReceive = (*path)->lastIn();
			p->paths[p->pathCount].trustedPathId = RR->topology->getOutboundPathTrust((*path)->address());
			p->paths[p->pathCount].alive = (*path)->alive(now) ? 1 : 0;
			p->paths[p->pathCount].preferred = ((*path) == bestp) ? 1 : 0;
			p->paths[p->pathCount].latency = (float)(*path)->latency();
			p->paths[p->pathCount].packetDelayVariance = (*path)->packetDelayVariance();
			p->paths[p->pathCount].throughputDisturbCoeff = (*path)->throughputDisturbanceCoefficient();
			p->paths[p->pathCount].packetErrorRatio = (*path)->packetErrorRatio();
			p->paths[p->pathCount].packetLossRatio = (*path)->packetLossRatio();
			p->paths[p->pathCount].stability = (*path)->lastComputedStability();
			p->paths[p->pathCount].throughput = (*path)->meanThroughput();
			p->paths[p->pathCount].maxThroughput = (*path)->maxLifetimeThroughput();
			p->paths[p->pathCount].allocation = (float)(*path)->allocation() / (float)255;
			p->paths[p->pathCount].ifname = (*path)->getName();

			++p->pathCount;
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

void Node::setNetworkUserPtr(uint64_t nwid,void *ptr)
{
	Mutex::Lock _l(_networks_m);
	const SharedPtr<Network> *const nw = _networks.get(nwid);
	if (nw)
		*((*nw)->userPtr()) = ptr;
}

void Node::freeQueryResult(void *qr)
{
	if (qr)
		::free(qr);
}

int Node::addLocalInterfaceAddress(const struct sockaddr_storage *addr)
{
	if (Path::isAddressValidForPath(*(reinterpret_cast<const InetAddress *>(addr)))) {
		Mutex::Lock _l(_localInterfaceAddresses_m);
		if (std::find(_localInterfaceAddresses.begin(),_localInterfaceAddresses.end(),*(reinterpret_cast<const InetAddress *>(addr))) == _localInterfaceAddresses.end()) {
			_localInterfaceAddresses.push_back(*(reinterpret_cast<const InetAddress *>(addr)));
			return 1;
		}
	}
	return 0;
}

void Node::clearLocalInterfaceAddresses()
{
	Mutex::Lock _l(_localInterfaceAddresses_m);
	_localInterfaceAddresses.clear();
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

void Node::setController(void *networkControllerInstance)
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

ZT_ResultCode Node::setPhysicalPathConfiguration(const struct sockaddr_storage *pathNetwork, const ZT_PhysicalPathConfiguration *pathConfig)
{
	RR->topology->setPhysicalPathConfiguration(pathNetwork,pathConfig);
	return ZT_RESULT_OK;
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
		ScopedPtr< Dictionary<ZT_NETWORKCONFIG_DICT_CAPACITY> > dconf(new Dictionary<ZT_NETWORKCONFIG_DICT_CAPACITY>());
		if (nc.toDictionary(*dconf,sendLegacyFormatConfig)) {
			uint64_t configUpdateId = Utils::random();
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

				uint8_t sig[256];
				const unsigned int siglen = RR->identity.sign(reinterpret_cast<const uint8_t *>(outp.data()) + sigStart,outp.size() - sigStart,sig,sizeof(sig));
				outp.append((uint8_t)1);
				outp.append((uint16_t)siglen);
				outp.append(sig,siglen);

				outp.compress();
				RR->sw->send((void *)0,outp,true);
				chunkIndex += chunkLen;
			}
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

void ZT_Node_processDNSResult(
	ZT_Node *node,
	void *tptr,
	uintptr_t dnsRequestID,
	const char *name,
	enum ZT_DNSRecordType recordType,
	const void *result,
	unsigned int resultLength,
	int resultIsString)
{
	try {
		reinterpret_cast<ZeroTier::Node *>(node)->processDNSResult(tptr,dnsRequestID,name,recordType,result,resultLength,resultIsString);
	} catch ( ... ) {}
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

ZT_RootList *ZT_Node_listRoots(ZT_Node *node,int64_t now)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->listRoots(now);
	} catch ( ... ) {
		return nullptr;
	}
}

enum ZT_ResultCode ZT_Node_setRoot(ZT_Node *node,const char *name,const void *locator,unsigned int locatorSize)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->setRoot(name,locator,locatorSize);
	} catch (std::bad_alloc &exc) {
		return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
	} catch ( ... ) {
		return ZT_RESULT_FATAL_ERROR_INTERNAL;
	}
}

enum ZT_ResultCode ZT_Node_removeRoot(ZT_Node *node,const char *name)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->removeRoot(name);
	} catch (std::bad_alloc &exc) {
		return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
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

void ZT_Node_setNetworkUserPtr(ZT_Node *node,uint64_t nwid,void *ptr)
{
	try {
		reinterpret_cast<ZeroTier::Node *>(node)->setNetworkUserPtr(nwid,ptr);
	} catch ( ... ) {}
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

void ZT_Node_setController(ZT_Node *node,void *networkControllerInstance)
{
	try {
		reinterpret_cast<ZeroTier::Node *>(node)->setController(networkControllerInstance);
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
