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

#include "Peer.hpp"

#include "Constants.hpp"
#include "Context.hpp"
#include "Endpoint.hpp"
#include "Expect.hpp"
#include "InetAddress.hpp"
#include "Protocol.hpp"
#include "SelfAwareness.hpp"
#include "Topology.hpp"
#include "Trace.hpp"

namespace ZeroTier {

// An arbitrary byte to send in single byte probes, incremented on each probe.
static uint8_t s_arbitraryByte = (uint8_t)Utils::random();

Peer::Peer()
    : m_key((uintptr_t)&m_identityKey)
    , m_keyRenegotiationNeeded(false)
    , m_lastReceive(0)
    , m_lastSend(0)
    , m_lastSentHello(0)
    , m_lastWhoisRequestReceived(0)
    , m_lastEchoRequestReceived(0)
    , m_lastProbeReceived(0)
    , m_alivePathCount(0)
    , m_bestPath(0)
    , m_vProto(0)
    , m_vMajor(0)
    , m_vMinor(0)
    , m_vRevision(0)
{
}

Peer::~Peer() { Utils::burn(m_helloMacKey, sizeof(m_helloMacKey)); }

bool Peer::init(const Context &ctx, const CallContext &cc, const Identity &peerIdentity)
{
    RWMutex::Lock l(m_lock);

    m_id = peerIdentity;

    uint8_t k[ZT_SYMMETRIC_KEY_SIZE];
    if (unlikely(!ctx.identity.agree(peerIdentity, k)))
        return false;
    m_identityKey.init(cc.ticks, k);
    Utils::burn(k, sizeof(k));

    m_deriveSecondaryIdentityKeys();

    return true;
}

void Peer::received(
    const Context &ctx, const CallContext &cc, const SharedPtr<Path> &path, const unsigned int hops,
    const uint64_t packetId, const unsigned int payloadLength, const Protocol::Verb verb,
    const Protocol::Verb /*inReVerb*/)
{
    m_lastReceive.store(cc.ticks, std::memory_order_relaxed);
    m_inMeter.log(cc.ticks, payloadLength);

    // NOTE: in the most common scenario we will be talking via the best path.
    // This does a check without a full mutex lock and if so there's nothing more
    // to do, which speeds things up in that case.
    if ((hops == 0) && ((uintptr_t)path.ptr() != m_bestPath.load(std::memory_order_relaxed))) {
        RWMutex::RMaybeWLock l(m_lock);

        // If this matches an existing path, skip path learning stuff. For the small number
        // of paths a peer will have linear scan is the fastest way to do lookup.
        for (unsigned int i = 0; i < m_alivePathCount; ++i) {
            if (m_paths[i] == path)
                return;
        }

        // If we made it here, we don't already know this path.
        if (ctx.node->filterPotentialPath(cc.tPtr, m_id, path->localSocket(), path->address())) {
            // SECURITY: note that if we've made it here we expected this OK, see Expect.hpp.
            // There is replay protection in effect for OK responses.
            if (verb == Protocol::VERB_OK) {
                // Acquire write access to the object and thus path set.
                l.writing();

                unsigned int newPathIdx;
                if (m_alivePathCount == ZT_MAX_PEER_NETWORK_PATHS) {
                    m_prioritizePaths(cc);
                    if (m_alivePathCount == ZT_MAX_PEER_NETWORK_PATHS) {
                        newPathIdx = ZT_MAX_PEER_NETWORK_PATHS - 1;
                    }
                    else {
                        newPathIdx = m_alivePathCount++;
                    }
                }
                else {
                    newPathIdx = m_alivePathCount++;
                }

                // Save a reference to the current path in case we replace it. This
                // should technically never happen, but this ensures safety if it does.
                const SharedPtr<Path> currentBest(reinterpret_cast<Path *>(m_bestPath.load(std::memory_order_acquire)));

                SharedPtr<Path> old;
                old.move(m_paths[newPathIdx]);
                m_paths[newPathIdx] = path;

                m_prioritizePaths(cc);

                ctx.t->learnedNewPath(
                    cc, 0x582fabdd, packetId, m_id, path->address(), (old) ? old->address() : InetAddress());
            }
            else {
                int64_t &lt = m_lastTried[Endpoint(path->address())];
                if ((cc.ticks - lt) < ZT_PATH_MIN_TRY_INTERVAL) {
                    lt = cc.ticks;
                    path->sent(cc, m_hello(ctx, cc, path->localSocket(), path->address(), false));
                    ctx.t->tryingNewPath(
                        cc, 0xb7747ddd, m_id, path->address(), path->address(), packetId, (uint8_t)verb, m_id);
                }
            }
        }
    }
}

void Peer::send(const Context &ctx, const CallContext &cc, const void *data, unsigned int len) noexcept
{
    SharedPtr<Path> via(reinterpret_cast<Path *>(m_bestPath.load(std::memory_order_acquire)));
    if (likely(via)) {
        if (likely(via->send(ctx, cc, data, len)))
            this->sent(cc, len);
    }
    else {
        const SharedPtr<Peer> root(ctx.topology->root());
        if (likely((root) && (root.ptr() != this))) {
            via = root->path(cc);
            if (likely(via)) {
                if (likely(via->send(ctx, cc, data, len))) {
                    root->relayed(cc, len);
                    this->sent(cc, len);
                }
            }
        }
    }
}

void Peer::pulse(const Context &ctx, const CallContext &cc)
{
    RWMutex::Lock l(m_lock);

    // Grab current key (this is never NULL).
    SymmetricKey *const key = reinterpret_cast<SymmetricKey *>(m_key.load(std::memory_order_relaxed));

    // Determine if we need a new ephemeral key pair and if a new HELLO needs
    // to be sent. The latter happens every ZT_PEER_HELLO_INTERVAL or if a new
    // ephemeral key pair is generated.
    bool needHello =
        (((m_vProto >= 20)
          && (m_keyRenegotiationNeeded || (key == &m_identityKey)
              || ((cc.ticks - key->timestamp()) >= (ZT_SYMMETRIC_KEY_TTL / 2))
              || (key->odometer() > (ZT_SYMMETRIC_KEY_TTL_MESSAGES / 2))))
         || ((cc.ticks - m_lastSentHello) >= ZT_PEER_HELLO_INTERVAL));

    // Prioritize paths and more importantly for here forget dead ones.
    m_prioritizePaths(cc);

    if (m_tryQueue.empty()) {
        if (m_alivePathCount == 0) {
            // If there are no living paths and nothing in the try queue, try addresses
            // from any locator we have on file or that are fetched via the external API
            // callback (if one was supplied).

            if (m_locator) {
                for (Vector<std::pair<Endpoint, SharedPtr<const Locator::EndpointAttributes>>>::const_iterator ep(
                         m_locator->endpoints().begin());
                     ep != m_locator->endpoints().end(); ++ep) {
                    if (ep->first.type == ZT_ENDPOINT_TYPE_IP_UDP) {
                        if (ctx.node->filterPotentialPath(cc.tPtr, m_id, -1, ep->first.ip())) {
                            int64_t &lt = m_lastTried[ep->first];
                            if ((cc.ticks - lt) > ZT_PATH_MIN_TRY_INTERVAL) {
                                lt = cc.ticks;
                                ctx.t->tryingNewPath(
                                    cc, 0x84b22322, m_id, ep->first.ip(), InetAddress::NIL, 0, 0, Identity::NIL);
                                sent(cc, m_sendProbe(ctx, cc, -1, ep->first.ip(), nullptr, 0));
                            }
                        }
                    }
                }
            }

            InetAddress addr;
            if (ctx.node->externalPathLookup(cc.tPtr, m_id, -1, addr)) {
                if ((addr) && ctx.node->filterPotentialPath(cc.tPtr, m_id, -1, addr)) {
                    int64_t &lt = m_lastTried[Endpoint(addr)];
                    if ((cc.ticks - lt) > ZT_PATH_MIN_TRY_INTERVAL) {
                        lt = cc.ticks;
                        ctx.t->tryingNewPath(cc, 0x84a10000, m_id, addr, InetAddress::NIL, 0, 0, Identity::NIL);
                        sent(cc, m_sendProbe(ctx, cc, -1, addr, nullptr, 0));
                    }
                }
            }
        }
    }
    else {
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
                // TODO: need to send something like a NOP for older target nodes.

                ++attempts;
                if (qi.iteration < 0) {
                    // If iteration is less than zero, try to contact the original address.
                    // It may be set to a larger negative value to try multiple times such
                    // as e.g. -3 to try 3 times.
                    sent(cc, m_sendProbe(ctx, cc, -1, qi.target.ip(), nullptr, 0));
                    ++qi.iteration;
                    goto requeue_item;
                }
                else if (qi.target.ip().isV4() && (m_alivePathCount == 0)) {
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
                    }
                    else {
                        // For un-privileged ports we'll try ZT_NAT_T_PORT_SCAN_MAX ports
                        // beyond the one we were sent to catch some sequentially assigning
                        // symmetric NATs.
                        InetAddress tmp(qi.target.ip());
                        unsigned int p = tmp.port() + 1 + (unsigned int)qi.iteration++;
                        if (p > 65535)
                            p -= 64512;   // wrap back to 1024
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
            else
                continue;

        // If the code skips here the front item is instead moved to the back.
        requeue_item:
            if (m_tryQueue.size() > 1)   // no point in doing this splice if there's only one item
                m_tryQueue.splice(m_tryQueue.end(), m_tryQueue, m_tryQueue.begin());
            if (attempts >= std::min((unsigned int)m_tryQueue.size(), (unsigned int)ZT_NAT_T_PORT_SCAN_MAX))
                break;
            else
                continue;
        }
    }

    // Do keepalive on all currently active paths, sending HELLO to the first
    // if needHello is true and sending small keepalives to others.
    for (unsigned int i = 0; i < m_alivePathCount; ++i) {
        if (needHello) {
            needHello = false;
            const unsigned int bytes =
                m_hello(ctx, cc, m_paths[i]->localSocket(), m_paths[i]->address(), m_keyRenegotiationNeeded);
            if (bytes) {
                m_paths[i]->sent(cc, bytes);
                sent(cc, bytes);
                m_lastSentHello          = cc.ticks;
                m_keyRenegotiationNeeded = false;
            }
        }
        else if ((cc.ticks - m_paths[i]->lastOut()) >= ZT_PATH_KEEPALIVE_PERIOD) {
            m_paths[i]->send(ctx, cc, &s_arbitraryByte, 1);
            ++s_arbitraryByte;
            sent(cc, 1);
        }
    }

    // Send a HELLO indirectly if we were not able to send one via any direct path.
    if (needHello) {
        const SharedPtr<Peer> root(ctx.topology->root());
        if (root) {
            const SharedPtr<Path> via(root->path(cc));
            if (via) {
                const unsigned int bytes =
                    m_hello(ctx, cc, via->localSocket(), via->address(), m_keyRenegotiationNeeded);
                if (bytes) {
                    via->sent(cc, bytes);
                    root->relayed(cc, bytes);
                    sent(cc, bytes);
                    m_lastSentHello          = cc.ticks;
                    m_keyRenegotiationNeeded = false;
                }
            }
        }
    }

    // Clean m_lastTried
    for (Map<Endpoint, int64_t>::iterator i(m_lastTried.begin()); i != m_lastTried.end();) {
        if ((cc.ticks - i->second) > (ZT_PATH_MIN_TRY_INTERVAL * 3))
            m_lastTried.erase(i++);
        else
            ++i;
    }
}

void Peer::contact(const Context &ctx, const CallContext &cc, const Endpoint &ep, int tries)
{
    RWMutex::Lock l(m_lock);

    // See if there's already a path to this endpoint and if so ignore it.
    if (ep.isInetAddr()) {
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
        ctx.cb.wirePacketSendFunction(
            reinterpret_cast<ZT_Node *>(ctx.node), ctx.uPtr, cc.tPtr, -1,
            reinterpret_cast<const ZT_InetAddress *>(&ep.ip()), &s_arbitraryByte, 1, 2);
        ++s_arbitraryByte;
    }

    // Make sure address is not already in the try queue. If so just update it.
    for (List<p_TryQueueItem>::iterator i(m_tryQueue.begin()); i != m_tryQueue.end(); ++i) {
        if (i->target.isSameAddress(ep)) {
            i->target    = ep;
            i->iteration = -tries;
            return;
        }
    }

    m_tryQueue.push_back(p_TryQueueItem(ep, -tries));
}

void Peer::resetWithinScope(
    const Context &ctx, const CallContext &cc, InetAddress::IpScope scope, int inetAddressFamily)
{
    RWMutex::Lock l(m_lock);
    unsigned int pc = 0;
    for (unsigned int i = 0; i < m_alivePathCount; ++i) {
        if ((m_paths[i])
            && (((int)m_paths[i]->address().as.sa.sa_family == inetAddressFamily)
                && (m_paths[i]->address().ipScope() == scope))) {
            const unsigned int bytes =
                m_sendProbe(ctx, cc, m_paths[i]->localSocket(), m_paths[i]->address(), nullptr, 0);
            m_paths[i]->sent(cc, bytes);
            sent(cc, bytes);
        }
        else if (pc != i) {
            m_paths[pc++] = m_paths[i];
        }
    }
    m_alivePathCount = pc;
    while (pc < ZT_MAX_PEER_NETWORK_PATHS)
        m_paths[pc++].zero();
}

void Peer::save(const Context &ctx, const CallContext &cc) const
{
    uint8_t buf[8 + ZT_PEER_MARSHAL_SIZE_MAX];

    // Prefix each saved peer with the current timestamp.
    Utils::storeBigEndian<uint64_t>(buf, (uint64_t)cc.clock);

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

    data[0] = 16;   // serialized peer version

    // Include our identity's address to detect if this changes and require
    // recomputation of m_identityKey.
    ctx.identity.address().copyTo(data + 1);

    // SECURITY: encryption in place is only to protect secrets if they are
    // cached to local storage. It's not used over the wire. Dumb ECB is fine
    // because secret keys are random and have no structure to reveal.
    ctx.localSecretCipher.encrypt(m_identityKey.key(), data + 1 + ZT_ADDRESS_LENGTH);
    ctx.localSecretCipher.encrypt(m_identityKey.key() + 16, data + 1 + ZT_ADDRESS_LENGTH + 16);
    ctx.localSecretCipher.encrypt(m_identityKey.key() + 32, data + 1 + ZT_ADDRESS_LENGTH + 32);

    int p = 1 + ZT_ADDRESS_LENGTH + 48;

    int s = m_id.marshal(data + p, false);
    if (s < 0)
        return -1;
    p += s;

    if (m_locator) {
        data[p++] = 1;
        s         = m_locator->marshal(data + p);
        if (s <= 0)
            return s;
        p += s;
    }
    else {
        data[p++] = 0;
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

    for (unsigned int i = 0; i < ZT_PEER_EPHEMERAL_KEY_BUFFER_SIZE; ++i) {
        m_ephemeralKeysSent[i].creationTime = -1;
        m_ephemeralSessions[i].established  = false;
    }
    m_key.store((uintptr_t)&m_identityKey, std::memory_order_relaxed);

    bool identityKeyRestored = false;
    if (Address(data + 1) == ctx.identity.address()) {
        uint8_t k[ZT_SYMMETRIC_KEY_SIZE];
        static_assert(
            ZT_SYMMETRIC_KEY_SIZE == 48,
            "marshal() and unmarshal() must be revisited if ZT_SYMMETRIC_KEY_SIZE is changed");
        ctx.localSecretCipher.decrypt(data + 1 + ZT_ADDRESS_LENGTH, k);
        ctx.localSecretCipher.decrypt(data + 1 + ZT_ADDRESS_LENGTH + 16, k + 16);
        ctx.localSecretCipher.decrypt(data + 1 + ZT_ADDRESS_LENGTH + 32, k + 32);
        m_identityKey.init(ticks, k);
        Utils::burn(k, sizeof(k));
        identityKeyRestored = true;
    }

    int p = 1 + ZT_ADDRESS_LENGTH + 48;

    int s = m_id.unmarshal(data + p, len - p);
    if (s < 0)
        return s;
    p += s;

    if (!identityKeyRestored) {
        uint8_t k[ZT_SYMMETRIC_KEY_SIZE];
        if (!ctx.identity.agree(m_id, k))
            return -1;
        m_identityKey.init(ticks, k);
        Utils::burn(k, sizeof(k));
    }

    if (p >= len)
        return -1;
    if (data[p] == 0) {
        ++p;
        m_locator.zero();
    }
    else if (data[p] == 1) {
        ++p;
        Locator *const loc = new Locator();
        s                  = loc->unmarshal(data + p, len - p);
        m_locator.set(loc);
        if (s < 0)
            return s;
        p += s;
    }
    else {
        return -1;
    }

    if ((p + 10) > len)
        return -1;
    m_vProto = Utils::loadBigEndian<uint16_t>(data + p);
    p += 2;
    m_vMajor = Utils::loadBigEndian<uint16_t>(data + p);
    p += 2;
    m_vMinor = Utils::loadBigEndian<uint16_t>(data + p);
    p += 2;
    m_vRevision = Utils::loadBigEndian<uint16_t>(data + p);
    p += 2;
    p += 2 + (int)Utils::loadBigEndian<uint16_t>(data + p);

    m_deriveSecondaryIdentityKeys();

    return (p > len) ? -1 : p;
}

struct _PathPriorityComparisonOperator {
    ZT_INLINE bool operator()(const SharedPtr<Path> &a, const SharedPtr<Path> &b) const noexcept
    {
        if (a) {
            if (b)
                return (a->lastIn() > b->lastIn());
            else
                return true;
        }
        else {
            return false;
        }
    }
};

void Peer::m_prioritizePaths(const CallContext &cc)
{
    // assumes m_lock is locked

    // Need to hold the current best just in case we drop it before changing the atomic.
    const SharedPtr<Path> oldBest(reinterpret_cast<Path *>(m_bestPath.load(std::memory_order_acquire)));

    // Clean and reprioritize paths.
    if (m_alivePathCount != 0) {
        unsigned int newCnt = 0;
        for (unsigned int i = 0; i < m_alivePathCount; ++i) {
            if ((m_paths[i]) && (m_paths[i]->alive(cc))) {
                if (i != newCnt)
                    m_paths[newCnt].move(m_paths[i]);
                ++newCnt;
            }
        }
        for (unsigned int i = newCnt; i < m_alivePathCount; ++i)
            m_paths[i].zero();
        m_alivePathCount = newCnt;

        std::sort(m_paths, m_paths + newCnt, _PathPriorityComparisonOperator());
    }

    // Update atomic holding pointer to best path.
    m_bestPath.store((m_alivePathCount != 0) ? (uintptr_t)m_paths[0].ptr() : (uintptr_t)0, std::memory_order_release);
}

unsigned int Peer::m_sendProbe(
    const Context &ctx, const CallContext &cc, int64_t localSocket, const InetAddress &atAddress, const uint16_t *ports,
    const unsigned int numPorts)
{
    // Assumes m_lock is locked

    // SECURITY: we use the long-lived identity key here since this is used for
    // trial contacts, etc. It contains no meaningful payload so who cares if
    // some future attacker compromises it.

    uint8_t p[ZT_PROTO_MIN_PACKET_LENGTH];
    Utils::storeMachineEndian<uint64_t>(
        p + ZT_PROTO_PACKET_ID_INDEX, m_identityKey.nextMessage(ctx.identity.address(), m_id.address()));
    m_id.address().copyTo(p + ZT_PROTO_PACKET_DESTINATION_INDEX);
    ctx.identity.address().copyTo(p + ZT_PROTO_PACKET_SOURCE_INDEX);
    p[ZT_PROTO_PACKET_FLAGS_INDEX] = 0;
    p[ZT_PROTO_PACKET_VERB_INDEX]  = Protocol::VERB_NOP;

    ctx.expect->sending(Protocol::armor(p, ZT_PROTO_MIN_PACKET_LENGTH, m_identityKey, cipher()), cc.ticks);

    if (numPorts > 0) {
        InetAddress tmp(atAddress);
        for (unsigned int i = 0; i < numPorts; ++i) {
            tmp.setPort(ports[i]);
            ctx.cb.wirePacketSendFunction(
                reinterpret_cast<ZT_Node *>(ctx.node), ctx.uPtr, cc.tPtr, -1,
                reinterpret_cast<const ZT_InetAddress *>(&tmp), p, ZT_PROTO_MIN_PACKET_LENGTH, 0);
        }
        return ZT_PROTO_MIN_PACKET_LENGTH * numPorts;
    }
    else {
        ctx.cb.wirePacketSendFunction(
            reinterpret_cast<ZT_Node *>(ctx.node), ctx.uPtr, cc.tPtr, -1,
            reinterpret_cast<const ZT_InetAddress *>(&atAddress), p, ZT_PROTO_MIN_PACKET_LENGTH, 0);
        return ZT_PROTO_MIN_PACKET_LENGTH;
    }
}

void Peer::m_deriveSecondaryIdentityKeys() noexcept
{
    // This is called in init() and unmarshal() to use KBKDF to derive keys
    // for encrypting the dictionary portion of HELLOs and HELLO HMAC from the
    // primary long-lived identity key.

    uint8_t hk[ZT_SYMMETRIC_KEY_SIZE];
    KBKDFHMACSHA384(m_identityKey.key(), ZT_KBKDF_LABEL_HELLO_DICTIONARY_ENCRYPT, 0, 0, hk);
    m_helloCipher.init(hk);
    Utils::burn(hk, sizeof(hk));

    KBKDFHMACSHA384(m_identityKey.key(), ZT_KBKDF_LABEL_PACKET_HMAC, 0, 0, m_helloMacKey);
}

unsigned int Peer::m_hello(
    const Context &ctx, const CallContext &cc, int64_t localSocket, const InetAddress &atAddress,
    const bool forceNewKey)
{
    // assumes m_lock is at least locked for reading

    /* SECURITY: note that HELLO is sent mostly in the clear and always uses
     * the long-lived identity key. This allows us to always bootstrap regardless
     * of ephemeral key state. HELLO contains nothing particularly sensitive,
     * though part of the message is encrypted with another derived key just to
     * conceal things like ephemeral public keys for defense in depth. HELLO is
     * always sent with the old salsa/poly algorithm (but minus salsa of course
     * as it's plaintext), but terminates with an additional HMAC-SHA3
     * authenticator to add extra hardness to the key exchange. The use of HMAC
     * here is also needed to satisfy some FIPS/NIST type requirements. */

    // Pick or generate an ephemeral key to send with this HELLO.
    p_EphemeralPrivate *ephemeral;
    {
        p_EphemeralPrivate *earliest     = m_ephemeralKeysSent;
        p_EphemeralPrivate *latest       = nullptr;
        int64_t earliestEphemeralPrivate = 9223372036854775807LL;
        int64_t latestEphemeralPrivate   = 0;
        for (unsigned int k = 0; k < ZT_PEER_EPHEMERAL_KEY_BUFFER_SIZE; ++k) {
            const int64_t ct = m_ephemeralKeysSent[k].creationTime;
            if (ct <= earliestEphemeralPrivate) {
                earliestEphemeralPrivate = ct;
                earliest                 = m_ephemeralKeysSent + k;
            }
            else if (ct >= latestEphemeralPrivate) {   // creationTime will be -1 if not initialized
                latestEphemeralPrivate = ct;
                latest                 = m_ephemeralKeysSent + k;
            }
        }

        if ((latest != nullptr) && (!forceNewKey) && ((cc.ticks - latest->creationTime) < (ZT_SYMMETRIC_KEY_TTL / 2))) {
            ephemeral = latest;
        }
        else {
            earliest->creationTime = cc.ticks;
            earliest->pub.type     = ZT_PROTO_EPHEMERAL_KEY_TYPE_C25519_P384;
            C25519::generateC25519(earliest->pub.c25519Public, earliest->c25519Private);
            ECC384GenerateKey(earliest->pub.p384Public, earliest->p384Private);
            SHA384(earliest->sha384OfPublic, &earliest->pub, sizeof(earliest->pub));
            ephemeral = earliest;
        }
    }

    // Initialize packet and add basic fields like identity and sent-to address.
    Buf outp;
    const uint64_t packetId = m_identityKey.nextMessage(ctx.identity.address(), m_id.address());
    int ii = Protocol::newPacket(outp, packetId, m_id.address(), ctx.identity.address(), Protocol::VERB_HELLO);
    outp.wI8(ii, ZT_PROTO_VERSION);
    outp.wI8(ii, ZEROTIER_VERSION_MAJOR);
    outp.wI8(ii, ZEROTIER_VERSION_MINOR);
    outp.wI16(ii, ZEROTIER_VERSION_REVISION);
    outp.wI64(ii, (uint64_t)cc.clock);
    outp.wO(ii, ctx.identity);
    outp.wO(ii, atAddress);

    // Add 12 random bytes to act as an IV for the encrypted dictionary field.
    const int ivStart = ii;
    outp.wR(ii, 12);

    // LEGACY: the six reserved bytes after the IV exist for legacy compatibility with v1.x nodes.
    // Once those are dead they'll become just reserved bytes for future use as flags etc.
    outp.wI32(ii, 0);   // reserved bytes
    void *const legacyMoonCountStart = outp.unsafeData + ii;
    outp.wI16(ii, 0);
    const uint64_t legacySalsaIv = packetId & ZT_CONST_TO_BE_UINT64(0xfffffffffffffff8ULL);
    Salsa20(m_identityKey.key(), &legacySalsaIv).crypt12(legacyMoonCountStart, legacyMoonCountStart, 2);

    // Append dictionary containinig meta-data and ephemeral key info.
    const int cryptSectionStart = ii;
    FCV<uint8_t, 2048> md;
    Dictionary::append(md, ZT_PROTO_HELLO_NODE_META_INSTANCE_ID, ctx.instanceId);
    // TODO: add other fields and ephemeral key info
    outp.wI16(ii, (uint16_t)md.size());
    outp.wB(ii, md.data(), (unsigned int)md.size());

    if (unlikely((ii + ZT_HMACSHA384_LEN) > ZT_BUF_SIZE))   // sanity check, should be impossible
        return 0;

    // Encrypt the meta-data dictionary using a derived static key and the IV
    // we generated above. This isn't strictly necessary as the data in there is
    // not "secret," but it's not a bad idea to hide it for defense in depth. In
    // particular this means that the public keys exchanged for ephemeral keying
    // are concealed from any observer.
    AES::CTR ctr(m_helloCipher);
    void *const cryptSection = outp.unsafeData + ii;
    ctr.init(outp.unsafeData + ivStart, 0, cryptSection);
    ctr.crypt(cryptSection, ii - cryptSectionStart);
    ctr.finish();

    // Add HMAC at the end for strong verification by v2 nodes.
    HMACSHA384(m_helloMacKey, outp.unsafeData, ii, outp.unsafeData + ii);
    ii += ZT_HMACSHA384_LEN;

    // Add poly1305 MAC for v1 nodes.
    uint8_t polyKey[ZT_POLY1305_KEY_SIZE], perPacketKey[ZT_SALSA20_KEY_SIZE];
    Protocol::salsa2012DeriveKey(m_identityKey.key(), perPacketKey, outp, ii);
    Salsa20(perPacketKey, &packetId).crypt12(Utils::ZERO256, polyKey, sizeof(polyKey));
    Poly1305 p1305(polyKey);
    p1305.update(
        outp.unsafeData + ZT_PROTO_PACKET_ENCRYPTED_SECTION_START, ii - ZT_PROTO_PACKET_ENCRYPTED_SECTION_START);
    uint64_t polyMac[2];
    p1305.finish(polyMac);
    Utils::storeMachineEndian<uint64_t>(outp.unsafeData + ZT_PROTO_PACKET_MAC_INDEX, polyMac[0]);

    return (likely(
               ctx.cb.wirePacketSendFunction(
                   reinterpret_cast<ZT_Node *>(ctx.node), ctx.uPtr, cc.tPtr, localSocket,
                   reinterpret_cast<const ZT_InetAddress *>(&atAddress), outp.unsafeData, ii, 0)
               == 0))
               ? (unsigned int)ii
               : 0U;
}

}   // namespace ZeroTier
