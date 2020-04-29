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
#include "Node.hpp"
#include "SelfAwareness.hpp"
#include "InetAddress.hpp"
#include "Protocol.hpp"
#include "Endpoint.hpp"
#include "Expect.hpp"

namespace ZeroTier {

Peer::Peer(const RuntimeEnvironment *renv) :
	RR(renv),
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
	m_probe(0),
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
	m_identityKey.set(new SymmetricKey(RR->node->now(),k,true));
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
					for (unsigned int i=0;i < m_alivePathCount;++i) {
						if ((m_paths[i]->address().family() == path->address().family()) &&
						    (m_paths[i]->localSocket() == path->localSocket()) && // TODO: should be localInterface when multipath is integrated
						    (m_paths[i]->address().ipsEqual2(path->address()))) {
							// Replace older path if everything is the same except the port number, since NAT/firewall reboots
							// and other wacky stuff can change port number assignments.
							m_paths[i] = path;
							return;
						} else if (m_paths[i]->lastIn() > lastReceiveTimeMax) {
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

void Peer::send(void *const tPtr,const int64_t now,const void *const data,const unsigned int len) noexcept
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

unsigned int Peer::hello(void *tPtr,int64_t localSocket,const InetAddress &atAddress,int64_t now)
{
#if 0
	Packet outp(_id.address(),RR->identity.address(),Packet::VERB_HELLO);

	outp.append((unsigned char)ZT_PROTO_VERSION);
	outp.append((unsigned char)ZEROTIER_VERSION_MAJOR);
	outp.append((unsigned char)ZEROTIER_VERSION_MINOR);
	outp.append((uint16_t)ZEROTIER_VERSION_REVISION);
	outp.append(now);
	RR->identity.serialize(outp,false);
	atAddress.serialize(outp);

	RR->node->expectReplyTo(outp.packetId());

	if (atAddress) {
		outp.armor(_key,false); // false == don't encrypt full payload, but add MAC
		RR->node->putPacket(tPtr,localSocket,atAddress,outp.data(),outp.size());
	} else {
		RR->sw->send(tPtr,outp,false); // false == don't encrypt full payload, but add MAC
	}
#endif
}

void Peer::pulse(void *const tPtr,const int64_t now,const bool isRoot)
{
	RWMutex::Lock l(m_lock);

	bool needHello = false;
	if ((now - m_lastSentHello) >= ZT_PEER_HELLO_INTERVAL) {
		m_lastSentHello = now;
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
				sent(now,m_sendProbe(tPtr,-1,addr,now));
			}
		}

		if (!m_bootstrap.empty()) {
			unsigned int tryAtIndex = (unsigned int)Utils::random() % (unsigned int)m_bootstrap.size();
			for(SortedMap< Endpoint::Type,Endpoint >::const_iterator i(m_bootstrap.begin());i != m_bootstrap.end();++i) {
				if (tryAtIndex > 0) {
					--tryAtIndex;
				} else {
					if ((i->second.isInetAddr())&&(!i->second.inetAddr().ipsEqual(addr))) {
						RR->t->tryingNewPath(tPtr, 0x0a009444, m_id, i->second.inetAddr(), InetAddress::NIL, 0, 0, Identity::NIL);
						sent(now,m_sendProbe(tPtr,-1,i->second.inetAddr(),now));
						break;
					}
				}
			}
		}
	}

	m_prioritizePaths(now);

	// Attempt queued paths to try.
	for(int k=0;(k<ZT_NAT_T_MAX_QUEUED_ATTEMPTS_PER_PULSE)&&(!m_tryQueue.empty());++k) {
		if (m_tryQueuePtr == m_tryQueue.end())
			m_tryQueuePtr = m_tryQueue.begin();

		if ((now - m_tryQueuePtr->ts) > ZT_PATH_ALIVE_TIMEOUT) {
			m_tryQueue.erase(m_tryQueuePtr++);
			continue;
		}

		if (m_tryQueuePtr->target.isInetAddr()) {
			// Make sure target does not overlap with any existing path.
			bool duplicate = false;
			for(unsigned int i=0;i<m_alivePathCount;++i) {
				if (m_paths[i]->address() == m_tryQueuePtr->target.inetAddr()) {
					duplicate = true;
					break;
				}
			}
			if (duplicate) {
				m_tryQueue.erase(m_tryQueuePtr++);
				continue;
			}

			if (m_tryQueuePtr->breakSymmetricBFG1024 && RR->node->natMustDie()) {
				// Attempt aggressive NAT traversal if both requested and enabled.
				uint16_t ports[1023];
				for (unsigned int i=0;i<1023;++i)
					ports[i] = (uint64_t)(i + 1);
				for (unsigned int i=0;i<512;++i) {
					const uint64_t rn = Utils::random();
					const unsigned int a = (unsigned int)rn % 1023;
					const unsigned int b = (unsigned int)(rn >> 32U) % 1023;
					if (a != b) {
						uint16_t tmp = ports[a];
						ports[a] = ports[b];
						ports[b] = tmp;
					}
				}
				InetAddress addr(m_tryQueuePtr->target.inetAddr());
				for (unsigned int i=0;i<ZT_NAT_T_BFG1024_PORTS_PER_ATTEMPT;++i) {
					addr.setPort(ports[i]);
					sent(now,m_sendProbe(tPtr,-1,addr,now));
				}
			} else {
				// Otherwise send a normal probe.
				sent(now,m_sendProbe(tPtr, -1, m_tryQueuePtr->target.inetAddr(), now));
			}
		}

		++m_tryQueuePtr;
	}

	// Do keepalive on all currently active paths.
	for(unsigned int i=0;i<m_alivePathCount;++i) {
		if (needHello) {
			needHello = false;
			const unsigned int bytes = hello(tPtr, m_paths[i]->localSocket(), m_paths[i]->address(), now);
			m_paths[i]->sent(now, bytes);
			sent(now,bytes);
		} else if ((now - m_paths[i]->lastOut()) >= ZT_PATH_KEEPALIVE_PERIOD) {
			m_paths[i]->send(RR, tPtr, &now, 1, now);
			sent(now,1);
		}
	}

	// If we need a HELLO and were not able to send one via any other path,
	// send one indirectly.
	if (needHello) {
		const SharedPtr<Peer> root(RR->topology->root());
		if (root) {
			const SharedPtr<Path> via(root->path(now));
			if (via) {
				const unsigned int bytes = hello(tPtr,via->localSocket(),via->address(),now);
				via->sent(now,bytes);
				root->relayed(now,bytes);
				sent(now,bytes);
			}
		}
	}
}

void Peer::tryDirectPath(const int64_t now,const Endpoint &ep,const bool breakSymmetricBFG1024)
{
	RWMutex::Lock l(m_lock);

	for(List<p_TryQueueItem>::iterator i(m_tryQueue.begin());i != m_tryQueue.end();++i) {
		if (i->target == ep) {
			i->ts = now;
			i->breakSymmetricBFG1024 = breakSymmetricBFG1024;
			return;
		}
	}

#ifdef __CPP11__
	m_tryQueue.emplace_back(now, ep, breakSymmetricBFG1024);
#else
	_tryQueue.push_back(_TryQueueItem(now,ep,breakSymmetricBFG1024));
#endif
}

void Peer::resetWithinScope(void *tPtr,InetAddress::IpScope scope,int inetAddressFamily,int64_t now)
{
	RWMutex::Lock l(m_lock);
	unsigned int pc = 0;
	for(unsigned int i=0;i<m_alivePathCount;++i) {
		if ((m_paths[i]) && ((m_paths[i]->address().family() == inetAddressFamily) && (m_paths[i]->address().ipScope() == scope))) {
			const unsigned int bytes = m_sendProbe(tPtr, m_paths[i]->localSocket(), m_paths[i]->address(), now);
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
		m_identityKey.set(new SymmetricKey(RR->node->now(),k,true));
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
		m_identityKey.set(new SymmetricKey(RR->node->now(),k,true));
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

	m_probe = 0; // ephemeral token, reset on unmarshal

	if ((p + 10) > len)
		return -1;
	m_vProto = Utils::loadBigEndian<uint16_t>(data + p); p += 2;
	m_vMajor = Utils::loadBigEndian<uint16_t>(data + p); p += 2;
	m_vMinor = Utils::loadBigEndian<uint16_t>(data + p); p += 2;
	m_vRevision = Utils::loadBigEndian<uint16_t>(data + p); p += 2;
	p += 2 + (int)Utils::loadBigEndian<uint16_t>(data + p);

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

unsigned int Peer::m_sendProbe(void *tPtr,int64_t localSocket,const InetAddress &atAddress,int64_t now)
{
	// Assumes m_lock is locked
	if ((m_vProto < 11)||(m_probe == 0)) {
		const SharedPtr<SymmetricKey> k(m_key());
		const uint64_t packetId = k->nextMessage(RR->identity.address(),m_id.address());

		uint8_t p[ZT_PROTO_MIN_PACKET_LENGTH + 1];
		Utils::storeAsIsEndian<uint64_t>(p + ZT_PROTO_PACKET_ID_INDEX,packetId);
		m_id.address().copyTo(p + ZT_PROTO_PACKET_DESTINATION_INDEX);
		RR->identity.address().copyTo(p + ZT_PROTO_PACKET_SOURCE_INDEX);
		p[ZT_PROTO_PACKET_FLAGS_INDEX] = 0;
		p[ZT_PROTO_PACKET_VERB_INDEX] = Protocol::VERB_ECHO;
		p[ZT_PROTO_PACKET_VERB_INDEX + 1] = (uint8_t)now; // arbitrary byte

		Protocol::armor(p,ZT_PROTO_MIN_PACKET_LENGTH,k,cipher());

		RR->expect->sending(packetId,now);
		RR->node->putPacket(tPtr,-1,atAddress,p,ZT_PROTO_MIN_PACKET_LENGTH);

		return ZT_PROTO_MIN_PACKET_LENGTH;
	} else {
		RR->node->putPacket(tPtr,-1,atAddress,&m_probe,4);
		return 4;
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
