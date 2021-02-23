/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
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
	ZT_INLINE _NodeObjects(RuntimeEnvironment *const RR, void *const tPtr, const int64_t now) :
		t(RR),
		expect(),
		vl2(RR),
		vl1(RR),
		sa(RR),
		topology(RR, tPtr, now)
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

} // anonymous namespace

Node::Node(
	void *uPtr,
	void *tPtr,
	const struct ZT_Node_Callbacks *callbacks,
	int64_t now) :
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
	Vector< uint8_t > data(stateObjectGet(tPtr, ZT_STATE_OBJECT_IDENTITY_SECRET, idtmp, 0));
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
		stateObjectPut(tPtr, ZT_STATE_OBJECT_IDENTITY_SECRET, idtmp, 1, RR->secretIdentityStr, (unsigned int)strlen(RR->secretIdentityStr));
		stateObjectPut(tPtr, ZT_STATE_OBJECT_IDENTITY_PUBLIC, idtmp, 1, RR->publicIdentityStr, (unsigned int)strlen(RR->publicIdentityStr));
		ZT_SPEW("no pre-existing identity found, created %s", RR->identity.toString().c_str());
	} else {
		idtmp[0] = RR->identity.address();
		idtmp[1] = 0;
		data = stateObjectGet(tPtr, ZT_STATE_OBJECT_IDENTITY_PUBLIC, idtmp, 1);
		if ((data.empty()) || (memcmp(data.data(), RR->publicIdentityStr, strlen(RR->publicIdentityStr)) != 0)) {
			stateObjectPut(tPtr, ZT_STATE_OBJECT_IDENTITY_PUBLIC, idtmp, 1, RR->publicIdentityStr, (unsigned int)strlen(RR->publicIdentityStr));
		}
	}

	// Create a secret key for encrypting local data at rest.
	uint8_t tmph[ZT_SHA384_DIGEST_SIZE];
	RR->identity.hashWithPrivate(tmph);
	SHA384(tmph, tmph, ZT_SHA384_DIGEST_SIZE);
	RR->localCacheSymmetric.init(tmph);
	Utils::burn(tmph, ZT_SHA384_DIGEST_SIZE);

	// Generate a random sort order for privileged ports for use in NAT-t algorithms.
	for (unsigned int i = 0; i < 1023; ++i)
		RR->randomPrivilegedPortOrder[i] = (uint16_t)(i + 1);
	for (unsigned int i = 0; i < 512; ++i) {
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
	m_objects = new _NodeObjects(RR, tPtr, now);
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
	SharedPtr< Buf > &packetData,
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
	SharedPtr< Buf > &frameData,
	unsigned int frameLength,
	volatile int64_t *nextBackgroundTaskDeadline)
{
	m_now = now;
	SharedPtr< Network > nw(this->network(nwid));
	if (nw) {
		RR->vl2->onLocalEthernet(tPtr, nw, MAC(sourceMac), MAC(destMac), etherType, vlanId, frameData, frameLength);
		return ZT_RESULT_OK;
	} else {
		return ZT_RESULT_ERROR_NETWORK_NOT_FOUND;
	}
}

ZT_ResultCode Node::processBackgroundTasks(
	void *tPtr,
	int64_t now,
	volatile int64_t *nextBackgroundTaskDeadline)
{
	m_now = now;
	Mutex::Lock bl(m_backgroundTasksLock);

	try {
		// Call peer pulse() method of all peers every ZT_PEER_PULSE_INTERVAL.
		if ((now - m_lastPeerPulse) >= ZT_PEER_PULSE_INTERVAL) {
			m_lastPeerPulse = now;
			ZT_SPEW("running pulse() on each peer...");
			try {
				Vector< SharedPtr< Peer > > allPeers, rootPeers;
				RR->topology->allPeers(allPeers, rootPeers);

				bool online = false;
				for (Vector< SharedPtr< Peer > >::iterator p(allPeers.begin()); p != allPeers.end(); ++p) {
					const bool isRoot = std::find(rootPeers.begin(), rootPeers.end(), *p) != rootPeers.end();
					(*p)->pulse(tPtr, now, isRoot);
					online |= ((isRoot || rootPeers.empty()) && (*p)->directlyConnected(now));
				}

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
			for (Map< uint64_t, SharedPtr< Network > >::const_iterator i(m_networks.begin()); i != m_networks.end(); ++i) {
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
			for (Map< p_LocalControllerAuth, int64_t >::iterator i(m_localControllerAuthorizations.begin()); i != m_localControllerAuthorizations.end();) { // NOLINT(hicpp-use-auto,modernize-use-auto)
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

ZT_ResultCode Node::join(
	uint64_t nwid,
	const ZT_Fingerprint *controllerFingerprint,
	void *uptr,
	void *tptr)
{
	Fingerprint fp;
	if (controllerFingerprint) {
		fp = *controllerFingerprint;
		ZT_SPEW("joining network %.16llx with fingerprint %s", nwid, fp.toString().c_str());
	} else {
		ZT_SPEW("joining network %.16llx", nwid);
	}

	RWMutex::Lock l(m_networks_l);
	SharedPtr< Network > &nw = m_networks[nwid];
	if (nw)
		return ZT_RESULT_OK;
	nw.set(new Network(RR, tptr, nwid, fp, uptr, nullptr));

	return ZT_RESULT_OK;
}

ZT_ResultCode Node::leave(
	uint64_t nwid,
	void **uptr,
	void *tptr)
{
	ZT_SPEW("leaving network %.16llx", nwid);
	ZT_VirtualNetworkConfig ctmp;

	m_networks_l.lock();
	Map< uint64_t, SharedPtr< Network > >::iterator nwi(m_networks.find(nwid)); // NOLINT(hicpp-use-auto,modernize-use-auto)
	if (nwi == m_networks.end()) {
		m_networks_l.unlock();
		return ZT_RESULT_OK;
	}
	SharedPtr< Network > nw(nwi->second);
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
	RR->node->stateObjectDelete(tptr, ZT_STATE_OBJECT_NETWORK_CONFIG, tmp, 1);

	return ZT_RESULT_OK;
}

ZT_ResultCode Node::multicastSubscribe(
	void *tPtr,
	uint64_t nwid,
	uint64_t multicastGroup,
	unsigned long multicastAdi)
{
	ZT_SPEW("multicast subscribe to %s:%lu", MAC(multicastGroup).toString().c_str(), multicastAdi);
	const SharedPtr< Network > nw(this->network(nwid));
	if (nw) {
		nw->multicastSubscribe(tPtr, MulticastGroup(MAC(multicastGroup), (uint32_t)(multicastAdi & 0xffffffff)));
		return ZT_RESULT_OK;
	} else return ZT_RESULT_ERROR_NETWORK_NOT_FOUND;
}

ZT_ResultCode Node::multicastUnsubscribe(
	uint64_t nwid,
	uint64_t multicastGroup,
	unsigned long multicastAdi)
{
	ZT_SPEW("multicast unsubscribe from %s:%lu", MAC(multicastGroup).toString().c_str(), multicastAdi);
	const SharedPtr< Network > nw(this->network(nwid));
	if (nw) {
		nw->multicastUnsubscribe(MulticastGroup(MAC(multicastGroup), (uint32_t)(multicastAdi & 0xffffffff)));
		return ZT_RESULT_OK;
	} else return ZT_RESULT_ERROR_NETWORK_NOT_FOUND;
}

uint64_t Node::address() const
{ return RR->identity.address().toInt(); }

void Node::status(ZT_NodeStatus *status) const
{
	status->address = RR->identity.address().toInt();
	status->identity = reinterpret_cast<const ZT_Identity *>(&RR->identity);
	status->publicIdentity = RR->publicIdentityStr;
	status->secretIdentity = RR->secretIdentityStr;
	status->online = m_online ? 1 : 0;
}

struct p_ZT_PeerListPrivate : public ZT_PeerList
{
	// Actual containers for the memory, hidden from external users.
	std::vector< ZT_Peer > p_peers;
	std::list< std::vector<ZT_Path> > p_paths;
	std::list< Identity > p_identities;
	std::list< Blob<ZT_LOCATOR_MARSHAL_SIZE_MAX> > p_locators;
};
static void p_peerListFreeFunction(const void *pl)
{
	if (pl)
		delete reinterpret_cast<p_ZT_PeerListPrivate *>(const_cast<void *>(pl));
}

struct p_sortPeerPtrsByAddress
{
	ZT_INLINE bool operator()(const SharedPtr< Peer > &a, const SharedPtr< Peer > &b) const noexcept
	{ return (a->address() < b->address()); }
};

ZT_PeerList *Node::peers() const
{
	p_ZT_PeerListPrivate *pl = nullptr;
	try {
		pl = new p_ZT_PeerListPrivate;
		pl->freeFunction = p_peerListFreeFunction;

		Vector< SharedPtr< Peer > > peers, rootPeers;
		RR->topology->allPeers(peers, rootPeers);
		std::sort(peers.begin(), peers.end(), p_sortPeerPtrsByAddress());
		std::sort(rootPeers.begin(), rootPeers.end());
		int64_t now = m_now;

		for (Vector< SharedPtr< Peer > >::iterator pi(peers.begin()); pi != peers.end(); ++pi) {
			pl->p_peers.push_back(ZT_Peer());
			ZT_Peer &p = pl->p_peers.back();
			Peer &pp = **pi;

			p.address = pp.address();
			pl->p_identities.push_back(pp.identity());
			p.identity = reinterpret_cast<const ZT_Identity *>(&(pl->p_identities.back()));
			p.fingerprint = &(pl->p_identities.back().fingerprint());
			if (pp.remoteVersionKnown()) {
				p.versionMajor = (int)pp.remoteVersionMajor();
				p.versionMinor = (int)pp.remoteVersionMinor();
				p.versionRev = (int)pp.remoteVersionRevision();
				p.versionProto = (int)pp.remoteVersionProtocol();
			} else {
				p.versionMajor = -1;
				p.versionMinor = -1;
				p.versionRev = -1;
				p.versionProto = -1;
			}
			p.latency = pp.latency();
			p.root = std::binary_search(rootPeers.begin(), rootPeers.end(), *pi) ? 1 : 0;

			p.networks = nullptr;
			p.networkCount = 0; // TODO: networks this peer belongs to

			Vector< SharedPtr<Path> > ztPaths;
			pp.getAllPaths(ztPaths);
			if (ztPaths.empty()) {
				pl->p_paths.push_back(std::vector< ZT_Path >());
				std::vector< ZT_Path > &apiPaths = pl->p_paths.back();
				apiPaths.resize(ztPaths.size());
				for (unsigned long i = 0; i < (unsigned long)ztPaths.size(); ++i) {
					SharedPtr< Path > &ztp = ztPaths[i];
					ZT_Path &apip = apiPaths[i];
					apip.endpoint.type = ZT_ENDPOINT_TYPE_IP_UDP;
					Utils::copy< sizeof(struct sockaddr_storage) >(&(apip.endpoint.value.ss), &(ztp->address().as.ss));
					apip.lastSend = ztp->lastOut();
					apip.lastReceive = ztp->lastIn();
					apip.alive = ztp->alive(now) ? 1 : 0;
					apip.preferred = (i == 0) ? 1 : 0;
				}
				p.paths = apiPaths.data();
				p.pathCount = (unsigned int)apiPaths.size();
			} else {
				p.paths = nullptr;
				p.pathCount = 0;
			}

			const SharedPtr< const Locator > loc(pp.locator());
			if (loc) {
				pl->p_locators.push_back(Blob< ZT_LOCATOR_MARSHAL_SIZE_MAX >());
				Blob< ZT_LOCATOR_MARSHAL_SIZE_MAX > &lb = pl->p_locators.back();
				Utils::zero< ZT_LOCATOR_MARSHAL_SIZE_MAX >(lb.data);
				const int ls = loc->marshal(lb.data);
				if (ls > 0) {
					p.locatorSize = (unsigned int)ls;
					p.locator = lb.data;
				}
			}
		}

		pl->peers = pl->p_peers.data();
		pl->peerCount = (unsigned long)pl->p_peers.size();

		return pl;
	} catch ( ... ) {
		delete pl;
		return nullptr;
	}
}

ZT_VirtualNetworkConfig *Node::networkConfig(uint64_t nwid) const
{
	SharedPtr< Network > nw(network(nwid));
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
	ZT_VirtualNetworkList *nl = (ZT_VirtualNetworkList *)buf;
	nl->freeFunction = reinterpret_cast<void (*)(const void *)>(free);
	nl->networks = (ZT_VirtualNetworkConfig *)(buf + sizeof(ZT_VirtualNetworkList));

	nl->networkCount = 0;
	for (Map< uint64_t, SharedPtr< Network > >::const_iterator i(m_networks.begin()); i != m_networks.end(); ++i) // NOLINT(modernize-use-auto,modernize-loop-convert,hicpp-use-auto)
		i->second->externalConfig(&(nl->networks[nl->networkCount++]));

	return nl;
}

void Node::setNetworkUserPtr(
	uint64_t nwid,
	void *ptr)
{
	SharedPtr< Network > nw(network(nwid));
	if (nw)
		*(nw->userPtr()) = ptr;
}

void Node::setInterfaceAddresses(
	const ZT_InterfaceAddress *addrs,
	unsigned int addrCount)
{
	Mutex::Lock _l(m_localInterfaceAddresses_m);
	m_localInterfaceAddresses.clear();
	for (unsigned int i = 0; i < addrCount; ++i) {
		bool dupe = false;
		for (unsigned int j = 0; j < i; ++j) {
			if (*(reinterpret_cast<const InetAddress *>(&addrs[j].address)) == *(reinterpret_cast<const InetAddress *>(&addrs[i].address))) {
				dupe = true;
				break;
			}
		}
		if (!dupe)
			m_localInterfaceAddresses.push_back(addrs[i]);
	}
}

ZT_ResultCode Node::addPeer(
	void *tptr,
	const ZT_Identity *identity)
{
	if (!identity)
		return ZT_RESULT_ERROR_BAD_PARAMETER;
	SharedPtr< Peer > peer(RR->topology->peer(tptr, reinterpret_cast<const Identity *>(identity)->address()));
	if (!peer) {
		peer.set(new Peer(RR));
		peer->init(*reinterpret_cast<const Identity *>(identity));
		peer = RR->topology->add(tptr, peer);
	}
	return (peer->identity() == *reinterpret_cast<const Identity *>(identity)) ? ZT_RESULT_OK : ZT_RESULT_ERROR_COLLIDING_OBJECT;
}

int Node::tryPeer(
	void *tptr,
	const ZT_Fingerprint *fp,
	const ZT_Endpoint *endpoint,
	int retries)
{
	if ((!fp) || (!endpoint))
		return 0;
	const SharedPtr< Peer > peer(RR->topology->peer(tptr, fp->address, true));
	if ((peer) && (peer->identity().fingerprint().bestSpecificityEquals(*fp))) {
		peer->contact(tptr, m_now, Endpoint(*endpoint), std::min(retries, 1));
		return 1;
	}
	return 0;
}

ZT_CertificateError Node::addCertificate(
	void *tptr,
	int64_t now,
	unsigned int localTrust,
	const ZT_Certificate *cert,
	const void *certData,
	unsigned int certSize)
{
	Certificate c;
	if (cert) {
		c = *cert;
	} else {
		if ((!certData) || (!certSize))
			return ZT_CERTIFICATE_ERROR_INVALID_FORMAT;
		if (!c.decode(certData, certSize))
			return ZT_CERTIFICATE_ERROR_INVALID_FORMAT;
	}
	return RR->topology->addCertificate(tptr, c, now, localTrust, true, true, true);
}

ZT_ResultCode Node::deleteCertificate(
	void *tptr,
	const void *serialNo)
{
	if (!serialNo)
		return ZT_RESULT_ERROR_BAD_PARAMETER;
	RR->topology->deleteCertificate(tptr, reinterpret_cast<const uint8_t *>(serialNo));
	return ZT_RESULT_OK;
}

struct p_certificateListInternal
{
	Vector< SharedPtr< const Certificate > > c;
	Vector< unsigned int > t;
};

static void p_freeCertificateList(const void *cl)
{
	if (cl) {
		reinterpret_cast<const p_certificateListInternal *>(reinterpret_cast<const uint8_t *>(cl) + sizeof(ZT_CertificateList))->~p_certificateListInternal();
		free(const_cast<void *>(cl));
	}
}

ZT_CertificateList *Node::listCertificates()
{
	ZT_CertificateList *const cl = (ZT_CertificateList *)malloc(sizeof(ZT_CertificateList) + sizeof(p_certificateListInternal));
	if (!cl)
		return nullptr;

	p_certificateListInternal *const clint = reinterpret_cast<p_certificateListInternal *>(reinterpret_cast<uint8_t *>(cl) + sizeof(ZT_CertificateList));
	new (clint) p_certificateListInternal;
	RR->topology->allCerts(clint->c, clint->t);

	cl->freeFunction = p_freeCertificateList;
	static_assert(sizeof(SharedPtr< const Certificate >) == sizeof(void *), "SharedPtr<> is not just a wrapped pointer");
	cl->certs = reinterpret_cast<const ZT_Certificate **>(clint->c.data());
	cl->localTrust = clint->t.data();
	cl->certCount = (unsigned long)clint->c.size();

	return cl;
}

int Node::sendUserMessage(
	void *tptr,
	uint64_t dest,
	uint64_t typeId,
	const void *data,
	unsigned int len)
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

Vector< uint8_t > Node::stateObjectGet(void *const tPtr, ZT_StateObjectType type, const uint64_t *id, const unsigned int idSize)
{
	Vector< uint8_t > r;
	if (m_cb.stateGetFunction) {
		void *data = nullptr;
		void (*freeFunc)(void *) = nullptr;
		int l = m_cb.stateGetFunction(
			reinterpret_cast<ZT_Node *>(this),
			m_uPtr,
			tPtr,
			type,
			id,
			idSize,
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
		for (Map< uint64_t, SharedPtr< Network > >::iterator i(m_networks.begin()); i != m_networks.end(); ++i) { // NOLINT(hicpp-use-auto,modernize-use-auto,modernize-loop-convert)
			for (unsigned int k = 0, j = i->second->config().staticIpCount; k < j; ++k) {
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
			reinterpret_cast<const ZT_InetAddress *>(&remoteAddress)) != 0);
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
			reinterpret_cast<ZT_InetAddress *>(&addr)) == ZT_RESULT_OK);
	}
	return false;
}

bool Node::localControllerHasAuthorized(const int64_t now, const uint64_t nwid, const Address &addr) const
{
	m_localControllerAuthorizations_l.lock();
	Map<Node::p_LocalControllerAuth, int64_t>::const_iterator i(m_localControllerAuthorizations.find(p_LocalControllerAuth(nwid, addr)));
	const int64_t at = (i == m_localControllerAuthorizations.end()) ? -1LL : i->second;
	m_localControllerAuthorizations_l.unlock();
	if (at > 0)
		return ((now - at) < (ZT_NETWORK_AUTOCONF_DELAY * 3));
	return false;
}

// Implementation of NetworkController::Sender ------------------------------------------------------------------------

void Node::ncSendConfig(uint64_t nwid, uint64_t requestPacketId, const Address &destination, const NetworkConfig &nc, bool sendLegacyFormatConfig)
{
	m_localControllerAuthorizations_l.lock();
	m_localControllerAuthorizations[p_LocalControllerAuth(nwid, destination)] = now();
	m_localControllerAuthorizations_l.unlock();

	if (destination == RR->identity.address()) {
		SharedPtr< Network > n(network(nwid));
		if (!n)
			return;
		n->setConfiguration((void *)0, nc, true);
	} else {
		Dictionary dconf;
		if (nc.toDictionary(dconf)) {
			uint64_t configUpdateId = Utils::random();
			if (!configUpdateId)
				++configUpdateId;

			Vector< uint8_t > ddata;
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

void Node::ncSendRevocation(const Address &destination, const RevocationCredential &rev)
{
	if (destination == RR->identity.address()) {
		SharedPtr< Network > n(network(rev.networkId()));
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
		SharedPtr< Network > n(network(nwid));
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
