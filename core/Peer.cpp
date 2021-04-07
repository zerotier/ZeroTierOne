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
#include "Context.hpp"
#include "Trace.hpp"
#include "Peer.hpp"
#include "Topology.hpp"
#include "SelfAwareness.hpp"
#include "InetAddress.hpp"
#include "Protocol.hpp"
#include "Endpoint.hpp"
#include "Expect.hpp"

namespace ZeroTier {

Peer::Peer() :
	m_ephemeralPairTimestamp(0),
	m_lastReceive(0),
	m_lastSend(0),
	m_lastSentHello(0),
	m_lastWhoisRequestReceived(0),
	m_lastEchoRequestReceived(0),
	m_lastPrioritizedPaths(0),
	m_lastProbeReceived(0),
	m_alivePathCount(0),
	m_tryQueue(),
	m_vProto(0),
	m_vMajor(0),
	m_vMinor(0),
	m_vRevision(0)
{}

Peer::~Peer()
{ Utils::burn(m_helloMacKey, sizeof(m_helloMacKey)); }

bool Peer::init(const Context &ctx, const CallContext &cc, const Identity &peerIdentity)
{
	RWMutex::Lock l(m_lock);

	if (m_id) // already initialized sanity check
		return false;
	m_id = peerIdentity;

	uint8_t k[ZT_SYMMETRIC_KEY_SIZE];
	if (!ctx.identity.agree(peerIdentity, k))
		return false;
	m_identityKey.set(new SymmetricKey(cc.ticks, k));
	Utils::burn(k, sizeof(k));

	m_deriveSecondaryIdentityKeys();

	return true;
}

void Peer::received(
	const Context &ctx,
	const CallContext &cc,
	const SharedPtr< Path > &path,
	const unsigned int hops,
	const uint64_t packetId,
	const unsigned int payloadLength,
	const Protocol::Verb verb,
	const Protocol::Verb inReVerb)
{
	m_lastReceive = cc.ticks;
	m_inMeter.log(cc.ticks, payloadLength);

	if (hops == 0) {
		RWMutex::RMaybeWLock l(m_lock);

		// If this matches an existing path, skip path learning stuff. For the small number
		// of paths a peer will have linear scan is the fastest way to do lookup.
		for (unsigned int i = 0; i < m_alivePathCount; ++i) {
			if (m_paths[i] == path)
				return;
		}

		// If we made it here, we don't already know this path.
		if (ctx.node->shouldUsePathForZeroTierTraffic(cc.tPtr, m_id, path->localSocket(), path->address())) {
			// SECURITY: note that if we've made it here we expected this OK, see Expect.hpp.
			// There is replay protection in effect for OK responses.
			if (verb == Protocol::VERB_OK) {
				// If we're learning a new path convert the lock to an exclusive write lock.
				l.writing();

				// If the path list is full, replace the least recently active path. Otherwise append new path.
				unsigned int newPathIdx = 0;
				if (m_alivePathCount == ZT_MAX_PEER_NETWORK_PATHS) {
					int64_t lastReceiveTimeMax = 0;
					for (unsigned int i = 0; i < m_alivePathCount; ++i) {
						if ((m_paths[i]->address().as.sa.sa_family == path->address().as.sa.sa_family) &&
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
				m_prioritizePaths(cc);

				// Add or update entry in the endpoint cache. If this endpoint
				// is already present, its timesSeen count is incremented. Otherwise
				// it replaces the lowest ranked entry.
				std::sort(m_endpointCache, m_endpointCache + ZT_PEER_ENDPOINT_CACHE_SIZE);
				Endpoint thisEndpoint(path->address());
				for (unsigned int i = 0;; ++i) {
					if (i == (ZT_PEER_ENDPOINT_CACHE_SIZE - 1)) {
						m_endpointCache[i].target = thisEndpoint;
						m_endpointCache[i].lastSeen = cc.ticks;
						break;
					} else if (m_endpointCache[i].target == thisEndpoint) {
						m_endpointCache[i].lastSeen = cc.ticks;
						break;
					}
				}

				ctx.t->learnedNewPath(cc, 0x582fabdd, packetId, m_id, path->address(), old);
			} else {
				path->sent(cc, hello(ctx, cc, path->localSocket(), path->address()));
				ctx.t->tryingNewPath(cc, 0xb7747ddd, m_id, path->address(), path->address(), packetId, (uint8_t)verb, m_id);
			}
		}
	}
}

void Peer::send(const Context &ctx, const CallContext &cc, const void *data, unsigned int len) noexcept
{
	SharedPtr< Path > via(this->path(cc));
	if (via) {
		via->send(ctx, cc, data, len);
	} else {
		const SharedPtr< Peer > root(ctx.topology->root());
		if ((root) && (root.ptr() != this)) {
			via = root->path(cc);
			if (via) {
				via->send(ctx, cc, data, len);
				root->relayed(cc, len);
			} else {
				return;
			}
		} else {
			return;
		}
	}
	sent(cc, len);
}

unsigned int Peer::hello(const Context &ctx, const CallContext &cc, int64_t localSocket, const InetAddress &atAddress)
{
	Buf outp;

	const uint64_t packetId = m_identityKey->nextMessage(ctx.identity.address(), m_id.address());
	int ii = Protocol::newPacket(outp, packetId, m_id.address(), ctx.identity.address(), Protocol::VERB_HELLO);

	outp.wI8(ii, ZT_PROTO_VERSION);
	outp.wI8(ii, ZEROTIER_VERSION_MAJOR);
	outp.wI8(ii, ZEROTIER_VERSION_MINOR);
	outp.wI16(ii, ZEROTIER_VERSION_REVISION);
	outp.wI64(ii, (uint64_t)cc.clock);
	outp.wO(ii, ctx.identity);
	outp.wO(ii, atAddress);

	const int ivStart = ii;
	outp.wR(ii, 12);

	// LEGACY: the six reserved bytes after the IV exist for legacy compatibility with v1.x nodes.
	// Once those are dead they'll become just reserved bytes for future use as flags etc.
	outp.wI32(ii, 0); // reserved bytes
	void *const legacyMoonCountStart = outp.unsafeData + ii;
	outp.wI16(ii, 0);
	const uint64_t legacySalsaIv = packetId & ZT_CONST_TO_BE_UINT64(0xfffffffffffffff8ULL);
	Salsa20(m_identityKey->secret, &legacySalsaIv).crypt12(legacyMoonCountStart, legacyMoonCountStart, 2);

	const int cryptSectionStart = ii;
	FCV< uint8_t, 4096 > md;
	Dictionary::append(md, ZT_PROTO_HELLO_NODE_META_INSTANCE_ID, ctx.instanceId);
	outp.wI16(ii, (uint16_t)md.size());
	outp.wB(ii, md.data(), (unsigned int)md.size());

	if (unlikely((ii + ZT_HMACSHA384_LEN) > ZT_BUF_SIZE)) // sanity check: should be impossible
		return 0;

	AES::CTR ctr(m_helloCipher);
	void *const cryptSection = outp.unsafeData + ii;
	ctr.init(outp.unsafeData + ivStart, 0, cryptSection);
	ctr.crypt(cryptSection, ii - cryptSectionStart);
	ctr.finish();

	HMACSHA384(m_helloMacKey, outp.unsafeData, ii, outp.unsafeData + ii);
	ii += ZT_HMACSHA384_LEN;

	// LEGACY: we also need Poly1305 for v1.x peers.
	uint8_t polyKey[ZT_POLY1305_KEY_SIZE], perPacketKey[ZT_SALSA20_KEY_SIZE];
	Protocol::salsa2012DeriveKey(m_identityKey->secret, perPacketKey, outp, ii);
	Salsa20(perPacketKey, &packetId).crypt12(Utils::ZERO256, polyKey, sizeof(polyKey));
	Poly1305 p1305(polyKey);
	p1305.update(outp.unsafeData + ZT_PROTO_PACKET_ENCRYPTED_SECTION_START, ii - ZT_PROTO_PACKET_ENCRYPTED_SECTION_START);
	uint64_t polyMac[2];
	p1305.finish(polyMac);
	Utils::storeMachineEndian< uint64_t >(outp.unsafeData + ZT_PROTO_PACKET_MAC_INDEX, polyMac[0]);

	return (likely(ctx.cb.wirePacketSendFunction(reinterpret_cast<ZT_Node *>(ctx.node), ctx.uPtr, cc.tPtr, localSocket, reinterpret_cast<const ZT_InetAddress *>(&atAddress), outp.unsafeData, ii, 0) == 0)) ? ii : 0;
}

void Peer::pulse(const Context &ctx, const CallContext &cc, const bool isRoot)
{
	RWMutex::Lock l(m_lock);

	// Determine if we need a new ephemeral key pair and if a new HELLO needs
	// to be sent. The latter happens every ZT_PEER_HELLO_INTERVAL or if a new
	// ephemeral key pair is generated.
	bool needHello = false;
	if ((m_vProto >= 11) && (((cc.ticks - m_ephemeralPairTimestamp) >= (ZT_SYMMETRIC_KEY_TTL / 2)) || ((m_ephemeralKeys[0]) && (m_ephemeralKeys[0]->odometer() >= (ZT_SYMMETRIC_KEY_TTL_MESSAGES / 2))))) {
		m_ephemeralPair.generate();
		needHello = true;
	} else if ((cc.ticks - m_lastSentHello) >= ZT_PEER_HELLO_INTERVAL) {
		needHello = true;
	}

	// Prioritize paths and more importantly for here forget dead ones.
	m_prioritizePaths(cc);

	if (m_tryQueue.empty()) {
		if (m_alivePathCount == 0) {
			// If there are no living paths and nothing in the try queue, try addresses
			// from any locator we have on file or that are fetched via the external API
			// callback (if one was supplied).

			if (m_locator) {
				for (Vector< std::pair<Endpoint, SharedPtr< const Locator::EndpointAttributes > > >::const_iterator ep(m_locator->endpoints().begin()); ep != m_locator->endpoints().end(); ++ep) {
					if (ep->first.type == ZT_ENDPOINT_TYPE_IP_UDP) {
						if (ctx.node->shouldUsePathForZeroTierTraffic(cc.tPtr, m_id, -1, ep->first.ip())) {
							int64_t &lt = m_lastTried[ep->first];
							if ((cc.ticks - lt) > ZT_PATH_MIN_TRY_INTERVAL) {
								lt = cc.ticks;
								ctx.t->tryingNewPath(cc, 0x84b22322, m_id, ep->first.ip(), InetAddress::NIL, 0, 0, Identity::NIL);
								sent(cc, m_sendProbe(ctx, cc, -1, ep->first.ip(), nullptr, 0));
							}
						}
					}
				}
			}

			for (unsigned int i = 0; i < ZT_PEER_ENDPOINT_CACHE_SIZE; ++i) {
				if ((m_endpointCache[i].lastSeen > 0) && (m_endpointCache[i].target.type == ZT_ENDPOINT_TYPE_IP_UDP)) {
					if (ctx.node->shouldUsePathForZeroTierTraffic(cc.tPtr, m_id, -1, m_endpointCache[i].target.ip())) {
						int64_t &lt = m_lastTried[m_endpointCache[i].target];
						if ((cc.ticks - lt) > ZT_PATH_MIN_TRY_INTERVAL) {
							lt = cc.ticks;
							ctx.t->tryingNewPath(cc, 0x84b22343, m_id, m_endpointCache[i].target.ip(), InetAddress::NIL, 0, 0, Identity::NIL);
							sent(cc, m_sendProbe(ctx, cc, -1, m_endpointCache[i].target.ip(), nullptr, 0));
						}
					}
				}
			}

			InetAddress addr;
			if (ctx.node->externalPathLookup(cc.tPtr, m_id, -1, addr)) {
				if ((addr) && ctx.node->shouldUsePathForZeroTierTraffic(cc.tPtr, m_id, -1, addr)) {
					int64_t &lt = m_lastTried[Endpoint(addr)];
					if ((cc.ticks - lt) > ZT_PATH_MIN_TRY_INTERVAL) {
						lt = cc.ticks;
						ctx.t->tryingNewPath(cc, 0x84a10000, m_id, addr, InetAddress::NIL, 0, 0, Identity::NIL);
						sent(cc, m_sendProbe(ctx, cc, -1, addr, nullptr, 0));
					}
				}
			}
		}
	} else {
		// Attempt up to ZT_NAT_T_MAX_QUEUED_ATTEMPTS_PER_PULSE queued addresses.

		// Note that m_lastTried is checked when contact() is called and something
		// is added to the try queue, not here.

		unsigned int attempts = 0;
		for (;;) {
			p_TryQueueItem &qi = m_tryQueue.front();

			if (qi.target.isInetAddr()) {
				// Skip entry if it overlaps with any currently active IP.
				for (unsigned int i = 0; i < m_alivePathCount; ++i) {
					if (m_paths[i]->address().ipsEqual(qi.target.ip()))
						goto discard_queue_item;
				}
			}

			if (qi.target.type == ZT_ENDPOINT_TYPE_IP_UDP) {
				++attempts;
				if (qi.iteration < 0) {

					// If iteration is less than zero, try to contact the original address.
					// It may be set to a larger negative value to try multiple times such
					// as e.g. -3 to try 3 times.
					sent(cc, m_sendProbe(ctx, cc, -1, qi.target.ip(), nullptr, 0));
					++qi.iteration;
					goto requeue_item;

				} else if (qi.target.ip().isV4() && (m_alivePathCount == 0)) {
					// When iteration reaches zero the queue item is dropped unless it's
					// IPv4 and we have no direct paths. In that case some heavier NAT-t
					// strategies are attempted.

					if (qi.target.ip().port() < 1024) {

						// If the source port is privileged, we actually scan every possible
						// privileged port in random order slowly over multiple iterations
						// of pulse(). This is done in batches of ZT_NAT_T_PORT_SCAN_MAX.
						uint16_t ports[ZT_NAT_T_PORT_SCAN_MAX];
						unsigned int pn = 0;
						while ((pn < ZT_NAT_T_PORT_SCAN_MAX) && (qi.iteration < 1023)) {
							const uint16_t p = ctx.randomPrivilegedPortOrder[qi.iteration++];
							if ((unsigned int)p != qi.target.ip().port())
								ports[pn++] = p;
						}
						if (pn > 0)
							sent(cc, m_sendProbe(ctx, cc, -1, qi.target.ip(), ports, pn));
						if (qi.iteration < 1023)
							goto requeue_item;

					} else {

						// For un-privileged ports we'll try ZT_NAT_T_PORT_SCAN_MAX ports
						// beyond the one we were sent to catch some sequentially assigning
						// symmetric NATs.
						InetAddress tmp(qi.target.ip());
						unsigned int p = tmp.port() + 1 + (unsigned int)qi.iteration++;
						if (p > 65535)
							p -= 64512; // wrap back to 1024
						tmp.setPort(p);
						sent(cc, m_sendProbe(ctx, cc, -1, tmp, nullptr, 0));
						if (qi.iteration < ZT_NAT_T_PORT_SCAN_MAX)
							goto requeue_item;

					}
				}
			}

			// Discard front item unless the code skips to requeue_item.
			discard_queue_item:
			m_tryQueue.pop_front();
			if (attempts >= std::min((unsigned int)m_tryQueue.size(), (unsigned int)ZT_NAT_T_PORT_SCAN_MAX))
				break;
			else continue;

			// If the code skips here the front item is instead moved to the back.
			requeue_item:
			if (m_tryQueue.size() > 1) // no point in doing this splice if there's only one item
				m_tryQueue.splice(m_tryQueue.end(), m_tryQueue, m_tryQueue.begin());
			if (attempts >= std::min((unsigned int)m_tryQueue.size(), (unsigned int)ZT_NAT_T_PORT_SCAN_MAX))
				break;
			else continue;
		}
	}

	// Do keepalive on all currently active paths, sending HELLO to the first
	// if needHello is true and sending small keepalives to others.
	uint64_t randomJunk = Utils::random();
	for (unsigned int i = 0; i < m_alivePathCount; ++i) {
		if (needHello) {
			needHello = false;
			const unsigned int bytes = hello(ctx, cc, m_paths[i]->localSocket(), m_paths[i]->address());
			m_paths[i]->sent(cc, bytes);
			sent(cc, bytes);
			m_lastSentHello = cc.ticks;
		} else if ((cc.ticks - m_paths[i]->lastOut()) >= ZT_PATH_KEEPALIVE_PERIOD) {
			m_paths[i]->send(ctx, cc, reinterpret_cast<uint8_t *>(&randomJunk) + (i & 7U), 1);
			sent(cc, 1);
		}
	}

	// Send a HELLO indirectly if we were not able to send one via any direct path.
	if (needHello) {
		const SharedPtr< Peer > root(ctx.topology->root());
		if (root) {
			const SharedPtr< Path > via(root->path(cc));
			if (via) {
				const unsigned int bytes = hello(ctx, cc, via->localSocket(), via->address());
				via->sent(cc, bytes);
				root->relayed(cc, bytes);
				sent(cc, bytes);
				m_lastSentHello = cc.ticks;
			}
		}
	}

	// Clean m_lastTried
	for (Map< Endpoint, int64_t >::iterator i(m_lastTried.begin()); i != m_lastTried.end();) {
		if ((cc.ticks - i->second) > (ZT_PATH_MIN_TRY_INTERVAL * 4))
			m_lastTried.erase(i++);
		else ++i;
	}
}

void Peer::contact(const Context &ctx, const CallContext &cc, const Endpoint &ep, int tries)
{
	static uint8_t foo = 0;
	RWMutex::Lock l(m_lock);

	// See if there's already a path to this endpoint and if so ignore it.
	if (ep.isInetAddr()) {
		if ((cc.ticks - m_lastPrioritizedPaths) > ZT_PEER_PRIORITIZE_PATHS_INTERVAL) {
			m_prioritizePaths(cc);
		}
		for (unsigned int i = 0; i < m_alivePathCount; ++i) {
			if (m_paths[i]->address().ipsEqual(ep.ip()))
				return;
		}
	}

	// Check underlying path attempt rate limit.
	int64_t &lt = m_lastTried[ep];
	if ((cc.ticks - lt) < ZT_PATH_MIN_TRY_INTERVAL)
		return;
	lt = cc.ticks;

	// For IPv4 addresses we send a tiny packet with a low TTL, which helps to
	// traverse some NAT types. It has no effect otherwise.
	if (ep.isInetAddr() && ep.ip().isV4()) {
		++foo;
		ctx.cb.wirePacketSendFunction(reinterpret_cast<ZT_Node *>(ctx.node), ctx.uPtr, cc.tPtr, -1, reinterpret_cast<const ZT_InetAddress *>(&ep.ip()), &foo, 1, 2);
	}

	// Make sure address is not already in the try queue. If so just update it.
	for (List< p_TryQueueItem >::iterator i(m_tryQueue.begin()); i != m_tryQueue.end(); ++i) {
		if (i->target.isSameAddress(ep)) {
			i->target = ep;
			i->iteration = -tries;
			return;
		}
	}

	m_tryQueue.push_back(p_TryQueueItem(ep, -tries));
}

void Peer::resetWithinScope(const Context &ctx, const CallContext &cc, InetAddress::IpScope scope, int inetAddressFamily)
{
	RWMutex::Lock l(m_lock);
	unsigned int pc = 0;
	for (unsigned int i = 0; i < m_alivePathCount; ++i) {
		if ((m_paths[i]) && (((int)m_paths[i]->address().as.sa.sa_family == inetAddressFamily) && (m_paths[i]->address().ipScope() == scope))) {
			const unsigned int bytes = m_sendProbe(ctx, cc, m_paths[i]->localSocket(), m_paths[i]->address(), nullptr, 0);
			m_paths[i]->sent(cc, bytes);
			sent(cc, bytes);
		} else if (pc != i) {
			m_paths[pc++] = m_paths[i];
		}
	}
	m_alivePathCount = pc;
	while (pc < ZT_MAX_PEER_NETWORK_PATHS)
		m_paths[pc++].zero();
}

bool Peer::directlyConnected(const CallContext &cc)
{
	if ((cc.ticks - m_lastPrioritizedPaths) > ZT_PEER_PRIORITIZE_PATHS_INTERVAL) {
		RWMutex::Lock l(m_lock);
		m_prioritizePaths(cc);
		return m_alivePathCount > 0;
	} else {
		RWMutex::RLock l(m_lock);
		return m_alivePathCount > 0;
	}
}

void Peer::getAllPaths(Vector< SharedPtr< Path > > &paths)
{
	RWMutex::RLock l(m_lock);
	paths.clear();
	paths.reserve(m_alivePathCount);
	paths.assign(m_paths, m_paths + m_alivePathCount);
}

void Peer::save(const Context &ctx, const CallContext &cc) const
{
	uint8_t buf[8 + ZT_PEER_MARSHAL_SIZE_MAX];

	// Prefix each saved peer with the current timestamp.
	Utils::storeBigEndian< uint64_t >(buf, (uint64_t)cc.clock);

	const int len = marshal(ctx, buf + 8);
	if (len > 0) {
		uint64_t id[2];
		id[0] = m_id.address().toInt();
		id[1] = 0;
		ctx.store->put(cc, ZT_STATE_OBJECT_PEER, id, 1, buf, (unsigned int)len + 8);
	}
}

int Peer::marshal(const Context &ctx, uint8_t data[ZT_PEER_MARSHAL_SIZE_MAX]) const noexcept
{
	RWMutex::RLock l(m_lock);

	if (!m_identityKey)
		return -1;

	data[0] = 16; // serialized peer version

	// Include our identity's address to detect if this changes and require
	// recomputation of m_identityKey.
	ctx.identity.address().copyTo(data + 1);

	// SECURITY: encryption in place is only to protect secrets if they are
	// cached to local storage. It's not used over the wire. Dumb ECB is fine
	// because secret keys are random and have no structure to reveal.
	ctx.localSecretCipher.encrypt(m_identityKey->secret, data + 1 + ZT_ADDRESS_LENGTH);
	ctx.localSecretCipher.encrypt(m_identityKey->secret + 16, data + 1 + ZT_ADDRESS_LENGTH + 16);
	ctx.localSecretCipher.encrypt(m_identityKey->secret + 32, data + 1 + ZT_ADDRESS_LENGTH + 32);

	int p = 1 + ZT_ADDRESS_LENGTH + 48;

	int s = m_id.marshal(data + p, false);
	if (s < 0)
		return -1;
	p += s;

	if (m_locator) {
		data[p++] = 1;
		s = m_locator->marshal(data + p);
		if (s <= 0)
			return s;
		p += s;
	} else {
		data[p++] = 0;
	}

	unsigned int cachedEndpointCount = 0;
	for (unsigned int i = 0; i < ZT_PEER_ENDPOINT_CACHE_SIZE; ++i) {
		if (m_endpointCache[i].lastSeen > 0)
			++cachedEndpointCount;
	}
	Utils::storeBigEndian(data + p, (uint16_t)cachedEndpointCount);
	p += 2;
	for (unsigned int i = 0; i < ZT_PEER_ENDPOINT_CACHE_SIZE; ++i) {
		Utils::storeBigEndian(data + p, (uint64_t)m_endpointCache[i].lastSeen);
		s = m_endpointCache[i].target.marshal(data + p);
		if (s <= 0)
			return -1;
		p += s;
	}

	Utils::storeBigEndian(data + p, (uint16_t)m_vProto);
	p += 2;
	Utils::storeBigEndian(data + p, (uint16_t)m_vMajor);
	p += 2;
	Utils::storeBigEndian(data + p, (uint16_t)m_vMinor);
	p += 2;
	Utils::storeBigEndian(data + p, (uint16_t)m_vRevision);
	p += 2;

	data[p++] = 0;
	data[p++] = 0;

	return p;
}

int Peer::unmarshal(const Context &ctx, const int64_t ticks, const uint8_t *restrict data, const int len) noexcept
{
	RWMutex::Lock l(m_lock);

	if ((len <= (1 + ZT_ADDRESS_LENGTH + 48)) || (data[0] != 16))
		return -1;

	m_identityKey.zero();
	m_ephemeralKeys[0].zero();
	m_ephemeralKeys[1].zero();

	if (Address(data + 1) == ctx.identity.address()) {
		uint8_t k[ZT_SYMMETRIC_KEY_SIZE];
		static_assert(ZT_SYMMETRIC_KEY_SIZE == 48, "marshal() and unmarshal() must be revisited if ZT_SYMMETRIC_KEY_SIZE is changed");
		ctx.localSecretCipher.decrypt(data + 1 + ZT_ADDRESS_LENGTH, k);
		ctx.localSecretCipher.decrypt(data + 1 + ZT_ADDRESS_LENGTH + 16, k + 16);
		ctx.localSecretCipher.decrypt(data + 1 + ZT_ADDRESS_LENGTH + 32, k + 32);
		m_identityKey.set(new SymmetricKey(ticks, k));
		Utils::burn(k, sizeof(k));
	}

	int p = 1 + ZT_ADDRESS_LENGTH + 48;

	int s = m_id.unmarshal(data + p, len - p);
	if (s < 0)
		return s;
	p += s;

	if (!m_identityKey) {
		uint8_t k[ZT_SYMMETRIC_KEY_SIZE];
		if (!ctx.identity.agree(m_id, k))
			return -1;
		m_identityKey.set(new SymmetricKey(ticks, k));
		Utils::burn(k, sizeof(k));
	}

	if (p >= len)
		return -1;
	if (data[p] == 0) {
		++p;
		m_locator.zero();
	} else if (data[p] == 1) {
		++p;
		Locator *const loc = new Locator();
		s = loc->unmarshal(data + p, len - p);
		m_locator.set(loc);
		if (s < 0)
			return s;
		p += s;
	} else {
		return -1;
	}

	const unsigned int cachedEndpointCount = Utils::loadBigEndian< uint16_t >(data + p);
	p += 2;
	for (unsigned int i = 0; i < cachedEndpointCount; ++i) {
		if (i < ZT_PEER_ENDPOINT_CACHE_SIZE) {
			if ((p + 8) >= len)
				return -1;
			m_endpointCache[i].lastSeen = (int64_t)Utils::loadBigEndian< uint64_t >(data + p);
			p += 8;
			s = m_endpointCache[i].target.unmarshal(data + p, len - p);
			if (s <= 0)
				return -1;
			p += s;
		}
	}

	if ((p + 10) > len)
		return -1;
	m_vProto = Utils::loadBigEndian< uint16_t >(data + p);
	p += 2;
	m_vMajor = Utils::loadBigEndian< uint16_t >(data + p);
	p += 2;
	m_vMinor = Utils::loadBigEndian< uint16_t >(data + p);
	p += 2;
	m_vRevision = Utils::loadBigEndian< uint16_t >(data + p);
	p += 2;
	p += 2 + (int)Utils::loadBigEndian< uint16_t >(data + p);

	m_deriveSecondaryIdentityKeys();

	return (p > len) ? -1 : p;
}

struct _PathPriorityComparisonOperator
{
	ZT_INLINE bool operator()(const SharedPtr< Path > &a, const SharedPtr< Path > &b) const noexcept
	{
		// Sort in descending order of most recent receive time.
		return (a->lastIn() > b->lastIn());
	}
};

void Peer::m_prioritizePaths(const CallContext &cc)
{
	// assumes _lock is locked for writing
	m_lastPrioritizedPaths = cc.ticks;

	if (m_alivePathCount > 0) {
		// Sort paths in descending order of priority.
		std::sort(m_paths, m_paths + m_alivePathCount, _PathPriorityComparisonOperator());

		// Let go of paths that have expired.
		for (unsigned int i = 0; i < ZT_MAX_PEER_NETWORK_PATHS; ++i) {
			if ((!m_paths[i]) || (!m_paths[i]->alive(cc))) {
				m_alivePathCount = i;
				for (; i < ZT_MAX_PEER_NETWORK_PATHS; ++i)
					m_paths[i].zero();
				break;
			}
		}
	}
}

unsigned int Peer::m_sendProbe(const Context &ctx, const CallContext &cc, int64_t localSocket, const InetAddress &atAddress, const uint16_t *ports, const unsigned int numPorts)
{
	// Assumes m_lock is locked
	const SharedPtr< SymmetricKey > k(m_key());
	const uint64_t packetId = k->nextMessage(ctx.identity.address(), m_id.address());

	uint8_t p[ZT_PROTO_MIN_PACKET_LENGTH];
	Utils::storeMachineEndian< uint64_t >(p + ZT_PROTO_PACKET_ID_INDEX, packetId);
	m_id.address().copyTo(p + ZT_PROTO_PACKET_DESTINATION_INDEX);
	ctx.identity.address().copyTo(p + ZT_PROTO_PACKET_SOURCE_INDEX);
	p[ZT_PROTO_PACKET_FLAGS_INDEX] = 0;
	p[ZT_PROTO_PACKET_VERB_INDEX] = Protocol::VERB_ECHO;

	Protocol::armor(p, ZT_PROTO_MIN_PACKET_LENGTH, k, cipher());

	ctx.expect->sending(packetId, cc.ticks);

	if (numPorts > 0) {
		InetAddress tmp(atAddress);
		for (unsigned int i = 0; i < numPorts; ++i) {
			tmp.setPort(ports[i]);
			ctx.cb.wirePacketSendFunction(reinterpret_cast<ZT_Node *>(ctx.node), ctx.uPtr, cc.tPtr, -1, reinterpret_cast<const ZT_InetAddress *>(&tmp), p, ZT_PROTO_MIN_PACKET_LENGTH, 0);
		}
		return ZT_PROTO_MIN_PACKET_LENGTH * numPorts;
	} else {
		ctx.cb.wirePacketSendFunction(reinterpret_cast<ZT_Node *>(ctx.node), ctx.uPtr, cc.tPtr, -1, reinterpret_cast<const ZT_InetAddress *>(&atAddress), p, ZT_PROTO_MIN_PACKET_LENGTH, 0);
		return ZT_PROTO_MIN_PACKET_LENGTH;
	}
}

void Peer::m_deriveSecondaryIdentityKeys() noexcept
{
	uint8_t hk[ZT_SYMMETRIC_KEY_SIZE];
	KBKDFHMACSHA384(m_identityKey->secret, ZT_KBKDF_LABEL_HELLO_DICTIONARY_ENCRYPT, 0, 0, hk);
	m_helloCipher.init(hk);
	Utils::burn(hk, sizeof(hk));
	KBKDFHMACSHA384(m_identityKey->secret, ZT_KBKDF_LABEL_PACKET_HMAC, 0, 0, m_helloMacKey);
}

} // namespace ZeroTier
