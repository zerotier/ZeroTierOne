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
#include "Expect.hpp"
#include "VL1.hpp"
#include "VL2.hpp"
#include "Buf.hpp"

namespace ZeroTier {

namespace {

// Structure containing all the core objects for a ZeroTier node to reduce memory allocations.
struct _NodeObjects
{
	ZT_INLINE _NodeObjects(RuntimeEnvironment *const RR, void *const tPtr) :
		t(RR),
		expect(),
		vl2(RR),
		vl1(RR),
		sa(RR),
		topology(RR, tPtr)
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
	ZT_INLINE bool operator()(const SharedPtr<Peer> &a, const SharedPtr<Peer> &b) const
	{ return (a->address() < b->address()); }
};

} // anonymous namespace

Node::Node(void *uPtr, void *tPtr, const struct ZT_Node_Callbacks *callbacks, int64_t now) :
	m_RR(this),
	RR(&m_RR),
	m_objects(nullptr),
	m_cb(*callbacks),
	m_uPtr(uPtr),
	m_networks(),
	m_lastPeerPulse(0),
	m_lastHousekeepingRun(0),
	m_lastNetworkHousekeepingRun(0),
	m_now(now),
	m_online(false)
{
	ZT_SPEW("starting up...");

	// Load this node's identity.
	uint64_t idtmp[2];
	idtmp[0] = 0;
	idtmp[1] = 0;
	Vector<uint8_t> data(stateObjectGet(tPtr, ZT_STATE_OBJECT_IDENTITY_SECRET, idtmp));
	bool haveIdentity = false;
	if (!data.empty()) {
		data.push_back(0); // zero-terminate string
		if (RR->identity.fromString((const char *)data.data())) {
			RR->identity.toString(false, RR->publicIdentityStr);
			RR->identity.toString(true, RR->secretIdentityStr);
			haveIdentity = true;
			ZT_SPEW("loaded identity %s", RR->identity.toString().c_str());
		}
	}

	// Generate a new identity if we don't have one.
	if (!haveIdentity) {
		RR->identity.generate(Identity::C25519);
		RR->identity.toString(false, RR->publicIdentityStr);
		RR->identity.toString(true, RR->secretIdentityStr);
		idtmp[0] = RR->identity.address();
		idtmp[1] = 0;
		stateObjectPut(tPtr, ZT_STATE_OBJECT_IDENTITY_SECRET, idtmp, RR->secretIdentityStr, (unsigned int)strlen(RR->secretIdentityStr));
		stateObjectPut(tPtr, ZT_STATE_OBJECT_IDENTITY_PUBLIC, idtmp, RR->publicIdentityStr, (unsigned int)strlen(RR->publicIdentityStr));
		ZT_SPEW("no pre-existing identity found, created %s", RR->identity.toString().c_str());
	} else {
		idtmp[0] = RR->identity.address();
		idtmp[1] = 0;
		data = stateObjectGet(tPtr, ZT_STATE_OBJECT_IDENTITY_PUBLIC, idtmp);
		if ((data.empty()) || (memcmp(data.data(), RR->publicIdentityStr, strlen(RR->publicIdentityStr)) != 0))
			stateObjectPut(tPtr, ZT_STATE_OBJECT_IDENTITY_PUBLIC, idtmp, RR->publicIdentityStr, (unsigned int)strlen(RR->publicIdentityStr));
	}

	// 2X hash our identity private key(s) to obtain a symmetric key for encrypting
	// locally cached data at rest (as a defense in depth measure). This is not used
	// for any network level encryption or authentication.
	uint8_t tmph[ZT_SHA384_DIGEST_SIZE];
	RR->identity.hashWithPrivate(tmph);
	SHA384(tmph, tmph, ZT_SHA384_DIGEST_SIZE);
	RR->localCacheSymmetric.init(tmph);
	Utils::burn(tmph, ZT_SHA384_DIGEST_SIZE);

	// Generate a random sort order for privileged ports for use in NAT-t algorithms.
	for (unsigned int i = 0;i < 1023;++i)
		RR->randomPrivilegedPortOrder[i] = (uint16_t)(i + 1);
	for (unsigned int i = 0;i < 512;++i) {
		uint64_t rn = Utils::random();
		const unsigned int a = (unsigned int)rn % 1023;
		const unsigned int b = (unsigned int)(rn >> 32U) % 1023;
		if (a != b) {
			const uint16_t tmp = RR->randomPrivilegedPortOrder[a];
			RR->randomPrivilegedPortOrder[a] = RR->randomPrivilegedPortOrder[b];
			RR->randomPrivilegedPortOrder[b] = tmp;
		}
	}

	// This constructs all the components of the ZeroTier core within a single contiguous memory container,
	// which reduces memory fragmentation and may improve cache locality.
	ZT_SPEW("initializing subsystem objects...");
	m_objects = new _NodeObjects(RR, tPtr);
	ZT_SPEW("node initialized!");

	postEvent(tPtr, ZT_EVENT_UP);
}

Node::~Node()
{
	ZT_SPEW("node destructor run");
	m_networks_l.lock();
	m_networks_l.unlock();
	m_networks.clear();
	m_networks_l.lock();
	m_networks_l.unlock();

	if (m_objects)
		delete (_NodeObjects *)m_objects;

	// Let go of cached Buf objects. If other nodes happen to be running in this
	// same process space new Bufs will be allocated as needed, but this is almost
	// never the case. Calling this here saves RAM if we are running inside something
	// that wants to keep running after tearing down its ZeroTier core instance.
	Buf::freePool();
}

void Node::shutdown(void *tPtr)
{
	ZT_SPEW("explicit shutdown() called");
	postEvent(tPtr, ZT_EVENT_DOWN);
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
	m_now = now;
	RR->vl1->onRemotePacket(tPtr, localSocket, (remoteAddress) ? InetAddress::NIL : *asInetAddress(remoteAddress), packetData, packetLength);
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
	m_now = now;
	SharedPtr<Network> nw(this->network(nwid));
	if (nw) {
		RR->vl2->onLocalEthernet(tPtr, nw, MAC(sourceMac), MAC(destMac), etherType, vlanId, frameData, frameLength);
		return ZT_RESULT_OK;
	} else {
		return ZT_RESULT_ERROR_NETWORK_NOT_FOUND;
	}
}

ZT_ResultCode Node::processBackgroundTasks(void *tPtr, int64_t now, volatile int64_t *nextBackgroundTaskDeadline)
{
	m_now = now;
	Mutex::Lock bl(m_backgroundTasksLock);

	try {
		// Call peer pulse() method of all peers every ZT_PEER_PULSE_INTERVAL.
		if ((now - m_lastPeerPulse) >= ZT_PEER_PULSE_INTERVAL) {
			m_lastPeerPulse = now;
			ZT_SPEW("running pulse() on each peer...");
			try {
				Vector< SharedPtr<Peer> > allPeers, rootPeers;
				RR->topology->getAllPeers(allPeers, rootPeers);

				bool online = false;
				for(Vector< SharedPtr<Peer> >::iterator p(allPeers.begin());p!=allPeers.end();++p) {
					const bool isRoot = std::find(rootPeers.begin(), rootPeers.end(), *p) != rootPeers.end();
					(*p)->pulse(tPtr, now, isRoot);
					online |= ((isRoot || rootPeers.empty()) && (*p)->directlyConnected(now));
				}

				RR->topology->rankRoots();

				if (m_online.exchange(online) != online)
					postEvent(tPtr, online ? ZT_EVENT_ONLINE : ZT_EVENT_OFFLINE);
			} catch (...) {
				return ZT_RESULT_FATAL_ERROR_INTERNAL;
			}
		}

		// Perform network housekeeping and possibly request new certs and configs every ZT_NETWORK_HOUSEKEEPING_PERIOD.
		if ((now - m_lastNetworkHousekeepingRun) >= ZT_NETWORK_HOUSEKEEPING_PERIOD) {
			m_lastHousekeepingRun = now;
			ZT_SPEW("running networking housekeeping...");
			RWMutex::RLock l(m_networks_l);
			for (Map<uint64_t, SharedPtr<Network> >::const_iterator i(m_networks.begin());i != m_networks.end();++i) {
				i->second->doPeriodicTasks(tPtr, now);
			}
		}

		// Clean up other stuff every ZT_HOUSEKEEPING_PERIOD.
		if ((now - m_lastHousekeepingRun) >= ZT_HOUSEKEEPING_PERIOD) {
			m_lastHousekeepingRun = now;
			ZT_SPEW("running housekeeping...");

			// Clean up any old local controller auth memoizations. This is an
			// optimization for network controllers to know whether to accept
			// or trust nodes without doing an extra cert check.
			m_localControllerAuthorizations_l.lock();
			for (Map<p_LocalControllerAuth, int64_t>::iterator i(m_localControllerAuthorizations.begin());i != m_localControllerAuthorizations.end();) { // NOLINT(hicpp-use-auto,modernize-use-auto)
				if ((i->second - now) > (ZT_NETWORK_AUTOCONF_DELAY * 3))
					m_localControllerAuthorizations.erase(i++);
				else ++i;
			}
			m_localControllerAuthorizations_l.unlock();

			RR->topology->doPeriodicTasks(tPtr, now);
			RR->sa->clean(now);
		}

		*nextBackgroundTaskDeadline = now + ZT_TIMER_TASK_INTERVAL;
	} catch (...) {
		return ZT_RESULT_FATAL_ERROR_INTERNAL;
	}

	return ZT_RESULT_OK;
}

ZT_ResultCode Node::join(uint64_t nwid, const ZT_Fingerprint *controllerFingerprint, void *uptr, void *tptr)
{
	Fingerprint fp;
	if (controllerFingerprint) {
		fp = *controllerFingerprint;
		ZT_SPEW("joining network %.16llx with fingerprint %s",nwid,fp.toString().c_str());
	} else {
		ZT_SPEW("joining network %.16llx",nwid);
	}

	RWMutex::Lock l(m_networks_l);
	SharedPtr<Network> &nw = m_networks[nwid];
	if (nw)
		return ZT_RESULT_OK;
	nw.set(new Network(RR, tptr, nwid, fp, uptr, nullptr));

	return ZT_RESULT_OK;
}

ZT_ResultCode Node::leave(uint64_t nwid, void **uptr, void *tptr)
{
	ZT_SPEW("leaving network %.16llx",nwid);
	ZT_VirtualNetworkConfig ctmp;

	m_networks_l.lock();
	Map<uint64_t, SharedPtr<Network> >::iterator nwi(m_networks.find(nwid)); // NOLINT(hicpp-use-auto,modernize-use-auto)
	if (nwi == m_networks.end()) {
		m_networks_l.unlock();
		return ZT_RESULT_OK;
	}
	SharedPtr<Network> nw(nwi->second);
	m_networks.erase(nwi);
	m_networks_l.unlock();

	if (uptr)
		*uptr = *nw->userPtr();
	nw->externalConfig(&ctmp);

	RR->node->configureVirtualNetworkPort(tptr, nwid, uptr, ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY, &ctmp);

	nw->destroy();
	nw.zero();

	uint64_t tmp[2];
	tmp[0] = nwid;
	tmp[1] = 0;
	RR->node->stateObjectDelete(tptr, ZT_STATE_OBJECT_NETWORK_CONFIG, tmp);

	return ZT_RESULT_OK;
}

ZT_ResultCode Node::multicastSubscribe(void *tPtr, uint64_t nwid, uint64_t multicastGroup, unsigned long multicastAdi)
{
	ZT_SPEW("multicast subscribe to %s:%lu",MAC(multicastGroup).toString().c_str(),multicastAdi);
	const SharedPtr<Network> nw(this->network(nwid));
	if (nw) {
		nw->multicastSubscribe(tPtr, MulticastGroup(MAC(multicastGroup), (uint32_t)(multicastAdi & 0xffffffff)));
		return ZT_RESULT_OK;
	} else return ZT_RESULT_ERROR_NETWORK_NOT_FOUND;
}

ZT_ResultCode Node::multicastUnsubscribe(uint64_t nwid, uint64_t multicastGroup, unsigned long multicastAdi)
{
	ZT_SPEW("multicast unsubscribe from %s:%lu",MAC(multicastGroup).toString().c_str(),multicastAdi);
	const SharedPtr<Network> nw(this->network(nwid));
	if (nw) {
		nw->multicastUnsubscribe(MulticastGroup(MAC(multicastGroup), (uint32_t)(multicastAdi & 0xffffffff)));
		return ZT_RESULT_OK;
	} else return ZT_RESULT_ERROR_NETWORK_NOT_FOUND;
}

ZT_ResultCode Node::addRoot(void *tPtr, const ZT_Identity *id)
{
	return (RR->topology->addRoot(tPtr, *reinterpret_cast<const Identity *>(id))) ? ZT_RESULT_OK : ZT_RESULT_ERROR_BAD_PARAMETER;
}

ZT_ResultCode Node::removeRoot(void *tPtr, const uint64_t address)
{
	RR->topology->removeRoot(tPtr, Address(address));
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
	status->online = m_online ? 1 : 0;
}

ZT_PeerList *Node::peers() const
{
	Vector<SharedPtr<Peer> > peers;
	RR->topology->getAllPeers(peers);
	std::sort(peers.begin(), peers.end(), _sortPeerPtrsByAddress());

	const unsigned int bufSize =
		sizeof(ZT_PeerList) +
		(sizeof(ZT_Peer) * peers.size()) +
		((sizeof(ZT_Path) * ZT_MAX_PEER_NETWORK_PATHS) * peers.size()) +
		(sizeof(Identity) * peers.size()) +
		(ZT_LOCATOR_MARSHAL_SIZE_MAX * peers.size());
	char *buf = (char *)malloc(bufSize);
	if (!buf)
		return nullptr;
	Utils::zero(buf, bufSize);
	ZT_PeerList *pl = reinterpret_cast<ZT_PeerList *>(buf);
	buf += sizeof(ZT_PeerList);
	pl->peers = reinterpret_cast<ZT_Peer *>(buf);
	buf += sizeof(ZT_Peer) * peers.size();
	ZT_Path *peerPath = reinterpret_cast<ZT_Path *>(buf);
	buf += (sizeof(ZT_Path) * ZT_MAX_PEER_NETWORK_PATHS) * peers.size();
	Identity *identities = reinterpret_cast<Identity *>(buf);
	buf += sizeof(Identity) * peers.size();
	uint8_t *locatorBuf = reinterpret_cast<uint8_t *>(buf);

	const int64_t now = m_now;

	pl->peerCount = 0;
	for (Vector<SharedPtr<Peer> >::iterator pi(peers.begin());pi != peers.end();++pi) {
		ZT_Peer *const p = pl->peers + pl->peerCount;

		p->address = (*pi)->address().toInt();
		identities[pl->peerCount] = (*pi)->identity(); // need to make a copy in case peer gets deleted
		p->identity = identities + pl->peerCount;
		p->fingerprint.address = p->address;
		Utils::copy<ZT_FINGERPRINT_HASH_SIZE>(p->fingerprint.hash, (*pi)->identity().fingerprint().hash);
		if ((*pi)->remoteVersionKnown()) {
			p->versionMajor = (int)(*pi)->remoteVersionMajor();
			p->versionMinor = (int)(*pi)->remoteVersionMinor();
			p->versionRev = (int)(*pi)->remoteVersionRevision();
		} else {
			p->versionMajor = -1;
			p->versionMinor = -1;
			p->versionRev = -1;
		}
		p->latency = (*pi)->latency();
		p->root = RR->topology->isRoot((*pi)->identity()) ? 1 : 0;

		p->networkCount = 0;
		// TODO: enumerate network memberships

		Vector< SharedPtr<Path> > paths;
		(*pi)->getAllPaths(paths);
		p->pathCount = (unsigned int)paths.size();
		p->paths = peerPath;
		for (Vector<SharedPtr<Path> >::iterator path(paths.begin());path != paths.end();++path) {
			ZT_Path *const pp = peerPath++;
			pp->endpoint.type = ZT_ENDPOINT_TYPE_IP_UDP; // only type supported right now
			Utils::copy<sizeof(sockaddr_storage)>(&pp->endpoint.value.ss, &((*path)->address().as.ss));
			pp->lastSend = (*path)->lastOut();
			pp->lastReceive = (*path)->lastIn();
			pp->alive = (*path)->alive(now) ? 1 : 0;
			pp->preferred = (p->pathCount == 0) ? 1 : 0;
		}

		const SharedPtr<const Locator> loc((*pi)->locator());
		if (loc) {
			const int ls = loc->marshal(locatorBuf);
			if (ls > 0) {
				p->locatorSize = (unsigned int)ls;
				p->locator = locatorBuf;
				locatorBuf += ls;
			}
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
	return nullptr;
}

ZT_VirtualNetworkList *Node::networks() const
{
	RWMutex::RLock l(m_networks_l);

	char *const buf = (char *)::malloc(sizeof(ZT_VirtualNetworkList) + (sizeof(ZT_VirtualNetworkConfig) * m_networks.size()));
	if (!buf)
		return nullptr;
	ZT_VirtualNetworkList *nl = (ZT_VirtualNetworkList *)buf; // NOLINT(modernize-use-auto,hicpp-use-auto)
	nl->networks = (ZT_VirtualNetworkConfig *)(buf + sizeof(ZT_VirtualNetworkList));

	nl->networkCount = 0;
	for (Map<uint64_t, SharedPtr<Network> >::const_iterator i(m_networks.begin());i != m_networks.end();++i) // NOLINT(modernize-use-auto,modernize-loop-convert,hicpp-use-auto)
		i->second->externalConfig(&(nl->networks[nl->networkCount++]));

	return nl;
}

void Node::setNetworkUserPtr(uint64_t nwid, void *ptr)
{
	SharedPtr<Network> nw(network(nwid));
	if (nw)
		*(nw->userPtr()) = ptr;
}

void Node::setInterfaceAddresses(const ZT_InterfaceAddress *addrs, unsigned int addrCount)
{
	Mutex::Lock _l(m_localInterfaceAddresses_m);
	m_localInterfaceAddresses.clear();
	for (unsigned int i = 0;i < addrCount;++i) {
		bool dupe = false;
		for (unsigned int j = 0;j < i;++j) {
			if (*(reinterpret_cast<const InetAddress *>(&addrs[j].address)) == *(reinterpret_cast<const InetAddress *>(&addrs[i].address))) {
				dupe = true;
				break;
			}
		}
		if (!dupe)
			m_localInterfaceAddresses.push_back(addrs[i]);
	}
}

int Node::sendUserMessage(void *tptr, uint64_t dest, uint64_t typeId, const void *data, unsigned int len)
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
	} catch (...) {}
	return 0;
}

void Node::setController(void *networkControllerInstance)
{
	RR->localNetworkController = reinterpret_cast<NetworkController *>(networkControllerInstance);
	if (networkControllerInstance)
		RR->localNetworkController->init(RR->identity, this);
}

// Methods used only within the core ----------------------------------------------------------------------------------

Vector<uint8_t> Node::stateObjectGet(void *const tPtr, ZT_StateObjectType type, const uint64_t id[2])
{
	Vector<uint8_t> r;
	if (m_cb.stateGetFunction) {
		void *data = nullptr;
		void (*freeFunc)(void *) = nullptr;
		int l = m_cb.stateGetFunction(
			reinterpret_cast<ZT_Node *>(this),
			m_uPtr,
			tPtr,
			type,
			id,
			&data,
			&freeFunc);
		if ((l > 0) && (data) && (freeFunc)) {
			r.assign(reinterpret_cast<const uint8_t *>(data), reinterpret_cast<const uint8_t *>(data) + l);
			freeFunc(data);
		}
	}
	return r;
}

bool Node::shouldUsePathForZeroTierTraffic(void *tPtr, const Identity &id, const int64_t localSocket, const InetAddress &remoteAddress)
{
	{
		RWMutex::RLock l(m_networks_l);
		for (Map<uint64_t, SharedPtr<Network> >::iterator i(m_networks.begin());i != m_networks.end();++i) { // NOLINT(hicpp-use-auto,modernize-use-auto,modernize-loop-convert)
			for (unsigned int k = 0, j = i->second->config().staticIpCount;k < j;++k) {
				if (i->second->config().staticIps[k].containsAddress(remoteAddress))
					return false;
			}
		}
	}

	if (m_cb.pathCheckFunction) {
		return (m_cb.pathCheckFunction(
			reinterpret_cast<ZT_Node *>(this),
			m_uPtr,
			tPtr,
			id.address().toInt(),
			(const ZT_Identity *)&id,
			localSocket,
			reinterpret_cast<const struct sockaddr_storage *>(&remoteAddress)) != 0);
	}

	return true;
}

bool Node::externalPathLookup(void *tPtr, const Identity &id, int family, InetAddress &addr)
{
	if (m_cb.pathLookupFunction) {
		return (m_cb.pathLookupFunction(
			reinterpret_cast<ZT_Node *>(this),
			m_uPtr,
			tPtr,
			id.address().toInt(),
			reinterpret_cast<const ZT_Identity *>(&id),
			family,
			reinterpret_cast<sockaddr_storage *>(&addr)) == ZT_RESULT_OK);
	}
	return false;
}

bool Node::localControllerHasAuthorized(const int64_t now, const uint64_t nwid, const Address &addr) const
{
	m_localControllerAuthorizations_l.lock();
	const int64_t *const at = m_localControllerAuthorizations.get(p_LocalControllerAuth(nwid, addr));
	m_localControllerAuthorizations_l.unlock();
	if (at)
		return ((now - *at) < (ZT_NETWORK_AUTOCONF_DELAY * 3));
	return false;
}

// Implementation of NetworkController::Sender ------------------------------------------------------------------------

void Node::ncSendConfig(uint64_t nwid, uint64_t requestPacketId, const Address &destination, const NetworkConfig &nc, bool sendLegacyFormatConfig)
{
	m_localControllerAuthorizations_l.lock();
	m_localControllerAuthorizations[p_LocalControllerAuth(nwid, destination)] = now();
	m_localControllerAuthorizations_l.unlock();

	if (destination == RR->identity.address()) {
		SharedPtr<Network> n(network(nwid));
		if (!n)
			return;
		n->setConfiguration((void *)0, nc, true);
	} else {
		Dictionary dconf;
		if (nc.toDictionary(dconf)) {
			uint64_t configUpdateId = Utils::random();
			if (!configUpdateId)
				++configUpdateId;

			Vector<uint8_t> ddata;
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

void Node::ncSendRevocation(const Address &destination, const Revocation &rev)
{
	if (destination == RR->identity.address()) {
		SharedPtr<Network> n(network(rev.networkId()));
		if (!n) return;
		n->addCredential(nullptr, RR->identity, rev);
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

void Node::ncSendError(uint64_t nwid, uint64_t requestPacketId, const Address &destination, NetworkController::ErrorCode errorCode)
{
	if (destination == RR->identity.address()) {
		SharedPtr<Network> n(network(nwid));
		if (!n) return;
		switch (errorCode) {
			case NetworkController::NC_ERROR_OBJECT_NOT_FOUND:
			case NetworkController::NC_ERROR_INTERNAL_SERVER_ERROR:
				n->setNotFound();
				break;
			case NetworkController::NC_ERROR_ACCESS_DENIED:
				n->setAccessDenied();
				break;

			default:
				break;
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
	} catch (...) {
		return nullptr; // can only happen on out of memory condition
	}
}

void ZT_freeBuffer(void *b)
{
	if (b)
		delete _ZT_PTRTOBUF(b);
}

void ZT_freeQueryResult(void *qr)
{
	if (qr)
		free(qr);
}

enum ZT_ResultCode ZT_Node_new(ZT_Node **node, void *uptr, void *tptr, const struct ZT_Node_Callbacks *callbacks, int64_t now)
{
	*node = (ZT_Node *)0;
	try {
		*node = reinterpret_cast<ZT_Node *>(new ZeroTier::Node(uptr, tptr, callbacks, now));
		return ZT_RESULT_OK;
	} catch (std::bad_alloc &exc) {
		return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
	} catch (std::runtime_error &exc) {
		return ZT_RESULT_FATAL_ERROR_DATA_STORE_FAILED;
	} catch (...) {
		return ZT_RESULT_FATAL_ERROR_INTERNAL;
	}
}

void ZT_Node_delete(ZT_Node *node, void *tPtr)
{
	try {
		reinterpret_cast<ZeroTier::Node *>(node)->shutdown(tPtr);
		delete (reinterpret_cast<ZeroTier::Node *>(node));
	} catch (...) {}
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
		ZeroTier::SharedPtr<ZeroTier::Buf> buf((isZtBuffer) ? _ZT_PTRTOBUF(packetData) : new ZeroTier::Buf(packetData, packetLength & ZT_BUF_MEM_MASK));
		return reinterpret_cast<ZeroTier::Node *>(node)->processWirePacket(tptr, now, localSocket, remoteAddress, buf, packetLength, nextBackgroundTaskDeadline);
	} catch (std::bad_alloc &exc) {
		return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
	} catch (...) {
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
		ZeroTier::SharedPtr<ZeroTier::Buf> buf((isZtBuffer) ? _ZT_PTRTOBUF(frameData) : new ZeroTier::Buf(frameData, frameLength & ZT_BUF_MEM_MASK));
		return reinterpret_cast<ZeroTier::Node *>(node)->processVirtualNetworkFrame(tptr, now, nwid, sourceMac, destMac, etherType, vlanId, buf, frameLength, nextBackgroundTaskDeadline);
	} catch (std::bad_alloc &exc) {
		return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
	} catch (...) {
		return ZT_RESULT_FATAL_ERROR_INTERNAL;
	}
}

enum ZT_ResultCode ZT_Node_processBackgroundTasks(ZT_Node *node, void *tptr, int64_t now, volatile int64_t *nextBackgroundTaskDeadline)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->processBackgroundTasks(tptr, now, nextBackgroundTaskDeadline);
	} catch (std::bad_alloc &exc) {
		return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
	} catch (...) {
		return ZT_RESULT_FATAL_ERROR_INTERNAL;
	}
}

enum ZT_ResultCode ZT_Node_join(ZT_Node *node, uint64_t nwid, const ZT_Fingerprint *controllerFingerprint, void *uptr, void *tptr)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->join(nwid, controllerFingerprint, uptr, tptr);
	} catch (std::bad_alloc &exc) {
		return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
	} catch (...) {
		return ZT_RESULT_FATAL_ERROR_INTERNAL;
	}
}

enum ZT_ResultCode ZT_Node_leave(ZT_Node *node, uint64_t nwid, void **uptr, void *tptr)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->leave(nwid, uptr, tptr);
	} catch (std::bad_alloc &exc) {
		return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
	} catch (...) {
		return ZT_RESULT_FATAL_ERROR_INTERNAL;
	}
}

enum ZT_ResultCode ZT_Node_multicastSubscribe(ZT_Node *node, void *tptr, uint64_t nwid, uint64_t multicastGroup, unsigned long multicastAdi)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->multicastSubscribe(tptr, nwid, multicastGroup, multicastAdi);
	} catch (std::bad_alloc &exc) {
		return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
	} catch (...) {
		return ZT_RESULT_FATAL_ERROR_INTERNAL;
	}
}

enum ZT_ResultCode ZT_Node_multicastUnsubscribe(ZT_Node *node, uint64_t nwid, uint64_t multicastGroup, unsigned long multicastAdi)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->multicastUnsubscribe(nwid, multicastGroup, multicastAdi);
	} catch (std::bad_alloc &exc) {
		return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
	} catch (...) {
		return ZT_RESULT_FATAL_ERROR_INTERNAL;
	}
}

enum ZT_ResultCode ZT_Node_addRoot(ZT_Node *node, void *tptr, const ZT_Identity *id)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->addRoot(tptr, id);
	} catch (std::bad_alloc &exc) {
		return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
	} catch (...) {
		return ZT_RESULT_FATAL_ERROR_INTERNAL;
	}
}

enum ZT_ResultCode ZT_Node_removeRoot(ZT_Node *node, void *tptr, const uint64_t address)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->removeRoot(tptr, address);
	} catch (std::bad_alloc &exc) {
		return ZT_RESULT_FATAL_ERROR_OUT_OF_MEMORY;
	} catch (...) {
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

void ZT_Node_status(ZT_Node *node, ZT_NodeStatus *status)
{
	try {
		reinterpret_cast<ZeroTier::Node *>(node)->status(status);
	} catch (...) {}
}

ZT_PeerList *ZT_Node_peers(ZT_Node *node)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->peers();
	} catch (...) {
		return (ZT_PeerList *)0;
	}
}

ZT_VirtualNetworkConfig *ZT_Node_networkConfig(ZT_Node *node, uint64_t nwid)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->networkConfig(nwid);
	} catch (...) {
		return (ZT_VirtualNetworkConfig *)0;
	}
}

ZT_VirtualNetworkList *ZT_Node_networks(ZT_Node *node)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->networks();
	} catch (...) {
		return (ZT_VirtualNetworkList *)0;
	}
}

void ZT_Node_setNetworkUserPtr(ZT_Node *node, uint64_t nwid, void *ptr)
{
	try {
		reinterpret_cast<ZeroTier::Node *>(node)->setNetworkUserPtr(nwid, ptr);
	} catch (...) {}
}

void ZT_Node_setInterfaceAddresses(ZT_Node *node, const ZT_InterfaceAddress *addrs, unsigned int addrCount)
{
	try {
		reinterpret_cast<ZeroTier::Node *>(node)->setInterfaceAddresses(addrs, addrCount);
	} catch (...) {}
}

int ZT_Node_sendUserMessage(ZT_Node *node, void *tptr, uint64_t dest, uint64_t typeId, const void *data, unsigned int len)
{
	try {
		return reinterpret_cast<ZeroTier::Node *>(node)->sendUserMessage(tptr, dest, typeId, data, len);
	} catch (...) {
		return 0;
	}
}

void ZT_Node_setController(ZT_Node *node, void *networkControllerInstance)
{
	try {
		reinterpret_cast<ZeroTier::Node *>(node)->setController(networkControllerInstance);
	} catch (...) {}
}

void ZT_version(int *major, int *minor, int *revision, int *build)
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
