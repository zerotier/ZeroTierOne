/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include <cstdlib>
#include <cstring>
#include <cstdint>

#include "Constants.hpp"
#include "SharedPtr.hpp"
#include "Node.hpp"
#include "NetworkController.hpp"
#include "Topology.hpp"
#include "Address.hpp"
#include "Identity.hpp"
#include "SelfAwareness.hpp"
#include "Network.hpp"
#include "Trace.hpp"
#include "Locator.hpp"
#include "Protocol.hpp"
#include "Expect.hpp"
#include "VL1.hpp"
#include "VL2.hpp"
#include "Buf.hpp"

namespace ZeroTier {

namespace {

struct _NodeObjects
{
	ZT_INLINE _NodeObjects(RuntimeEnvironment *const RR,void *const tPtr) :
		t(RR),
		expect(),
		vl2(RR),
		vl1(RR),
		sa(RR),
		topology(RR,tPtr)
	{
		RR->t = &t;
		RR->expect = &expect;
		RR->vl2 = &vl2;
		RR->vl1 = &vl1;
		RR->sa = &sa;
		RR->topology = &topology;
	}
	Trace t;
	Expect expect;
	VL2 vl2;
	VL1 vl1;
	SelfAwareness sa;
	Topology topology;
};

struct _sortPeerPtrsByAddress
{
	ZT_INLINE bool operator()(const SharedPtr<Peer> &a,const SharedPtr<Peer> &b) const { return (a->address() < b->address()); }
};

} // anonymous namespace

Node::Node(void *uPtr,void *tPtr,const struct ZT_Node_Callbacks *callbacks,int64_t now) :
	_RR(this),
	_objects(nullptr),
	RR(&_RR),
	_cb(*callbacks),
	_uPtr(uPtr),
	_networks(),
	_networksMask(15),
	_now(now),
	_lastPing(0),
	_lastHousekeepingRun(0),
	_lastNetworkHousekeepingRun(0),
	_lastPathKeepaliveCheck(0),
	_natMustDie(true),
	_online(false)
{
	_networks.resize(16); // _networksMask + 1, must be power of two

	uint64_t idtmp[2]; idtmp[0] = 0; idtmp[1] = 0;
	std::vector<uint8_t> data(stateObjectGet(tPtr,ZT_STATE_OBJECT_IDENTITY_SECRET,idtmp));
	bool haveIdentity = false;
	if (!data.empty()) {
		data.push_back(0); // zero-terminate string
		if (RR->identity.fromString((const char *)data.data())) {
			RR->identity.toString(false,RR->publicIdentityStr);
			RR->identity.toString(true,RR->secretIdentityStr);
			haveIdentity = true;
		}
	}

	if (!haveIdentity) {
		RR->identity.generate(Identity::C25519);
		RR->identity.toString(false,RR->publicIdentityStr);
		RR->identity.toString(true,RR->secretIdentityStr);
		idtmp[0] = RR->identity.address().toInt(); idtmp[1] = 0;
		stateObjectPut(tPtr,ZT_STATE_OBJECT_IDENTITY_SECRET,idtmp,RR->secretIdentityStr,(unsigned int)strlen(RR->secretIdentityStr));
		stateObjectPut(tPtr,ZT_STATE_OBJECT_IDENTITY_PUBLIC,idtmp,RR->publicIdentityStr,(unsigned int)strlen(RR->publicIdentityStr));
	} else {
		idtmp[0] = RR->identity.address().toInt(); idtmp[1] = 0;
		data = stateObjectGet(tPtr,ZT_STATE_OBJECT_IDENTITY_PUBLIC,idtmp);
		if ((data.empty())||(memcmp(data.data(),RR->publicIdentityStr,strlen(RR->publicIdentityStr)) != 0))
			stateObjectPut(tPtr,ZT_STATE_OBJECT_IDENTITY_PUBLIC,idtmp,RR->publicIdentityStr,(unsigned int)strlen(RR->publicIdentityStr));
	}

	uint8_t tmph[ZT_IDENTITY_HASH_SIZE];
	RR->identity.hashWithPrivate(tmph);
	RR->localCacheSymmetric.init(tmph);
	Utils::burn(tmph,sizeof(tmph));

	// This constructs all the components of the ZeroTier core within a single contiguous memory container,
	// which reduces memory fragmentation and may improve cache locality.
	_objects = new _NodeObjects(RR,tPtr);

	postEvent(tPtr, ZT_EVENT_UP);
}

Node::~Node()
{
	// Let go of all networks to leave them. Do it this way in case Network wants to
	// do anything in its destructor that locks the _networks lock to avoid a deadlock.
	std::vector< SharedPtr<Network> > networks;
	{
		RWMutex::Lock _l(_networks_m);
		networks.swap(_networks);
	}
	networks.clear();

	_networks_m.lock();
	_networks_m.unlock();

	if (_objects)
		delete (_NodeObjects *)_objects;

	// Let go of cached Buf objects. If other nodes happen to be running in this
	// same process space new Bufs will be allocated as needed, but this is almost
	// never the case. Calling this here saves RAM if we are running inside something
	// that wants to keep running after tearing down its ZeroTier core instance.
	Buf::freePool();
}

void Node::shutdown(void *tPtr)
{
	if (RR->topology)
		RR->topology->saveAll(tPtr);
}

ZT_ResultCode Node::processWirePacket(
	void *tPtr,
	int64_t now,
	int64_t localSocket,
	const struct sockaddr_storage *remoteAddress,
	SharedPtr<Buf> &packetData,
	unsigned int packetLength,
	volatile int64_t *nextBackgroundTaskDeadline)
{
	_now = now;
	RR->vl1->onRemotePacket(tPtr,localSocket,(remoteAddress) ? InetAddress::NIL : *asInetAddress(remoteAddress),packetData,packetLength);
	return ZT_RESULT_OK;
}

ZT_ResultCode Node::processVirtualNetworkFrame(
	void *tPtr,
	int64_t now,
	uint64_t nwid,
	uint64_t sourceMac,
	uint64_t destMac,
	unsigned int etherType,
	unsigned int vlanId,
	SharedPtr<Buf> &frameData,
	unsigned int frameLength,
	volatile int64_t *nextBackgroundTaskDeadline)
{
	_now = now;
	SharedPtr<Network> nw(this->network(nwid));
	if (nw) {
		RR->vl2->onLocalEthernet(tPtr,nw,MAC(sourceMac),MAC(destMac),etherType,vlanId,frameData,frameLength);
		return ZT_RESULT_OK;
	} else {
		return ZT_RESULT_ERROR_NETWORK_NOT_FOUND;
	}
}

struct _processBackgroundTasks_ping_eachPeer
{
	int64_t now;
	Node *parent;
	void *tPtr;
	bool online;
	std::vector<Address> rootsNotOnline;
	ZT_INLINE void operator()(const SharedPtr<Peer> &peer,const bool isRoot)
	{
		peer->ping(tPtr,now,isRoot);
		if (isRoot) {
			if (peer->active(now)) {
				online = true;
			} else {
				rootsNotOnline.push_back(peer->address());
			}
		}
	}
};

static uint8_t keepAlivePayload = 0; // junk payload for keepalive packets
struct _processBackgroundTasks_path_keepalive
{
	int64_t now;
	RuntimeEnvironment *RR;
	void *tPtr;
	ZT_INLINE void operator()(const SharedPtr<Path> &path)
	{
		if ((now - path->lastOut()) >= ZT_PATH_KEEPALIVE_PERIOD) {
			++keepAlivePayload;
			path->send(RR,tPtr,&keepAlivePayload,1,now);
		}
	}
};

ZT_ResultCode Node::processBackgroundTasks(void *tPtr, int64_t now, volatile int64_t *nextBackgroundTaskDeadline)
{
	_now = now;
	Mutex::Lock bl(_backgroundTasksLock);

	if ((now - _lastPing) >= ZT_PEER_PING_PERIOD) {
		_lastPing = now;
		try {
			_processBackgroundTasks_ping_eachPeer pf;
			pf.now = now;
			pf.parent = this;
			pf.tPtr = tPtr;
			pf.online = false;
			RR->topology->eachPeerWithRoot<_processBackgroundTasks_ping_eachPeer &>(pf);

			if (pf.online != _online) {
				_online = pf.online;
				postEvent(tPtr, _online ? ZT_EVENT_ONLINE : ZT_EVENT_OFFLINE);
			}

			RR->topology->rankRoots(now);

			if (pf.online) {
				// If we have at least one online root, request whois for roots not online.
				// This will give us updated locators for these roots which may contain new
				// IP addresses. It will also auto-discover IPs for roots that were not added
				// with an initial bootstrap address.
				// TODO
				//for (std::vector<Address>::const_iterator r(pf.rootsNotOnline.begin()); r != pf.rootsNotOnline.end(); ++r)
				//	RR->sw->requestWhois(tPtr,now,*r);
			}
		} catch ( ... ) {
			return ZT_RESULT_FATAL_ERROR_INTERNAL;
		}
	}

	if ((now - _lastNetworkHousekeepingRun) >= ZT_NETWORK_HOUSEKEEPING_PERIOD) {
		_lastHousekeepingRun = now;
		{
			RWMutex::RLock l(_networks_m);
			for(std::vector< SharedPtr<Network> >::const_iterator i(_networks.begin());i!=_networks.end();++i) {
				if ((*i))
					(*i)->doPeriodicTasks(tPtr,now);
			}
		}
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

			RR->topology->doPeriodicTasks(tPtr, now);
			RR->sa->clean(now);
		} catch ( ... ) {
			return ZT_RESULT_FATAL_ERROR_INTERNAL;
		}
	}

	if ((now - _lastPathKeepaliveCheck) >= ZT_PATH_KEEPALIVE_PERIOD) {
		_lastPathKeepaliveCheck = now;
		_processBackgroundTasks_path_keepalive pf;
		pf.now = now;
		pf.RR = RR;
		pf.tPtr = tPtr;
		RR->topology->eachPath<_processBackgroundTasks_path_keepalive &>(pf);
	}

	int64_t earliestAlarmAt = 0x7fffffffffffffffLL;
	std::vector<Address> bzzt;
	{
		RWMutex::RMaybeWLock l(_peerAlarms_l);
		for(std::map<Address,int64_t>::iterator a(_peerAlarms.begin());a!=_peerAlarms.end();) {
			if (now >= a->second) {
				bzzt.push_back(a->first);
				l.writing();
				_peerAlarms.erase(a++);
			} else {
				if (a->second < earliestAlarmAt)
					earliestAlarmAt = a->second;
				++a;
			}
		}
	}
	for(std::vector<Address>::iterator a(bzzt.begin());a!=bzzt.end();++a) {
		const SharedPtr<Peer> p(RR->topology->peer(tPtr,*a,false));
		if (p)
			p->alarm(tPtr,now);
	}

	try {
		*nextBackgroundTaskDeadline = std::min(earliestAlarmAt,now + ZT_MAX_TIMER_TASK_INTERVAL);
	} catch ( ... ) {
		return ZT_RESULT_FATAL_ERROR_INTERNAL;
	}

	return ZT_RESULT_OK;
}

ZT_ResultCode Node::join(uint64_t nwid,void *uptr,void *tptr)
{
	RWMutex::Lock l(_networks_m);

	const uint64_t nwidHashed = nwid + (nwid >> 32U);
	SharedPtr<Network> *nw = &(_networks[(unsigned long)(nwidHashed & _networksMask)]);

	// Enlarge flat hash table of networks until all networks fit without collisions.
	if (*nw) {
		unsigned long newNetworksSize = (unsigned long)_networks.size();
		std::vector< SharedPtr<Network> > newNetworks;
		uint64_t newNetworksMask,id;
		std::vector< SharedPtr<Network> >::const_iterator i;

try_larger_network_hashtable:
		newNetworksSize <<= 1U; // must remain a power of two
		newNetworks.clear();
		newNetworks.resize(newNetworksSize);
		newNetworksMask = (uint64_t)(newNetworksSize - 1);

		for(i=_networks.begin();i!=_networks.end();++i) {
			id = (*i)->id();
			nw = &(newNetworks[(unsigned long)((id + (id >> 32U)) & newNetworksMask)]);
			if (*nw)
				goto try_larger_network_hashtable;
			*nw = *i;
		}
		if (newNetworks[(unsigned long)(nwidHashed & newNetworksMask)])
			goto try_larger_network_hashtable;

		_networks.swap(newNetworks);
		_networksMask = newNetworksMask;
		nw = &(_networks[(unsigned long)(nwidHashed & newNetworksMask)]);
	}

	nw->set(new Network(RR,tptr,nwid,uptr,(const NetworkConfig *)0));

	return ZT_RESULT_OK;
}

ZT_ResultCode Node::leave(uint64_t nwid,void **uptr,void *tptr)
{
	const uint64_t nwidHashed = nwid + (nwid >> 32U);

	ZT_VirtualNetworkConfig ctmp;
	void **nUserPtr = (void **)0;
	{
		RWMutex::RLock l(_networks_m);
		SharedPtr<Network> &nw = _networks[(unsigned long)(nwidHashed & _networksMask)];
		if (!nw)
			return ZT_RESULT_OK;
		if (uptr)
			*uptr = nw->userPtr();
		nw->externalConfig(&ctmp);
		nw->destroy();
		nUserPtr = nw->userPtr();
	}

	if (nUserPtr)
		RR->node->configureVirtualNetworkPort(tptr,nwid,nUserPtr,ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY,&ctmp);

	{
		RWMutex::Lock _l(_networks_m);
		_networks[(unsigned long)(nwidHashed & _networksMask)].zero();
	}

	uint64_t tmp[2];
	tmp[0] = nwid; tmp[1] = 0;
	RR->node->stateObjectDelete(tptr,ZT_STATE_OBJECT_NETWORK_CONFIG,tmp);

	return ZT_RESULT_OK;
}

ZT_ResultCode Node::multicastSubscribe(void *tPtr,uint64_t nwid,uint64_t multicastGroup,unsigned long multicastAdi)
{
	SharedPtr<Network> nw(this->network(nwid));
	if (nw) {
		nw->multicastSubscribe(tPtr,MulticastGroup(MAC(multicastGroup),(uint32_t)(multicastAdi & 0xffffffff)));
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

ZT_ResultCode Node::addRoot(void *tPtr,const ZT_Identity *identity,const sockaddr_storage *bootstrap)
{
	if (!identity)
		return ZT_RESULT_ERROR_BAD_PARAMETER;
	InetAddress a;
	if (bootstrap)
		a = bootstrap;
	RR->topology->addRoot(tPtr,*reinterpret_cast<const Identity *>(identity),a);
	return ZT_RESULT_OK;
}

ZT_ResultCode Node::removeRoot(void *tPtr,const ZT_Identity *identity)
{
	if (!identity)
		return ZT_RESULT_ERROR_BAD_PARAMETER;
	RR->topology->removeRoot(*reinterpret_cast<const Identity *>(identity));
	return ZT_RESULT_OK;
}

uint64_t Node::address() const
{
	return RR->identity.address().toInt();
}

void Node::status(ZT_NodeStatus *status) const
{
	status->address = RR->identity.address().toInt();
	status->identity = reinterpret_cast<const ZT_Identity *>(&RR->identity);
	status->publicIdentity = RR->publicIdentityStr;
	status->secretIdentity = RR->secretIdentityStr;
	status->online = _online ? 1 : 0;
}

ZT_PeerList *Node::peers() const
{
	std::vector< SharedPtr<Peer> > peers;
	RR->topology->getAllPeers(peers);
	std::sort(peers.begin(),peers.end(),_sortPeerPtrsByAddress());

	char *buf = (char *)::malloc(sizeof(ZT_PeerList) + (sizeof(ZT_Peer) * peers.size()) + (sizeof(Identity) * peers.size()));
	if (!buf)
		return (ZT_PeerList *)0;
	ZT_PeerList *pl = (ZT_PeerList *)buf;
	pl->peers = (ZT_Peer *)(buf + sizeof(ZT_PeerList));
	Identity *identities = (Identity *)(buf + sizeof(ZT_PeerList) + (sizeof(ZT_Peer) * peers.size()));

	const int64_t now = _now;
	pl->peerCount = 0;
	for(std::vector< SharedPtr<Peer> >::iterator pi(peers.begin());pi!=peers.end();++pi) {
		ZT_Peer *p = &(pl->peers[pl->peerCount]);

		p->address = (*pi)->address().toInt();
		identities[pl->peerCount] = (*pi)->identity(); // need to make a copy in case peer gets deleted
		p->identity = &identities[pl->peerCount];
		p->fingerprint.address = p->address;
		memcpy(p->fingerprint.hash,(*pi)->identity().fingerprint().hash(),ZT_IDENTITY_HASH_SIZE);
		if ((*pi)->remoteVersionKnown()) {
			p->versionMajor = (int)(*pi)->remoteVersionMajor();
			p->versionMinor = (int)(*pi)->remoteVersionMinor();
			p->versionRev = (int)(*pi)->remoteVersionRevision();
		} else {
			p->versionMajor = -1;
			p->versionMinor = -1;
			p->versionRev = -1;
		}
		p->latency = (int)(*pi)->latency();
		if (p->latency >= 0xffff)
			p->latency = -1;
		p->root = RR->topology->isRoot((*pi)->identity()) ? 1 : 0;
		memcpy(&p->bootstrap,&((*pi)->bootstrap()),sizeof(sockaddr_storage));

		std::vector< SharedPtr<Path> > paths;
		(*pi)->getAllPaths(paths);
		p->pathCount = 0;
		for(std::vector< SharedPtr<Path> >::iterator path(paths.begin());path!=paths.end();++path) {
			memcpy(&(p->paths[p->pathCount].address),&((*path)->address()),sizeof(struct sockaddr_storage));
			p->paths[p->pathCount].lastSend = (*path)->lastOut();
			p->paths[p->pathCount].lastReceive = (*path)->lastIn();
			p->paths[p->pathCount].trustedPathId = RR->topology->getOutboundPathTrust((*path)->address());
			p->paths[p->pathCount].alive = (*path)->alive(now) ? 1 : 0;
			p->paths[p->pathCount].preferred = (p->pathCount == 0) ? 1 : 0;
			++p->pathCount;
		}

		++pl->peerCount;
	}

	return pl;
}

ZT_VirtualNetworkConfig *Node::networkConfig(uint64_t nwid) const
{
	SharedPtr<Network> nw(network(nwid));
	if (nw) {
		ZT_VirtualNetworkConfig *const nc = (ZT_VirtualNetworkConfig *)::malloc(sizeof(ZT_VirtualNetworkConfig));
		nw->externalConfig(nc);
		return nc;
	}
	return (ZT_VirtualNetworkConfig *)0;
}

ZT_VirtualNetworkList *Node::networks() const
{
	RWMutex::RLock l(_networks_m);

	unsigned long networkCount = 0;
	for(std::vector< SharedPtr<Network> >::const_iterator i(_networks.begin());i!=_networks.end();++i) {
		if ((*i))
			++networkCount;
	}

	char *const buf = (char *)::malloc(sizeof(ZT_VirtualNetworkList) + (sizeof(ZT_VirtualNetworkConfig) * networkCount));
	if (!buf)
		return (ZT_VirtualNetworkList *)0;
	ZT_VirtualNetworkList *nl = (ZT_VirtualNetworkList *)buf;
	nl->networks = (ZT_VirtualNetworkConfig *)(buf + sizeof(ZT_VirtualNetworkList));

	nl->networkCount = 0;
	for(std::vector< SharedPtr<Network> >::const_iterator i(_networks.begin());i!=_networks.end();++i) {
		if ((*i))
			(*i)->externalConfig(&(nl->networks[nl->networkCount++]));
	}

	return nl;
}

void Node::setNetworkUserPtr(uint64_t nwid,void *ptr)
{
	SharedPtr<Network> nw(network(nwid));
	if (nw)
		*(nw->userPtr()) = ptr;
}

void Node::freeQueryResult(void *qr)
{
	if (qr)
		::free(qr);
}

void Node::setInterfaceAddresses(const ZT_InterfaceAddress *addrs,unsigned int addrCount)
{
	Mutex::Lock _l(_localInterfaceAddresses_m);
	_localInterfaceAddresses.clear();
	for(unsigned int i=0;i<addrCount;++i) {
		bool dupe = false;
		for(unsigned int j=0;j<i;++j) {
			if (*(reinterpret_cast<const InetAddress *>(&addrs[j].address)) == *(reinterpret_cast<const InetAddress *>(&addrs[i].address))) {
				dupe = true;
				break;
			}
		}
		if (!dupe)
			_localInterfaceAddresses.push_back(addrs[i]);
	}
}

int Node::sendUserMessage(void *tptr,uint64_t dest,uint64_t typeId,const void *data,unsigned int len)
{
	try {
		if (RR->identity.address().toInt() != dest) {
			// TODO
			/*
			Packet outp(Address(dest),RR->identity.address(),Packet::VERB_USER_MESSAGE);
			outp.append(typeId);
			outp.append(data,len);
			outp.compress();
			RR->sw->send(tptr,outp,true);
			*/
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

// Methods used only within the core ----------------------------------------------------------------------------------

std::vector<uint8_t> Node::stateObjectGet(void *const tPtr,ZT_StateObjectType type,const uint64_t id[2])
{
	std::vector<uint8_t> r;
	if (_cb.stateGetFunction) {
		void *data = 0;
		void (*freeFunc)(void *) = 0;
		int l = _cb.stateGetFunction(
			reinterpret_cast<ZT_Node *>(this),
			_uPtr,
			tPtr,
			type,
			id,
			&data,
			&freeFunc);
		if ((l > 0)&&(data)&&(freeFunc)) {
			r.assign(reinterpret_cast<const uint8_t *>(data),reinterpret_cast<const uint8_t *>(data) + l);
			freeFunc(data);
		}
	}
	return r;
}

bool Node::shouldUsePathForZeroTierTraffic(void *tPtr,const Identity &id,const int64_t localSocket,const InetAddress &remoteAddress)
{
	{
		RWMutex::RLock l(_networks_m);
		for (std::vector<SharedPtr<Network> >::iterator i(_networks.begin()); i != _networks.end(); ++i) {
			if ((*i)) {
				for (unsigned int k = 0,j = (*i)->config().staticIpCount; k < j; ++k) {
					if ((*i)->config().staticIps[k].containsAddress(remoteAddress))
						return false;
				}
			}
		}
	}

	if (_cb.pathCheckFunction) {
		return (_cb.pathCheckFunction(
			reinterpret_cast<ZT_Node *>(this),
			_uPtr,
			tPtr,
			id.address().toInt(),
			(const ZT_Identity *)&id,
			localSocket,
			reinterpret_cast<const struct sockaddr_storage *>(&remoteAddress)) != 0);
	}

	return true;
}

bool Node::externalPathLookup(void *tPtr,const Identity &id,int family,InetAddress &addr)
{
	if (_cb.pathLookupFunction) {
		return (_cb.pathLookupFunction(
			reinterpret_cast<ZT_Node *>(this),
			_uPtr,
			tPtr,
			id.address().toInt(),
			reinterpret_cast<const ZT_Identity *>(&id),
			family,
			reinterpret_cast<sockaddr_storage *>(&addr)) == ZT_RESULT_OK);
	}
	return false;
}

ZT_ResultCode Node::setPhysicalPathConfiguration(const struct sockaddr_storage *pathNetwork, const ZT_PhysicalPathConfiguration *pathConfig)
{
	RR->topology->setPhysicalPathConfiguration(pathNetwork,pathConfig);
	return ZT_RESULT_OK;
}

bool Node::localControllerHasAuthorized(const int64_t now,const uint64_t nwid,const Address &addr) const
{
	_localControllerAuthorizations_m.lock();
	const int64_t *const at = _localControllerAuthorizations.get(_LocalControllerAuth(nwid,addr));
	_localControllerAuthorizations_m.unlock();
	if (at)
		return ((now - *at) < (ZT_NETWORK_AUTOCONF_DELAY * 3));
	return false;
}

// Implementation of NetworkController::Sender ------------------------------------------------------------------------

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
		Dictionary dconf;
		if (nc.toDictionary(dconf,sendLegacyFormatConfig)) {
			uint64_t configUpdateId = Utils::random();
			if (!configUpdateId) ++configUpdateId;

			std::vector<uint8_t> ddata;
			dconf.encode(ddata);
			// TODO
			/*
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
			*/
		}
	}
}

void Node::ncSendRevocation(const Address &destination,const Revocation &rev)
{
	if (destination == RR->identity.address()) {
		SharedPtr<Network> n(network(rev.networkId()));
		if (!n) return;
		n->addCredential((void *)0,RR->identity,rev);
	} else {
		// TODO
		/*
		Packet outp(destination,RR->identity.address(),Packet::VERB_NETWORK_CREDENTIALS);
		outp.append((uint8_t)0x00);
		outp.append((uint16_t)0);
		outp.append((uint16_t)0);
		outp.append((uint16_t)1);
		rev.serialize(outp);
		outp.append((uint16_t)0);
		RR->sw->send((void *)0,outp,true);
		*/
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
		// TODO
		/*
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
		*/
	} // else we can't send an ERROR() in response to nothing, so discard
}

} // namespace ZeroTier

// C API --------------------------------------------------------------------------------------------------------------

extern "C" {

// These macros make the idiom of passing buffers to outside code via the API work properly even
// if the first address of Buf does not overlap with its data field, since the C++ standard does
// not absolutely guarantee this.
#define _ZT_PTRTOBUF(p) ((ZeroTier::Buf *)( ((uintptr_t)(p)) - ((uintptr_t)&(((ZeroTier::Buf *)0)->unsafeData[0])) ))
#define _ZT_BUFTOPTR(b) ((void *)(&((b)->unsafeData[0])))

void *ZT_getBuffer()
{
	// When external code requests a Buf, grab one from the pool (or freshly allocated)
	// and return it with its reference count left at zero. It's the responsibility of
	// external code to bring it back via freeBuffer() or one of the processX() calls.
	// When this occurs it's either sent back to the pool with Buf's delete operator or
	// wrapped in a SharedPtr<> to be passed into the core.
	try {
		return _ZT_BUFTOPTR(new ZeroTier::Buf());
	} catch ( ... ) {
		return nullptr; // can only happen on out of memory condition
	}
}

ZT_SDK_API void ZT_freeBuffer(void *b)
{
	if (b)
		delete _ZT_PTRTOBUF(b);
}

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

void ZT_Node_delete(ZT_Node *node,void *tPtr)
{
	try {
		reinterpret_cast<ZeroTier::Node *>(node)->shutdown(tPtr);
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
	int isZtBuffer,
	volatile int64_t *nextBackgroundTaskDeadline)
{
	try {
		ZeroTier::SharedPtr<ZeroTier::Buf> buf((isZtBuffer) ? _ZT_PTRTOBUF(packetData) : new ZeroTier::Buf(packetData,packetLength & ZT_BUF_MEM_MASK));
		return reinterpret_cast<ZeroTier::Node *>(node)->processWirePacket(tptr,now,localSocket,remoteAddress,buf,packetLength,nextBackgroundTaskDeadline);
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
	int isZtBuffer,
	volatile int64_t *nextBackgroundTaskDeadline)
{
	try {
		ZeroTier::SharedPtr<ZeroTier::Buf> buf((isZtBuffer) ? _ZT_PTRTOBUF(frameData) : new ZeroTier::Buf(frameData,frameLength & ZT_BUF_MEM_MASK));
		return reinterpret_cast<ZeroTier::Node *>(node)->processVirtualNetworkFrame(tptr,now,nwid,sourceMac,destMac,etherType,vlanId,buf,frameLength,nextBackgroundTaskDeadline);
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

enum ZT_ResultCode ZT_Node_addRoot(ZT_Node *node,void *tptr,const ZT_Identity *identity,const struct sockaddr_storage *bootstrap)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->addRoot(tptr,identity,bootstrap);
	} catch (std::bad_alloc &exc) {
		return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
	} catch ( ... ) {
		return ZT_RESULT_FATAL_ERROR_INTERNAL;
	}
}

enum ZT_ResultCode ZT_Node_removeRoot(ZT_Node *node,void *tptr,const ZT_Identity *identity)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->removeRoot(tptr,identity);
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

const ZT_Identity *ZT_Node_identity(ZT_Node *node)
{
	return (const ZT_Identity *)(&(reinterpret_cast<ZeroTier::Node *>(node)->identity()));
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

void ZT_Node_setInterfaceAddresses(ZT_Node *node,const ZT_InterfaceAddress *addrs,unsigned int addrCount)
{
	try {
		reinterpret_cast<ZeroTier::Node *>(node)->setInterfaceAddresses(addrs,addrCount);
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

void ZT_version(int *major,int *minor,int *revision,int *build)
{
	if (major)
		*major = ZEROTIER_VERSION_MAJOR;
	if (minor)
		*minor = ZEROTIER_VERSION_MINOR;
	if (revision)
		*revision = ZEROTIER_VERSION_REVISION;
	if (build)
		*build = ZEROTIER_VERSION_BUILD;
}

} // extern "C"
