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
#include "RuntimeEnvironment.hpp"
#include "Trace.hpp"
#include "Peer.hpp"
#include "Topology.hpp"
#include "SelfAwareness.hpp"
#include "InetAddress.hpp"
#include "Protocol.hpp"
#include "Endpoint.hpp"
#include "Expect.hpp"

namespace ZeroTier {

Peer::Peer(const RuntimeEnvironment *renv) :
	RR(renv),
	m_ephemeralPairTimestamp(0),
	m_lastReceive(0),
	m_lastSend(0),
	m_lastSentHello(),
	m_lastWhoisRequestReceived(0),
	m_lastEchoRequestReceived(0),
	m_lastPrioritizedPaths(0),
	m_lastProbeReceived(0),
	m_alivePathCount(0),
	m_tryQueue(),
	m_tryQueuePtr(m_tryQueue.end()),
	m_vProto(0),
	m_vMajor(0),
	m_vMinor(0),
	m_vRevision(0)
{
}

Peer::~Peer()
{
	Utils::burn(m_helloMacKey,sizeof(m_helloMacKey));
}

bool Peer::init(const Identity &peerIdentity)
{
	RWMutex::Lock l(m_lock);

	if (m_id) // already initialized sanity check
		return false;
	m_id = peerIdentity;

	uint8_t k[ZT_SYMMETRIC_KEY_SIZE];
	if (!RR->identity.agree(peerIdentity,k))
		return false;
	m_identityKey.set(new SymmetricKey(RR->node->now(),k));
	Utils::burn(k,sizeof(k));

	m_deriveSecondaryIdentityKeys();

	return true;
}

void Peer::received(
	void *tPtr,
	const SharedPtr<Path> &path,
	const unsigned int hops,
	const uint64_t packetId,
	const unsigned int payloadLength,
	const Protocol::Verb verb,
	const Protocol::Verb inReVerb)
{
	const int64_t now = RR->node->now();

	m_lastReceive = now;
	m_inMeter.log(now,payloadLength);

	if (hops == 0) {
		RWMutex::RMaybeWLock l(m_lock);

		// If this matches an existing path, skip path learning stuff. For the small number
		// of paths a peer will have linear scan is the fastest way to do lookup.
		for (unsigned int i=0;i < m_alivePathCount;++i) {
			if (m_paths[i] == path)
				return;
		}

		// If we made it here, we don't already know this path.
		if (RR->node->shouldUsePathForZeroTierTraffic(tPtr, m_id, path->localSocket(), path->address())) {
			// SECURITY: note that if we've made it here we expected this OK, see Expect.hpp.
			// There is replay protection in effect for OK responses.
			if (verb == Protocol::VERB_OK) {
				// If we're learning a new path convert the lock to an exclusive write lock.
				l.writing();

				// If the path list is full, replace the least recently active path. Otherwise append new path.
				unsigned int newPathIdx = 0;
				if (m_alivePathCount == ZT_MAX_PEER_NETWORK_PATHS) {
					int64_t lastReceiveTimeMax = 0;
					for (unsigned int i=0;i<m_alivePathCount;++i) {
						if ((m_paths[i]->address().family() == path->address().family()) &&
						    (m_paths[i]->localSocket() == path->localSocket()) && // TODO: should be localInterface when multipath is integrated
						    (m_paths[i]->address().ipsEqual2(path->address()))) {
							// Replace older path if everything is the same except the port number, since NAT/firewall reboots
							// and other wacky stuff can change port number assignments.
							m_paths[i] = path;
							return;
						} else if (m_paths[i]->lastIn() >= lastReceiveTimeMax) {
							lastReceiveTimeMax = m_paths[i]->lastIn();
							newPathIdx = i;
						}
					}
				} else {
					newPathIdx = m_alivePathCount++;
				}

				InetAddress old;
				if (m_paths[newPathIdx])
					old = m_paths[newPathIdx]->address();
				m_paths[newPathIdx] = path;

				// Re-prioritize paths to include the new one.
				m_prioritizePaths(now);

				// Remember most recently learned paths for future bootstrap attempts on restart.
				Endpoint pathEndpoint(path->address());
				m_bootstrap[pathEndpoint.type()] = pathEndpoint;

				RR->t->learnedNewPath(tPtr, 0x582fabdd, packetId, m_id, path->address(), old);
			} else {
				path->sent(now,hello(tPtr,path->localSocket(),path->address(),now));
				RR->t->tryingNewPath(tPtr, 0xb7747ddd, m_id, path->address(), path->address(), packetId, (uint8_t)verb, m_id);
			}
		}
	}
}

void Peer::send(void *tPtr,int64_t now,const void *data,unsigned int len) noexcept
{
	SharedPtr<Path> via(this->path(now));
	if (via) {
		via->send(RR,tPtr,data,len,now);
	} else {
		const SharedPtr<Peer> root(RR->topology->root());
		if ((root)&&(root.ptr() != this)) {
			via = root->path(now);
			if (via) {
				via->send(RR,tPtr,data,len,now);
				root->relayed(now,len);
			} else {
				return;
			}
		} else {
			return;
		}
	}
	sent(now,len);
}

unsigned int Peer::hello(void *tPtr,int64_t localSocket,const InetAddress &atAddress,const int64_t now)
{
	Buf outp;

	const uint64_t packetId = m_identityKey->nextMessage(RR->identity.address(),m_id.address());
	int ii = Protocol::newPacket(outp,packetId,m_id.address(),RR->identity.address(),Protocol::VERB_HELLO);

	outp.wI8(ii,ZT_PROTO_VERSION);
	outp.wI8(ii,ZEROTIER_VERSION_MAJOR);
	outp.wI8(ii,ZEROTIER_VERSION_MINOR);
	outp.wI16(ii,ZEROTIER_VERSION_REVISION);
	outp.wI64(ii,(uint64_t)now);
	outp.wO(ii,RR->identity);
	outp.wO(ii,atAddress);

	const int ivStart = ii;
	outp.wR(ii,12);

	// LEGACY: the six reserved bytes after the IV exist for legacy compatibility with v1.x nodes.
	// Once those are dead they'll become just reserved bytes for future use as flags etc.
	outp.wI32(ii,0); // reserved bytes
	void *const legacyMoonCountStart = outp.unsafeData + ii;
	outp.wI16(ii,0);
	const uint64_t legacySalsaIv = packetId & ZT_CONST_TO_BE_UINT64(0xfffffffffffffff8ULL);
	Salsa20(m_identityKey->secret,&legacySalsaIv).crypt12(legacyMoonCountStart,legacyMoonCountStart,2);

	const int cryptSectionStart = ii;
	FCV<uint8_t,4096> md;
	Dictionary::append(md,ZT_PROTO_HELLO_NODE_META_INSTANCE_ID,RR->instanceId);
	outp.wI16(ii,(uint16_t)md.size());
	outp.wB(ii,md.data(),(unsigned int)md.size());

	if (unlikely((ii + ZT_HMACSHA384_LEN) > ZT_BUF_SIZE)) // sanity check: should be impossible
		return 0;

	AES::CTR ctr(m_helloCipher);
	void *const cryptSection = outp.unsafeData + ii;
	ctr.init(outp.unsafeData + ivStart,0,cryptSection);
	ctr.crypt(cryptSection,ii - cryptSectionStart);
	ctr.finish();

	HMACSHA384(m_helloMacKey,outp.unsafeData,ii,outp.unsafeData + ii);
	ii += ZT_HMACSHA384_LEN;

	// LEGACY: we also need Poly1305 for v1.x peers.
	uint8_t polyKey[ZT_POLY1305_KEY_SIZE],perPacketKey[ZT_SALSA20_KEY_SIZE];
	Protocol::salsa2012DeriveKey(m_identityKey->secret,perPacketKey,outp,ii);
	Salsa20(perPacketKey,&packetId).crypt12(Utils::ZERO256,polyKey,sizeof(polyKey));
	Poly1305 p1305(polyKey);
	p1305.update(outp.unsafeData + ZT_PROTO_PACKET_ENCRYPTED_SECTION_START,ii - ZT_PROTO_PACKET_ENCRYPTED_SECTION_START);
	uint64_t polyMac[2];
	p1305.finish(polyMac);
	Utils::storeAsIsEndian<uint64_t>(outp.unsafeData + ZT_PROTO_PACKET_MAC_INDEX,polyMac[0]);

	if (likely(RR->node->putPacket(tPtr,localSocket,atAddress,outp.unsafeData,ii)))
		return ii;
	return 0;
}

void Peer::pulse(void *const tPtr,const int64_t now,const bool isRoot)
{
	RWMutex::Lock l(m_lock);

	// Determine if we need to send a full HELLO because we are refreshing ephemeral
	// keys or it's simply been too long.
	bool needHello = false;
	if ( (m_vProto >= 11) && ( ((now - m_ephemeralPairTimestamp) >= (ZT_SYMMETRIC_KEY_TTL / 2)) || ((m_ephemeralKeys[0])&&(m_ephemeralKeys[0]->odometer() >= (ZT_SYMMETRIC_KEY_TTL_MESSAGES / 2))) ) ) {
		m_ephemeralPair.generate();
		needHello = true;
	} else if ((now - m_lastSentHello) >= ZT_PEER_HELLO_INTERVAL) {
		needHello = true;
	}

	// If we have no active paths and none queued to try, attempt any
	// old paths we have cached in m_bootstrap or that external code
	// supplies to the core via the optional API callback.
	if (m_tryQueue.empty()&&(m_alivePathCount == 0)) {
		InetAddress addr;
		if (RR->node->externalPathLookup(tPtr, m_id, -1, addr)) {
			if ((addr)&&(RR->node->shouldUsePathForZeroTierTraffic(tPtr, m_id, -1, addr))) {
				RR->t->tryingNewPath(tPtr, 0x84a10000, m_id, addr, InetAddress::NIL, 0, 0, Identity::NIL);
				sent(now,m_sendProbe(tPtr,-1,addr,nullptr,0,now));
			}
		}

		if (!m_bootstrap.empty()) {
			unsigned int tryAtIndex = (unsigned int)Utils::random() % (unsigned int)m_bootstrap.size();
			for(SortedMap< Endpoint::Type,Endpoint >::const_iterator i(m_bootstrap.begin());i != m_bootstrap.end();++i) {
				if (tryAtIndex > 0) {
					--tryAtIndex;
				} else {
					if ((i->second.isInetAddr())&&(!i->second.ip().ipsEqual(addr))) {
						RR->t->tryingNewPath(tPtr, 0x0a009444, m_id, i->second.ip(), InetAddress::NIL, 0, 0, Identity::NIL);
						sent(now,m_sendProbe(tPtr,-1,i->second.ip(),nullptr,0,now));
						break;
					}
				}
			}
		}
	}

	// Sort paths and forget expired ones.
	m_prioritizePaths(now);

	// Attempt queued endpoints if they don't overlap with paths.
	if (!m_tryQueue.empty()) {
		for(int k=0;k<ZT_NAT_T_MAX_QUEUED_ATTEMPTS_PER_PULSE;++k) {
			// This is a global circular pointer that iterates through the list of
			// endpoints to attempt.
			if (m_tryQueuePtr == m_tryQueue.end()) {
				if (m_tryQueue.empty())
					break;
				m_tryQueuePtr = m_tryQueue.begin();
			}

			if (likely((now - m_tryQueuePtr->ts) < ZT_PATH_ALIVE_TIMEOUT)) {
				if (m_tryQueuePtr->target.isInetAddr()) {
					for(unsigned int i=0;i<m_alivePathCount;++i) {
						if (m_paths[i]->address().ipsEqual(m_tryQueuePtr->target.ip()))
							goto skip_tryQueue_item;
					}

					if ((m_alivePathCount == 0) && (m_tryQueuePtr->breakSymmetricBFG1024) && (RR->node->natMustDie())) {
						// Attempt aggressive NAT traversal if both requested and enabled. This sends a probe
						// to all ports under 1024, which assumes that the peer has bound to such a port and
						// has attempted to initiate a connection through it. This can traverse a decent number
						// of symmetric NATs at the cost of 32KiB per attempt and the potential to trigger IDS
						// systems by looking like a port scan (because it is).
						uint16_t ports[1023];
						for (unsigned int i=0;i<1023;++i)
							ports[i] = (uint64_t)(i + 1);
						for (unsigned int i=0;i<512;++i) {
							const uint64_t rn = Utils::random();
							const unsigned int a = (unsigned int)rn % 1023;
							const unsigned int b = (unsigned int)(rn >> 32U) % 1023;
							if (a != b) {
								const uint16_t tmp = ports[a];
								ports[a] = ports[b];
								ports[b] = tmp;
							}
						}
						sent(now,m_sendProbe(tPtr, -1, m_tryQueuePtr->target.ip(), ports, 1023, now));
					} else {
						sent(now,m_sendProbe(tPtr, -1, m_tryQueuePtr->target.ip(), nullptr, 0, now));
					}
				}
			}

skip_tryQueue_item:
			m_tryQueue.erase(m_tryQueuePtr++);
		}
	}

	// Do keepalive on all currently active paths, sending HELLO to the first
	// if needHello is true and sending small keepalives to others.
	uint64_t randomJunk = Utils::random();
	for(unsigned int i=0;i<m_alivePathCount;++i) {
		if (needHello) {
			needHello = false;
			const unsigned int bytes = hello(tPtr, m_paths[i]->localSocket(), m_paths[i]->address(), now);
			m_paths[i]->sent(now, bytes);
			sent(now,bytes);
			m_lastSentHello = now;
		} else if ((now - m_paths[i]->lastOut()) >= ZT_PATH_KEEPALIVE_PERIOD) {
			m_paths[i]->send(RR, tPtr, reinterpret_cast<uint8_t *>(&randomJunk) + (i & 7U), 1, now);
			sent(now,1);
		}
	}

	// Send a HELLO indirectly if we were not able to send one via any direct path.
	if (needHello) {
		const SharedPtr<Peer> root(RR->topology->root());
		if (root) {
			const SharedPtr<Path> via(root->path(now));
			if (via) {
				const unsigned int bytes = hello(tPtr,via->localSocket(),via->address(),now);
				via->sent(now,bytes);
				root->relayed(now,bytes);
				sent(now,bytes);
				m_lastSentHello = now;
			}
		}
	}
}

void Peer::contact(void *tPtr,const int64_t now,const Endpoint &ep,const bool breakSymmetricBFG1024)
{
	static uint8_t foo = 0;
	RWMutex::Lock l(m_lock);

	if (ep.isInetAddr()&&ep.ip().isV4()) {
		// For IPv4 addresses we send a tiny packet with a low TTL, which helps to
		// traverse some NAT types. It has no effect otherwise. It's important to
		// send this right away in case this is a coordinated attempt via RENDEZVOUS.
		RR->node->putPacket(tPtr,-1,ep.ip(),&foo,1,2);
		++foo;
	}

	const bool wasEmpty = m_tryQueue.empty();
	if (!wasEmpty) {
		for(List<p_TryQueueItem>::iterator i(m_tryQueue.begin());i!=m_tryQueue.end();++i) {
			if (i->target == ep) {
				i->ts = now;
				i->breakSymmetricBFG1024 = breakSymmetricBFG1024;
				return;
			}
		}
	}

#ifdef __CPP11__
	m_tryQueue.emplace_back(now, ep, breakSymmetricBFG1024);
#else
	_tryQueue.push_back(_TryQueueItem(now,ep,breakSymmetricBFG1024));
#endif

	if (wasEmpty)
		m_tryQueuePtr = m_tryQueue.begin();
}

void Peer::resetWithinScope(void *tPtr,InetAddress::IpScope scope,int inetAddressFamily,int64_t now)
{
	RWMutex::Lock l(m_lock);
	unsigned int pc = 0;
	for(unsigned int i=0;i<m_alivePathCount;++i) {
		if ((m_paths[i]) && ((m_paths[i]->address().family() == inetAddressFamily) && (m_paths[i]->address().ipScope() == scope))) {
			const unsigned int bytes = m_sendProbe(tPtr, m_paths[i]->localSocket(), m_paths[i]->address(), nullptr, 0, now);
			m_paths[i]->sent(now, bytes);
			sent(now,bytes);
		} else if (pc != i) {
			m_paths[pc++] = m_paths[i];
		}
	}
	m_alivePathCount = pc;
	while (pc < ZT_MAX_PEER_NETWORK_PATHS)
		m_paths[pc].zero();
}

bool Peer::directlyConnected(int64_t now)
{
	if ((now - m_lastPrioritizedPaths) > ZT_PEER_PRIORITIZE_PATHS_INTERVAL) {
		RWMutex::Lock l(m_lock);
		m_prioritizePaths(now);
		return m_alivePathCount > 0;
	} else {
		RWMutex::RLock l(m_lock);
		return m_alivePathCount > 0;
	}
}

void Peer::getAllPaths(Vector< SharedPtr<Path> > &paths)
{
	RWMutex::RLock l(m_lock);
	paths.clear();
	paths.reserve(m_alivePathCount);
	paths.assign(m_paths, m_paths + m_alivePathCount);
}

void Peer::save(void *tPtr) const
{
	uint8_t buf[8 + ZT_PEER_MARSHAL_SIZE_MAX];

	// Prefix each saved peer with the current timestamp.
	Utils::storeBigEndian<uint64_t>(buf,(uint64_t)RR->node->now());

	const int len = marshal(buf + 8);
	if (len > 0) {
		uint64_t id[2];
		id[0] = m_id.address().toInt();
		id[1] = 0;
		RR->node->stateObjectPut(tPtr,ZT_STATE_OBJECT_PEER,id,buf,(unsigned int)len + 8);
	}
}

int Peer::marshal(uint8_t data[ZT_PEER_MARSHAL_SIZE_MAX]) const noexcept
{
	RWMutex::RLock l(m_lock);

	if (!m_identityKey)
		return -1;

	data[0] = 0; // serialized peer version

	// Include our identity's address to detect if this changes and require
	// recomputation of m_identityKey.
	RR->identity.address().copyTo(data + 1);

	// SECURITY: encryption in place is only to protect secrets if they are
	// cached to local storage. It's not used over the wire. Dumb ECB is fine
	// because secret keys are random and have no structure to reveal.
	RR->localCacheSymmetric.encrypt(m_identityKey->secret,data + 6);
	RR->localCacheSymmetric.encrypt(m_identityKey->secret + 22,data + 17);
	RR->localCacheSymmetric.encrypt(m_identityKey->secret + 38,data + 33);

	int p = 54;

	int s = m_id.marshal(data + p, false);
	if (s < 0)
		return -1;
	p += s;

	s = m_locator.marshal(data + p);
	if (s <= 0)
		return s;
	p += s;

	data[p++] = (uint8_t)m_bootstrap.size();
	for(std::map< Endpoint::Type,Endpoint >::const_iterator i(m_bootstrap.begin());i != m_bootstrap.end();++i) { // NOLINT(modernize-loop-convert,hicpp-use-auto,modernize-use-auto)
		s = i->second.marshal(data + p);
		if (s <= 0)
			return -1;
		p += s;
	}

	Utils::storeBigEndian(data + p,(uint16_t)m_vProto);
	p += 2;
	Utils::storeBigEndian(data + p,(uint16_t)m_vMajor);
	p += 2;
	Utils::storeBigEndian(data + p,(uint16_t)m_vMinor);
	p += 2;
	Utils::storeBigEndian(data + p,(uint16_t)m_vRevision);
	p += 2;

	data[p++] = 0;
	data[p++] = 0;

	return p;
}

int Peer::unmarshal(const uint8_t *restrict data,const int len) noexcept
{
	RWMutex::Lock l(m_lock);

	if ((len <= 54) || (data[0] != 0))
		return -1;

	m_identityKey.zero();
	m_ephemeralKeys[0].zero();
	m_ephemeralKeys[1].zero();

	if (Address(data + 1) == RR->identity.address()) {
		uint8_t k[ZT_SYMMETRIC_KEY_SIZE];
		static_assert(ZT_SYMMETRIC_KEY_SIZE == 48,"marshal() and unmarshal() must be revisited if ZT_SYMMETRIC_KEY_SIZE is changed");
		RR->localCacheSymmetric.decrypt(data + 1,k);
		RR->localCacheSymmetric.decrypt(data + 17,k + 16);
		RR->localCacheSymmetric.decrypt(data + 33,k + 32);
		m_identityKey.set(new SymmetricKey(RR->node->now(),k));
		Utils::burn(k,sizeof(k));
	}

	int p = 49;

	int s = m_id.unmarshal(data + 38, len - 38);
	if (s < 0)
		return s;
	p += s;

	if (!m_identityKey) {
		uint8_t k[ZT_SYMMETRIC_KEY_SIZE];
		if (!RR->identity.agree(m_id,k))
			return -1;
		m_identityKey.set(new SymmetricKey(RR->node->now(),k));
		Utils::burn(k,sizeof(k));
	}

	s = m_locator.unmarshal(data + p, len - p);
	if (s < 0)
		return s;
	p += s;

	if (p >= len)
		return -1;
	const unsigned int bootstrapCount = data[p++];
	if (bootstrapCount > ZT_MAX_PEER_NETWORK_PATHS)
		return -1;
	m_bootstrap.clear();
	for(unsigned int i=0;i<bootstrapCount;++i) {
		Endpoint tmp;
		s = tmp.unmarshal(data + p,len - p);
		if (s < 0)
			return s;
		p += s;
		m_bootstrap[tmp.type()] = tmp;
	}

	if ((p + 10) > len)
		return -1;
	m_vProto = Utils::loadBigEndian<uint16_t>(data + p); p += 2;
	m_vMajor = Utils::loadBigEndian<uint16_t>(data + p); p += 2;
	m_vMinor = Utils::loadBigEndian<uint16_t>(data + p); p += 2;
	m_vRevision = Utils::loadBigEndian<uint16_t>(data + p); p += 2;
	p += 2 + (int)Utils::loadBigEndian<uint16_t>(data + p);

	m_deriveSecondaryIdentityKeys();

	return (p > len) ? -1 : p;
}

struct _PathPriorityComparisonOperator
{
	ZT_INLINE bool operator()(const SharedPtr<Path> &a,const SharedPtr<Path> &b) const noexcept
	{
		// Sort in descending order of most recent receive time.
		return (a->lastIn() > b->lastIn());
	}
};

void Peer::m_prioritizePaths(int64_t now)
{
	// assumes _lock is locked for writing
	m_lastPrioritizedPaths = now;

	if (m_alivePathCount > 0) {
		// Sort paths in descending order of priority.
		std::sort(m_paths, m_paths + m_alivePathCount, _PathPriorityComparisonOperator());

		// Let go of paths that have expired.
		for (unsigned int i = 0;i<ZT_MAX_PEER_NETWORK_PATHS;++i) {
			if ((!m_paths[i]) || (!m_paths[i]->alive(now))) {
				m_alivePathCount = i;
				for (;i < ZT_MAX_PEER_NETWORK_PATHS;++i)
					m_paths[i].zero();
				break;
			}
		}
	}
}

unsigned int Peer::m_sendProbe(void *tPtr,int64_t localSocket,const InetAddress &atAddress,const uint16_t *ports,const unsigned int numPorts,int64_t now)
{
	// Assumes m_lock is locked
	const SharedPtr<SymmetricKey> k(m_key());
	const uint64_t packetId = k->nextMessage(RR->identity.address(),m_id.address());

	uint8_t p[ZT_PROTO_MIN_PACKET_LENGTH + 1];
	Utils::storeAsIsEndian<uint64_t>(p + ZT_PROTO_PACKET_ID_INDEX,packetId);
	m_id.address().copyTo(p + ZT_PROTO_PACKET_DESTINATION_INDEX);
	RR->identity.address().copyTo(p + ZT_PROTO_PACKET_SOURCE_INDEX);
	p[ZT_PROTO_PACKET_FLAGS_INDEX] = 0;
	p[ZT_PROTO_PACKET_VERB_INDEX] = Protocol::VERB_ECHO;
	p[ZT_PROTO_PACKET_VERB_INDEX + 1] = 0; // arbitrary payload

	Protocol::armor(p,ZT_PROTO_MIN_PACKET_LENGTH + 1,k,cipher());

	RR->expect->sending(packetId,now);

	if (numPorts > 0) {
		InetAddress tmp(atAddress);
		for(unsigned int i=0;i<numPorts;++i) {
			tmp.setPort(ports[i]);
			RR->node->putPacket(tPtr,-1,tmp,p,ZT_PROTO_MIN_PACKET_LENGTH + 1);
		}
		return ZT_PROTO_MIN_PACKET_LENGTH * numPorts;
	} else {
		RR->node->putPacket(tPtr,-1,atAddress,p,ZT_PROTO_MIN_PACKET_LENGTH + 1);
		return ZT_PROTO_MIN_PACKET_LENGTH;
	}
}

void Peer::m_deriveSecondaryIdentityKeys() noexcept
{
	uint8_t hk[ZT_SYMMETRIC_KEY_SIZE];
	KBKDFHMACSHA384(m_identityKey->secret,ZT_KBKDF_LABEL_HELLO_DICTIONARY_ENCRYPT,0,0,hk);
	m_helloCipher.init(hk);
	Utils::burn(hk,sizeof(hk));
	KBKDFHMACSHA384(m_identityKey->secret,ZT_KBKDF_LABEL_PACKET_HMAC,0,0,m_helloMacKey);
}

} // namespace ZeroTier
