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
	ZT_INLINE _NodeObjects(Context &ctx, const CallContext &cc) :
		networks(),
		t(ctx),
		expect(),
		vl2(ctx),
		vl1(ctx),
		topology(ctx, cc),
		sa(ctx),
		ts()
	{
		ctx.networks = &networks;
		ctx.t = &t;
		ctx.expect = &expect;
		ctx.vl2 = &vl2;
		ctx.vl1 = &vl1;
		ctx.topology = &topology;
		ctx.sa = &sa;
		ctx.ts = &ts;
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
	const struct ZT_Node_Callbacks *callbacks,
	const CallContext &cc) :
	m_ctx(this),
	m_store(&m_ctx),
	m_objects(nullptr),
	m_lastPeerPulse(0),
	m_lastHousekeepingRun(0),
	m_lastNetworkHousekeepingRun(0),
	m_lastTrustStoreUpdate(0),
	m_online(false)
{
	ZT_SPEW("Node starting up!");

	Utils::copy< sizeof(ZT_Node_Callbacks) >(&m_ctx.cb, callbacks);
	m_ctx.uPtr = uPtr;
	m_ctx.store = &m_store;

	Vector< uint8_t > data(m_store.get(cc, ZT_STATE_OBJECT_IDENTITY_SECRET, Utils::ZERO256, 0));
	bool haveIdentity = false;
	if (!data.empty()) {
		data.push_back(0); // zero-terminate string
		if (m_ctx.identity.fromString((const char *)data.data())) {
			m_ctx.identity.toString(false, m_ctx.publicIdentityStr);
			m_ctx.identity.toString(true, m_ctx.secretIdentityStr);
			haveIdentity = true;
			ZT_SPEW("loaded identity %s", m_ctx.identity.toString().c_str());
		}
	}

	if (!haveIdentity) {
		m_ctx.identity.generate(Identity::C25519);
		m_ctx.identity.toString(false, m_ctx.publicIdentityStr);
		m_ctx.identity.toString(true, m_ctx.secretIdentityStr);
		m_store.put(cc, ZT_STATE_OBJECT_IDENTITY_SECRET, Utils::ZERO256, 0, m_ctx.secretIdentityStr, (unsigned int)strlen(m_ctx.secretIdentityStr));
		m_store.put(cc, ZT_STATE_OBJECT_IDENTITY_PUBLIC, Utils::ZERO256, 0, m_ctx.publicIdentityStr, (unsigned int)strlen(m_ctx.publicIdentityStr));
		ZT_SPEW("no pre-existing identity found, created %s", m_ctx.identity.toString().c_str());
	} else {
		data = m_store.get(cc, ZT_STATE_OBJECT_IDENTITY_PUBLIC, Utils::ZERO256, 0);
		if ((data.empty()) || (memcmp(data.data(), m_ctx.publicIdentityStr, strlen(m_ctx.publicIdentityStr)) != 0))
			m_store.put(cc, ZT_STATE_OBJECT_IDENTITY_PUBLIC, Utils::ZERO256, 0, m_ctx.publicIdentityStr, (unsigned int)strlen(m_ctx.publicIdentityStr));
	}

	uint8_t localSecretCipherKey[ZT_FINGERPRINT_HASH_SIZE];
	m_ctx.identity.hashWithPrivate(localSecretCipherKey);
	++localSecretCipherKey[0];
	SHA384(localSecretCipherKey, localSecretCipherKey, ZT_FINGERPRINT_HASH_SIZE);
	m_ctx.localSecretCipher.init(localSecretCipherKey);

	for (unsigned int i = 0; i < 1023; ++i)
		m_ctx.randomPrivilegedPortOrder[i] = (uint16_t)(i + 1);
	for (unsigned int i = 0; i < 512; ++i) {
		uint64_t rn = Utils::random();
		const unsigned int a = (unsigned int)rn % 1023;
		const unsigned int b = (unsigned int)(rn >> 32U) % 1023;
		if (a != b) {
			const uint16_t tmp = m_ctx.randomPrivilegedPortOrder[a];
			m_ctx.randomPrivilegedPortOrder[a] = m_ctx.randomPrivilegedPortOrder[b];
			m_ctx.randomPrivilegedPortOrder[b] = tmp;
		}
	}

	m_objects = new _NodeObjects(m_ctx, cc);

	ZT_SPEW("node initialized!");
	postEvent(cc.tPtr, ZT_EVENT_UP);
}

Node::~Node()
{
	ZT_SPEW("Node shutting down (in destructor).");

	m_allNetworks_l.lock();
	m_ctx.networks->clear();
	m_allNetworks.clear();
	m_allNetworks_l.unlock();

	delete reinterpret_cast<_NodeObjects *>(m_objects);

	// Let go of cached Buf objects. If other nodes happen to be running in this
	// same process space new Bufs will be allocated as needed, but this is almost
	// never the case. Calling this here saves RAM if we are running inside something
	// that wants to keep running after tearing down its ZeroTier core instance.
	Buf::freePool();
}

void Node::shutdown(const CallContext &cc)
{
	m_allNetworks_l.lock();
	m_ctx.networks->clear();
	m_allNetworks.clear();
	m_allNetworks_l.unlock();

	postEvent(cc.tPtr, ZT_EVENT_DOWN);

	if (m_ctx.topology)
		m_ctx.topology->saveAll(cc);
}

ZT_ResultCode Node::processBackgroundTasks(
	const CallContext &cc,
	volatile int64_t *nextBackgroundTaskDeadline)
{
	Mutex::Lock bl(m_backgroundTasksLock);

	try {
		// Updating the trust store means checking certificates and certificate chains
		// against the current time, etc., and also resynchronizing roots as specified by
		// certificates. This also happens on demand when the trust store is changed.
		if ((cc.ticks - m_lastTrustStoreUpdate) >= ZT_TRUSTSTORE_UPDATE_PERIOD) {
			m_lastTrustStoreUpdate = cc.ticks;
			if (m_ctx.ts->update(cc.ticks, nullptr))
				m_ctx.topology->trustStoreChanged(cc);
		}

		// Networks perform housekeeping here such as refreshing configs.
		if ((cc.ticks - m_lastNetworkHousekeepingRun) >= ZT_NETWORK_HOUSEKEEPING_PERIOD) {
			m_lastNetworkHousekeepingRun = cc.ticks;
			ZT_SPEW("running networking housekeeping...");
			Mutex::Lock l(m_allNetworks_l);
			for (Vector< SharedPtr< Network > >::const_iterator n(m_allNetworks.begin()); n != m_allNetworks.end(); ++n)
				(*n)->doPeriodicTasks(cc);
		}

		// Perform general housekeeping for other objects in the system.
		if ((cc.ticks - m_lastHousekeepingRun) >= ZT_HOUSEKEEPING_PERIOD) {
			m_lastHousekeepingRun = cc.ticks;
			ZT_SPEW("running housekeeping...");

			m_ctx.topology->doPeriodicTasks(cc);
			m_ctx.sa->clean(cc);
		}

		// Peers have a "pulse" method that does things like keepalive and path housekeeping.
		// This happens last because keepalives are only necessary if nothing has been sent
		// in a while, and some of the above actions may cause peers to send things which may
		// reduce the need for keepalives. Root ranking also happens here.
		if ((cc.ticks - m_lastPeerPulse) >= ZT_PEER_PULSE_INTERVAL) {
			m_lastPeerPulse = cc.ticks;
			ZT_SPEW("running pulse() on each peer...");
			try {
				Vector< SharedPtr< Peer > > allPeers, rootPeers;
				m_ctx.topology->allPeers(allPeers, rootPeers);
				std::sort(rootPeers.begin(), rootPeers.end());

				bool online = false;
				for (Vector< SharedPtr< Peer > >::iterator p(allPeers.begin()); p != allPeers.end(); ++p) {
					const bool isRoot = std::binary_search(rootPeers.begin(), rootPeers.end(), *p);
					(*p)->pulse(m_ctx, cc, isRoot);
					online |= ((isRoot || rootPeers.empty()) && (*p)->directlyConnected(cc));
				}

				if (m_online.exchange(online, std::memory_order_relaxed) != online)
					postEvent(cc.tPtr, online ? ZT_EVENT_ONLINE : ZT_EVENT_OFFLINE);

				ZT_SPEW("ranking roots...");
				m_ctx.topology->rankRoots(cc);
			} catch (...) {
				return ZT_RESULT_FATAL_ERROR_INTERNAL;
			}
		}

		*nextBackgroundTaskDeadline = cc.ticks + ZT_TIMER_TASK_INTERVAL;
	} catch (...) {
		return ZT_RESULT_FATAL_ERROR_INTERNAL;
	}

	return ZT_RESULT_OK;
}

ZT_ResultCode Node::join(
	uint64_t nwid,
	const ZT_Fingerprint *controllerFingerprint,
	void *uptr,
	const CallContext &cc)
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
	SharedPtr< Network > network(new Network(m_ctx, cc, nwid, fp, uptr, nullptr));
	m_allNetworks.push_back(network);
	m_ctx.networks->set(nwid, network);

	return ZT_RESULT_OK;
}

ZT_ResultCode Node::leave(
	uint64_t nwid,
	void **uptr,
	const CallContext &cc)
{
	Mutex::Lock l(m_allNetworks_l);

	ZT_SPEW("leaving network %.16llx", nwid);
	ZT_VirtualNetworkConfig ctmp;

	SharedPtr< Network > network;
	m_ctx.networks->erase(nwid);
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
	m_store.erase(cc, ZT_STATE_OBJECT_NETWORK_CONFIG, tmp, 1);

	if (network) {
		if (uptr)
			*uptr = *network->userPtr();
		network->externalConfig(&ctmp);
		m_ctx.cb.virtualNetworkConfigFunction(reinterpret_cast<ZT_Node *>(this), m_ctx.uPtr, cc.tPtr, nwid, network->userPtr(), ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY, &ctmp);
		network->destroy();
		return ZT_RESULT_OK;
	} else {
		return ZT_RESULT_ERROR_NETWORK_NOT_FOUND;
	}
}

ZT_ResultCode Node::multicastSubscribe(
	const CallContext &cc,
	uint64_t nwid,
	uint64_t multicastGroup,
	unsigned long multicastAdi)
{
	ZT_SPEW("multicast subscribe to %s:%lu", MAC(multicastGroup).toString().c_str(), multicastAdi);
	const SharedPtr< Network > nw(m_ctx.networks->get(nwid));
	if (nw) {
		nw->multicastSubscribe(cc, MulticastGroup(MAC(multicastGroup), (uint32_t)(multicastAdi & 0xffffffff)));
		return ZT_RESULT_OK;
	} else {
		return ZT_RESULT_ERROR_NETWORK_NOT_FOUND;
	}
}

ZT_ResultCode Node::multicastUnsubscribe(
	const CallContext &cc,
	uint64_t nwid,
	uint64_t multicastGroup,
	unsigned long multicastAdi)
{
	ZT_SPEW("multicast unsubscribe from %s:%lu", MAC(multicastGroup).toString().c_str(), multicastAdi);
	const SharedPtr< Network > nw(m_ctx.networks->get(nwid));
	if (nw) {
		nw->multicastUnsubscribe(MulticastGroup(MAC(multicastGroup), (uint32_t)(multicastAdi & 0xffffffff)));
		return ZT_RESULT_OK;
	} else {
		return ZT_RESULT_ERROR_NETWORK_NOT_FOUND;
	}
}

void Node::status(ZT_NodeStatus *status) const
{
	status->address = m_ctx.identity.address().toInt();
	status->identity = reinterpret_cast<const ZT_Identity *>(&m_ctx.identity);
	status->publicIdentity = m_ctx.publicIdentityStr;
	status->secretIdentity = m_ctx.secretIdentityStr;
	status->online = m_online ? 1 : 0;
}

struct p_ZT_PeerListPrivate : public ZT_PeerList
{
	// Actual containers for the memory, hidden from external users.
	Vector< ZT_Peer > p_peers;
	ForwardList< Vector< ZT_Path > > p_paths;
	ForwardList< Identity > p_identities;
	ForwardList< Blob< ZT_LOCATOR_MARSHAL_SIZE_MAX > > p_locators;
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

ZT_PeerList *Node::peers(const CallContext &cc) const
{
	p_ZT_PeerListPrivate *pl = nullptr;
	try {
		pl = new p_ZT_PeerListPrivate;
		pl->freeFunction = p_peerListFreeFunction;

		Vector< SharedPtr< Peer > > peers, rootPeers;
		m_ctx.topology->allPeers(peers, rootPeers);
		std::sort(peers.begin(), peers.end(), p_sortPeerPtrsByAddress());
		std::sort(rootPeers.begin(), rootPeers.end());

		for (Vector< SharedPtr< Peer > >::iterator pi(peers.begin()); pi != peers.end(); ++pi) {
			pl->p_peers.push_back(ZT_Peer());
			ZT_Peer &p = pl->p_peers.back();
			Peer &pp = **pi;

			p.address = pp.address();
			pl->p_identities.push_front(pp.identity());
			p.identity = reinterpret_cast<const ZT_Identity *>(&(pl->p_identities.front()));
			p.fingerprint = &(pl->p_identities.front().fingerprint());
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
				pl->p_paths.push_front(Vector< ZT_Path >());
				std::vector< ZT_Path > &apiPaths = pl->p_paths.front();
				apiPaths.resize(ztPaths.size());
				for (unsigned long i = 0; i < (unsigned long)ztPaths.size(); ++i) {
					SharedPtr< Path > &ztp = ztPaths[i];
					ZT_Path &apip = apiPaths[i];
					apip.endpoint.type = ZT_ENDPOINT_TYPE_IP_UDP;
					Utils::copy< sizeof(struct sockaddr_storage) >(&(apip.endpoint.value.ss), &(ztp->address().as.ss));
					apip.lastSend = ztp->lastOut();
					apip.lastReceive = ztp->lastIn();
					apip.alive = ztp->alive(cc) ? 1 : 0;
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
				pl->p_locators.push_front(Blob< ZT_LOCATOR_MARSHAL_SIZE_MAX >());
				Blob< ZT_LOCATOR_MARSHAL_SIZE_MAX > &lb = pl->p_locators.front();
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
	const SharedPtr< Network > nw(m_ctx.networks->get(nwid));
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
	SharedPtr< Network > nw(m_ctx.networks->get(nwid));
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

ZT_CertificateError Node::addCertificate(
	const CallContext &cc,
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
	m_ctx.ts->add(c, localTrust);
	m_ctx.ts->update(cc.clock, nullptr);
	SharedPtr< TrustStore::Entry > ent(m_ctx.ts->get(c.getSerialNo()));
	return (ent) ? ent->error() : ZT_CERTIFICATE_ERROR_INVALID_FORMAT; // should never be null, but if so it means invalid
}

ZT_ResultCode Node::deleteCertificate(
	const CallContext &cc,
	const void *serialNo)
{
	if (!serialNo)
		return ZT_RESULT_ERROR_BAD_PARAMETER;
	m_ctx.ts->erase(H384(serialNo));
	m_ctx.ts->update(-1, nullptr);
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

	clint->entries = m_ctx.ts->all(false);
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
	const CallContext &cc,
	uint64_t dest,
	uint64_t /*typeId*/,
	const void */*data*/,
	unsigned int /*len*/)
{
	try {
		if (m_ctx.identity.address().toInt() != dest) {
			// TODO
			/*
			Packet outp(Address(dest),m_ctx.identity.address(),Packet::VERB_USER_MESSAGE);
			outp.append(typeId);
			outp.append(data,len);
			outp.compress();
			m_ctx.sw->send(tptr,outp,true);
			*/
			return 1;
		}
	} catch (...) {}
	return 0;
}

void Node::setController(void *networkControllerInstance)
{
	m_ctx.localNetworkController = reinterpret_cast<NetworkController *>(networkControllerInstance);
	if (networkControllerInstance)
		m_ctx.localNetworkController->init(m_ctx.identity, this);
}

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

	if (m_ctx.cb.pathCheckFunction) {
		return (m_ctx.cb.pathCheckFunction(
			reinterpret_cast<ZT_Node *>(this),
			m_ctx.uPtr,
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
	if (m_ctx.cb.pathLookupFunction) {
		return (m_ctx.cb.pathLookupFunction(
			reinterpret_cast<ZT_Node *>(this),
			m_ctx.uPtr,
			tPtr,
			id.address().toInt(),
			reinterpret_cast<const ZT_Identity *>(&id),
			family,
			reinterpret_cast<ZT_InetAddress *>(&addr)) == ZT_RESULT_OK);
	}
	return false;
}

// Implementation of NetworkController::Sender ------------------------------------------------------------------------

void Node::ncSendConfig(void *tPtr, int64_t clock, int64_t ticks, uint64_t nwid, uint64_t requestPacketId, const Address &destination, const NetworkConfig &nc, bool sendLegacyFormatConfig)
{
	if (destination == m_ctx.identity.address()) {
		SharedPtr< Network > n(m_ctx.networks->get(nwid));
		if (!n)
			return;
		CallContext cc(clock, ticks, tPtr);
		n->setConfiguration(cc, nc, true);
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
				Packet outp(destination,m_ctx.identity.address(),(requestPacketId) ? Packet::VERB_OK : Packet::VERB_NETWORK_CONFIG);
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
				const unsigned int siglen = m_ctx.identity.sign(reinterpret_cast<const uint8_t *>(outp.data()) + sigStart,outp.size() - sigStart,sig,sizeof(sig));
				outp.append((uint8_t)1);
				outp.append((uint16_t)siglen);
				outp.append(sig,siglen);

				outp.compress();
				m_ctx.sw->send((void *)0,outp,true);
				chunkIndex += chunkLen;
			}
			*/
		}
	}
}

void Node::ncSendRevocation(void *tPtr, int64_t clock, int64_t ticks, const Address &destination, const RevocationCredential &rev)
{
	if (destination == m_ctx.identity.address()) {
		SharedPtr< Network > n(m_ctx.networks->get(rev.networkId()));
		if (!n)
			return;
		CallContext cc(clock, ticks, tPtr);
		n->addCredential(cc, m_ctx.identity, rev);
	} else {
		// TODO
		/*
		Packet outp(destination,m_ctx.identity.address(),Packet::VERB_NETWORK_CREDENTIALS);
		outp.append((uint8_t)0x00);
		outp.append((uint16_t)0);
		outp.append((uint16_t)0);
		outp.append((uint16_t)1);
		rev.serialize(outp);
		outp.append((uint16_t)0);
		m_ctx.sw->send((void *)0,outp,true);
		*/
	}
}

void Node::ncSendError(void *tPtr, int64_t clock, int64_t ticks, uint64_t nwid, uint64_t requestPacketId, const Address &destination, NetworkController::ErrorCode errorCode)
{
	if (destination == m_ctx.identity.address()) {
		SharedPtr< Network > n(m_ctx.networks->get(nwid));
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
		Packet outp(destination,m_ctx.identity.address(),Packet::VERB_ERROR);
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
		m_ctx.sw->send((void *)0,outp,true);
		*/
	} // else we can't send an ERROR() in response to nothing, so discard
}

} // namespace ZeroTier
