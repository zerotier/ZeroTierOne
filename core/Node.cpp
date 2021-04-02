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
#include "TrustStore.hpp"
#include "Store.hpp"

namespace ZeroTier {

namespace {

struct _NodeObjects
{
	ZT_INLINE _NodeObjects(RuntimeEnvironment *const RR, Node *const n, void *const tPtr, const int64_t now) :
		networks(),
		t(RR),
		expect(),
		vl2(RR),
		vl1(RR),
		topology(RR, tPtr, now),
		sa(RR),
		ts()
	{
		RR->networks = &networks;
		RR->t = &t;
		RR->expect = &expect;
		RR->vl2 = &vl2;
		RR->vl1 = &vl1;
		RR->topology = &topology;
		RR->sa = &sa;
		RR->ts = &ts;
	}

	TinyMap< SharedPtr< Network > > networks;
	Trace t;
	Expect expect;
	VL2 vl2;
	VL1 vl1;
	Topology topology;
	SelfAwareness sa;
	TrustStore ts;
};

} // anonymous namespace

Node::Node(
	void *uPtr,
	void *tPtr,
	const struct ZT_Node_Callbacks *callbacks,
	int64_t now) :
	m_RR(this),
	RR(&m_RR),
	m_store(&m_RR),
	m_objects(nullptr),
	m_lastPeerPulse(0),
	m_lastHousekeepingRun(0),
	m_lastNetworkHousekeepingRun(0),
	m_lastRootRank(0),
	m_now(now),
	m_online(false)
{
	ZT_SPEW("Node starting up!");

	Utils::copy< sizeof(ZT_Node_Callbacks) >(&m_RR.cb, callbacks);
	m_RR.uPtr = uPtr;
	m_RR.store = &m_store;

	Vector< uint8_t > data(m_store.get(tPtr, ZT_STATE_OBJECT_IDENTITY_SECRET, Utils::ZERO256, 0));
	bool haveIdentity = false;
	if (!data.empty()) {
		data.push_back(0); // zero-terminate string
		if (m_RR.identity.fromString((const char *)data.data())) {
			m_RR.identity.toString(false, m_RR.publicIdentityStr);
			m_RR.identity.toString(true, m_RR.secretIdentityStr);
			haveIdentity = true;
			ZT_SPEW("loaded identity %s", RR->identity.toString().c_str());
		}
	}

	if (!haveIdentity) {
		m_RR.identity.generate(Identity::C25519);
		m_RR.identity.toString(false, m_RR.publicIdentityStr);
		m_RR.identity.toString(true, m_RR.secretIdentityStr);
		m_store.put(tPtr, ZT_STATE_OBJECT_IDENTITY_SECRET, Utils::ZERO256, 0, m_RR.secretIdentityStr, (unsigned int)strlen(m_RR.secretIdentityStr));
		m_store.put(tPtr, ZT_STATE_OBJECT_IDENTITY_PUBLIC, Utils::ZERO256, 0, m_RR.publicIdentityStr, (unsigned int)strlen(m_RR.publicIdentityStr));
		ZT_SPEW("no pre-existing identity found, created %s", RR->identity.toString().c_str());
	} else {
		data = m_store.get(tPtr, ZT_STATE_OBJECT_IDENTITY_PUBLIC, Utils::ZERO256, 0);
		if ((data.empty()) || (memcmp(data.data(), m_RR.publicIdentityStr, strlen(m_RR.publicIdentityStr)) != 0))
			m_store.put(tPtr, ZT_STATE_OBJECT_IDENTITY_PUBLIC, Utils::ZERO256, 0, m_RR.publicIdentityStr, (unsigned int)strlen(m_RR.publicIdentityStr));
	}

	uint8_t localSecretCipherKey[ZT_FINGERPRINT_HASH_SIZE];
	m_RR.identity.hashWithPrivate(localSecretCipherKey);
	++localSecretCipherKey[0];
	SHA384(localSecretCipherKey, localSecretCipherKey, ZT_FINGERPRINT_HASH_SIZE);
	m_RR.localSecretCipher.init(localSecretCipherKey);

	for (unsigned int i = 0; i < 1023; ++i)
		m_RR.randomPrivilegedPortOrder[i] = (uint16_t)(i + 1);
	for (unsigned int i = 0; i < 512; ++i) {
		uint64_t rn = Utils::random();
		const unsigned int a = (unsigned int)rn % 1023;
		const unsigned int b = (unsigned int)(rn >> 32U) % 1023;
		if (a != b) {
			const uint16_t tmp = m_RR.randomPrivilegedPortOrder[a];
			m_RR.randomPrivilegedPortOrder[a] = m_RR.randomPrivilegedPortOrder[b];
			m_RR.randomPrivilegedPortOrder[b] = tmp;
		}
	}

	m_objects = new _NodeObjects(&m_RR, this, tPtr, now);

	ZT_SPEW("node initialized!");
	postEvent(tPtr, ZT_EVENT_UP);
}

Node::~Node()
{
	ZT_SPEW("Node shutting down (in destructor).");

	m_allNetworks_l.lock();
	RR->networks->clear();
	m_allNetworks.clear();
	m_allNetworks_l.unlock();

	delete reinterpret_cast<_NodeObjects *>(m_objects);

	// Let go of cached Buf objects. If other nodes happen to be running in this
	// same process space new Bufs will be allocated as needed, but this is almost
	// never the case. Calling this here saves RAM if we are running inside something
	// that wants to keep running after tearing down its ZeroTier core instance.
	Buf::freePool();
}

void Node::shutdown(void *tPtr)
{
	m_allNetworks_l.lock();
	RR->networks->clear();
	m_allNetworks.clear();
	m_allNetworks_l.unlock();
	postEvent(tPtr, ZT_EVENT_DOWN);
	if (RR->topology)
		RR->topology->saveAll(tPtr);
}

ZT_ResultCode Node::processBackgroundTasks(
	void *tPtr,
	int64_t now,
	volatile int64_t *nextBackgroundTaskDeadline)
{
	m_now = now;
	Mutex::Lock bl(m_backgroundTasksLock);

	try {
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

		if ((now - m_lastNetworkHousekeepingRun) >= ZT_NETWORK_HOUSEKEEPING_PERIOD) {
			m_lastHousekeepingRun = now;
			ZT_SPEW("running networking housekeeping...");
			Mutex::Lock l(m_allNetworks_l);
			for (Vector< SharedPtr< Network > >::const_iterator i(m_allNetworks.begin()); i != m_allNetworks.end(); ++i) {
				(*i)->doPeriodicTasks(tPtr, now);
			}
		}

		if ((now - m_lastHousekeepingRun) >= ZT_HOUSEKEEPING_PERIOD) {
			m_lastHousekeepingRun = now;
			ZT_SPEW("running housekeeping...");

			RR->topology->doPeriodicTasks(tPtr, now);
			RR->sa->clean(now);
		}

		if ((now - m_lastRootRank) >= ZT_ROOT_RANK_PERIOD) {
			m_lastRootRank = now;
			ZT_SPEW("ranking roots...");
			RR->topology->rankRoots(now);
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
	Mutex::Lock l(m_allNetworks_l);

	Fingerprint fp;
	if (controllerFingerprint) {
		fp = *controllerFingerprint;
		ZT_SPEW("joining network %.16llx with controller fingerprint %s", nwid, fp.toString().c_str());
	} else {
		ZT_SPEW("joining network %.16llx", nwid);
	}

	for (Vector< SharedPtr< Network > >::iterator n(m_allNetworks.begin()); n != m_allNetworks.end(); ++n) {
		if ((*n)->id() == nwid)
			return ZT_RESULT_OK;
	}
	SharedPtr< Network > network(new Network(RR, tptr, nwid, fp, uptr, nullptr));
	m_allNetworks.push_back(network);
	RR->networks->set(nwid, network);

	return ZT_RESULT_OK;
}

ZT_ResultCode Node::leave(
	uint64_t nwid,
	void **uptr,
	void *tptr)
{
	Mutex::Lock l(m_allNetworks_l);

	ZT_SPEW("leaving network %.16llx", nwid);
	ZT_VirtualNetworkConfig ctmp;

	SharedPtr< Network > network;
	RR->networks->erase(nwid);
	for (Vector< SharedPtr< Network > >::iterator n(m_allNetworks.begin()); n != m_allNetworks.end(); ++n) {
		if ((*n)->id() == nwid) {
			network.move(*n);
			m_allNetworks.erase(n);
			break;
		}
	}

	uint64_t tmp[2];
	tmp[0] = nwid;
	tmp[1] = 0;
	m_store.erase(tptr, ZT_STATE_OBJECT_NETWORK_CONFIG, tmp, 1);

	if (network) {
		if (uptr)
			*uptr = *network->userPtr();
		network->externalConfig(&ctmp);
		RR->node->configureVirtualNetworkPort(tptr, nwid, uptr, ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY, &ctmp);
		network->destroy();
		return ZT_RESULT_OK;
	}
	return ZT_RESULT_ERROR_NETWORK_NOT_FOUND;
}

ZT_ResultCode Node::multicastSubscribe(
	void *tPtr,
	uint64_t nwid,
	uint64_t multicastGroup,
	unsigned long multicastAdi)
{
	ZT_SPEW("multicast subscribe to %s:%lu", MAC(multicastGroup).toString().c_str(), multicastAdi);
	const SharedPtr< Network > nw(RR->networks->get(nwid));
	if (nw) {
		nw->multicastSubscribe(tPtr, MulticastGroup(MAC(multicastGroup), (uint32_t)(multicastAdi & 0xffffffff)));
		return ZT_RESULT_OK;
	} else {
		return ZT_RESULT_ERROR_NETWORK_NOT_FOUND;
	}
}

ZT_ResultCode Node::multicastUnsubscribe(
	uint64_t nwid,
	uint64_t multicastGroup,
	unsigned long multicastAdi)
{
	ZT_SPEW("multicast unsubscribe from %s:%lu", MAC(multicastGroup).toString().c_str(), multicastAdi);
	const SharedPtr< Network > nw(RR->networks->get(nwid));
	if (nw) {
		nw->multicastUnsubscribe(MulticastGroup(MAC(multicastGroup), (uint32_t)(multicastAdi & 0xffffffff)));
		return ZT_RESULT_OK;
	} else {
		return ZT_RESULT_ERROR_NETWORK_NOT_FOUND;
	}
}

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
	Vector< ZT_Peer > p_peers;
	List< Vector< ZT_Path > > p_paths;
	List< Identity > p_identities;
	List< Blob< ZT_LOCATOR_MARSHAL_SIZE_MAX > > p_locators;
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

			Vector< SharedPtr< Path > > ztPaths;
			pp.getAllPaths(ztPaths);
			if (ztPaths.empty()) {
				pl->p_paths.push_back(Vector< ZT_Path >());
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
	} catch (...) {
		delete pl;
		return nullptr;
	}
}

ZT_VirtualNetworkConfig *Node::networkConfig(uint64_t nwid) const
{
	const SharedPtr< Network > nw(RR->networks->get(nwid));
	if (nw) {
		ZT_VirtualNetworkConfig *const nc = (ZT_VirtualNetworkConfig *)::malloc(sizeof(ZT_VirtualNetworkConfig));
		nw->externalConfig(nc);
		return nc;
	} else {
		return nullptr;
	}
}

ZT_VirtualNetworkList *Node::networks() const
{
	Mutex::Lock l(m_allNetworks_l);

	char *const buf = (char *)::malloc(sizeof(ZT_VirtualNetworkList) + (sizeof(ZT_VirtualNetworkConfig) * m_allNetworks.size()));
	if (!buf)
		return nullptr;
	ZT_VirtualNetworkList *nl = (ZT_VirtualNetworkList *)buf;
	nl->freeFunction = reinterpret_cast<void (*)(const void *)>(free);
	nl->networks = (ZT_VirtualNetworkConfig *)(buf + sizeof(ZT_VirtualNetworkList));

	nl->networkCount = 0;
	for (Vector< SharedPtr< Network > >::const_iterator i(m_allNetworks.begin()); i != m_allNetworks.end(); ++i)
		(*i)->externalConfig(&(nl->networks[nl->networkCount++]));

	return nl;
}

void Node::setNetworkUserPtr(
	uint64_t nwid,
	void *ptr)
{
	SharedPtr< Network > nw(RR->networks->get(nwid));
	if (nw) {
		m_allNetworks_l.lock(); // ensure no concurrent modification of user PTR in network
		*(nw->userPtr()) = ptr;
		m_allNetworks_l.unlock();
	}
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
	RR->ts->add(c, localTrust);
	RR->ts->update(now, nullptr);
	SharedPtr< TrustStore::Entry > ent(RR->ts->get(c.getSerialNo()));
	return (ent) ? ent->error() : ZT_CERTIFICATE_ERROR_INVALID_FORMAT; // should never be null, but if so it means invalid
}

ZT_ResultCode Node::deleteCertificate(
	void *tptr,
	const void *serialNo)
{
	if (!serialNo)
		return ZT_RESULT_ERROR_BAD_PARAMETER;
	RR->ts->erase(H384(serialNo));
	RR->ts->update(-1, nullptr);
	return ZT_RESULT_OK;
}

struct p_certificateListInternal
{
	Vector< SharedPtr< TrustStore::Entry > > entries;
	Vector< const ZT_Certificate * > c;
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
	new(clint) p_certificateListInternal;

	clint->entries = RR->ts->all(false);
	clint->c.reserve(clint->entries.size());
	clint->t.reserve(clint->entries.size());
	for (Vector< SharedPtr< TrustStore::Entry > >::const_iterator i(clint->entries.begin()); i != clint->entries.end(); ++i) {
		clint->c.push_back(&((*i)->certificate()));
		clint->t.push_back((*i)->localTrust());
	}

	cl->freeFunction = p_freeCertificateList;
	cl->certs = clint->c.data();
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
	m_RR.localNetworkController = reinterpret_cast<NetworkController *>(networkControllerInstance);
	if (networkControllerInstance)
		m_RR.localNetworkController->init(RR->identity, this);
}

// Methods used only within the core ----------------------------------------------------------------------------------

bool Node::shouldUsePathForZeroTierTraffic(void *tPtr, const Identity &id, const int64_t localSocket, const InetAddress &remoteAddress)
{
	{
		Mutex::Lock l(m_allNetworks_l);
		for (Vector< SharedPtr< Network > >::iterator i(m_allNetworks.begin()); i != m_allNetworks.end(); ++i) {
			for (unsigned int k = 0, j = (*i)->config().staticIpCount; k < j; ++k) {
				if ((*i)->config().staticIps[k].containsAddress(remoteAddress))
					return false;
			}
		}
	}

	if (RR->cb.pathCheckFunction) {
		return (RR->cb.pathCheckFunction(
			reinterpret_cast<ZT_Node *>(this),
			RR->uPtr,
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
	if (RR->cb.pathLookupFunction) {
		return (RR->cb.pathLookupFunction(
			reinterpret_cast<ZT_Node *>(this),
			RR->uPtr,
			tPtr,
			id.address().toInt(),
			reinterpret_cast<const ZT_Identity *>(&id),
			family,
			reinterpret_cast<ZT_InetAddress *>(&addr)) == ZT_RESULT_OK);
	}
	return false;
}

// Implementation of NetworkController::Sender ------------------------------------------------------------------------

void Node::ncSendConfig(uint64_t nwid, uint64_t requestPacketId, const Address &destination, const NetworkConfig &nc, bool sendLegacyFormatConfig)
{
	if (destination == RR->identity.address()) {
		SharedPtr< Network > n(RR->networks->get(nwid));
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
		SharedPtr< Network > n(RR->networks->get(rev.networkId()));
		if (!n)
			return;
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
		SharedPtr< Network > n(RR->networks->get(nwid));
		if (!n)
			return;
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
