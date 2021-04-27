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

#ifndef ZT_NETWORK_HPP
#define ZT_NETWORK_HPP

#include "Address.hpp"
#include "Buf.hpp"
#include "CallContext.hpp"
#include "Constants.hpp"
#include "Containers.hpp"
#include "Dictionary.hpp"
#include "MAC.hpp"
#include "Member.hpp"
#include "MembershipCredential.hpp"
#include "MulticastGroup.hpp"
#include "Mutex.hpp"
#include "NetworkConfig.hpp"
#include "SharedPtr.hpp"

#define ZT_NETWORK_MAX_INCOMING_UPDATES 3

namespace ZeroTier {

class Context;

class Peer;

/**
 * A virtual LAN
 */
class Network {
    friend class SharedPtr<Network>;

  public:
    /**
     * Broadcast multicast group: ff:ff:ff:ff:ff:ff / 0
     */
    static const MulticastGroup BROADCAST;

    /**
     * Compute primary controller device ID from network ID
     */
    static ZT_INLINE Address controllerFor(uint64_t nwid) noexcept
    {
        return Address(nwid >> 24U);
    }

    /**
     * Construct a new network
     *
     * Note that init() should be called immediately after the network is
     * constructed to actually configure the port.
     *
     * @param nwid Network ID
     * @param controllerFingerprint Initial controller fingerprint if non-NULL
     * @param uptr Arbitrary pointer used by externally-facing API (for user use)
     * @param nconf Network config, if known
     */
    Network(const Context& ctx, const CallContext& cc, uint64_t nwid, const Fingerprint& controllerFingerprint, void* uptr, const NetworkConfig* nconf);

    ~Network();

    ZT_INLINE uint64_t id() const noexcept
    {
        return m_id;
    }

    ZT_INLINE Address controller() const noexcept
    {
        return Address(m_id >> 24U);
    }

    ZT_INLINE bool multicastEnabled() const noexcept
    {
        return (m_config.multicastLimit > 0);
    }

    ZT_INLINE bool hasConfig() const noexcept
    {
        return (m_config);
    }

    ZT_INLINE uint64_t lastConfigUpdate() const noexcept
    {
        return m_lastConfigUpdate;
    }

    ZT_INLINE ZT_VirtualNetworkStatus status() const noexcept
    {
        return m_status();
    }

    ZT_INLINE const NetworkConfig& config() const noexcept
    {
        return m_config;
    }

    ZT_INLINE const MAC& mac() const noexcept
    {
        return m_mac;
    }

    /**
     * Apply filters to an outgoing packet
     *
     * This applies filters from our network config and, if that doesn't match,
     * our capabilities in ascending order of capability ID. Additional actions
     * such as TEE may be taken, and credentials may be pushed, so this is not
     * side-effect-free. It's basically step one in sending something over VL2.
     *
     * @param noTee If true, do not TEE anything anywhere (for two-pass filtering as done with multicast and bridging)
     * @param ztSource Source ZeroTier address
     * @param ztDest Destination ZeroTier address
     * @param macSource Ethernet layer source address
     * @param macDest Ethernet layer destination address
     * @param frameData Ethernet frame data
     * @param frameLen Ethernet frame payload length
     * @param etherType 16-bit ethernet type ID
     * @param vlanId 16-bit VLAN ID
     * @return True if packet should be sent, false if dropped or redirected
     */
    bool filterOutgoingPacket(
        const CallContext& cc,
        bool noTee,
        const Address& ztSource,
        const Address& ztDest,
        const MAC& macSource,
        const MAC& macDest,
        const uint8_t* frameData,
        unsigned int frameLen,
        unsigned int etherType,
        unsigned int vlanId,
        uint8_t& qosBucket);

    /**
     * Apply filters to an incoming packet
     *
     * This applies filters from our network config and, if that doesn't match,
     * the peer's capabilities in ascending order of capability ID. If there is
     * a match certain actions may be taken such as sending a copy of the packet
     * to a TEE or REDIRECT target.
     *
     * @param sourcePeer Source Peer
     * @param ztDest Destination ZeroTier address
     * @param macSource Ethernet layer source address
     * @param macDest Ethernet layer destination address
     * @param frameData Ethernet frame data
     * @param frameLen Ethernet frame payload length
     * @param etherType 16-bit ethernet type ID
     * @param vlanId 16-bit VLAN ID
     * @return 0 == drop, 1 == accept, 2 == accept even if bridged
     */
    int filterIncomingPacket(
        const CallContext& cc,
        const SharedPtr<Peer>& sourcePeer,
        const Address& ztDest,
        const MAC& macSource,
        const MAC& macDest,
        const uint8_t* frameData,
        unsigned int frameLen,
        unsigned int etherType,
        unsigned int vlanId);

    /**
     * Subscribe to a multicast group
     *
     * @param mg New multicast group
     */
    void multicastSubscribe(const CallContext& cc, const MulticastGroup& mg);

    /**
     * Unsubscribe from a multicast group
     *
     * @param mg Multicast group
     */
    void multicastUnsubscribe(const MulticastGroup& mg);

    /**
     * Parse, verify, and handle an inbound network config chunk
     *
     * This is called from IncomingPacket to handle incoming network config
     * chunks via OK(NETWORK_CONFIG_REQUEST) or NETWORK_CONFIG. It's a common
     * bit of packet parsing code that also verifies chunks and replicates
     * them (via rumor mill flooding) if their fast propagate flag is set.
     *
     * @param packetId Packet ID or 0 if none (e.g. via cluster path)
     * @param source Peer that actually sent this chunk (probably controller)
     * @param chunk Buffer containing chunk
     * @param ptr Index of chunk and related fields in packet (starting with network ID)
     * @param size Size of data in chunk buffer (total, not relative to ptr)
     * @return Update ID if update was fully assembled and accepted or 0 otherwise
     */
    uint64_t handleConfigChunk(const CallContext& cc, uint64_t packetId, const SharedPtr<Peer>& source, const Buf& chunk, int ptr, int size);

    /**
     * Set network configuration
     *
     * This is normally called internally when a configuration is received
     * and fully assembled, but it can also be called on Node startup when
     * cached configurations are re-read from the data store.
     *
     * @param nconf Network configuration
     * @param saveToDisk Save to disk? Used during loading, should usually be true otherwise.
     * @return 0 == bad, 1 == accepted but duplicate/unchanged, 2 == accepted and new
     */
    int setConfiguration(const CallContext& cc, const NetworkConfig& nconf, bool saveToDisk);

    /**
     * Set netconf failure to 'access denied' -- called in IncomingPacket when controller reports this
     */
    ZT_INLINE void setAccessDenied() noexcept
    {
        _netconfFailure = NETCONF_FAILURE_ACCESS_DENIED;
    }

    /**
     * Set netconf failure to 'not found' -- called by IncomingPacket when controller reports this
     */
    ZT_INLINE void setNotFound() noexcept
    {
        _netconfFailure = NETCONF_FAILURE_NOT_FOUND;
    }

    /**
     * Determine whether this peer is permitted to communicate on this network
     *
     * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
     * @param peer Peer to check
     */
    bool gate(void* tPtr, const SharedPtr<Peer>& peer) noexcept;

    /**
     * Do periodic cleanup and housekeeping tasks
     */
    void doPeriodicTasks(const CallContext& cc);

    /**
     * Find the node on this network that has this MAC behind it (if any)
     *
     * @param mac MAC address
     * @return ZeroTier address of bridge to this MAC
     */
    ZT_INLINE Address findBridgeTo(const MAC& mac) const
    {
        Mutex::Lock _l(m_remoteBridgeRoutes_l);
        Map<MAC, Address>::const_iterator br(m_remoteBridgeRoutes.find(mac));
        return ((br == m_remoteBridgeRoutes.end()) ? Address() : br->second);
    }

    /**
     * Set a bridge route
     *
     * @param mac MAC address of destination
     * @param addr Bridge this MAC is reachable behind
     */
    void learnBridgeRoute(const MAC& mac, const Address& addr);

    /**
     * Learn a multicast group that is bridged to our tap device
     *
     * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
     * @param mg Multicast group
     * @param now Current time
     */
    ZT_INLINE void learnBridgedMulticastGroup(const MulticastGroup& mg, int64_t now)
    {
        Mutex::Lock l(m_myMulticastGroups_l);
        m_multicastGroupsBehindMe[mg] = now;
    }

    /**
     * Validate a credential and learn it if it passes certificate and other checks
     */
    Member::AddCredentialResult addCredential(const CallContext& cc, const Identity& sourcePeerIdentity, const MembershipCredential& com);

    /**
     * Validate a credential and learn it if it passes certificate and other checks
     */
    Member::AddCredentialResult addCredential(const CallContext& cc, const Identity& sourcePeerIdentity, const CapabilityCredential& cap);

    /**
     * Validate a credential and learn it if it passes certificate and other checks
     */
    Member::AddCredentialResult addCredential(const CallContext& cc, const Identity& sourcePeerIdentity, const TagCredential& tag);

    /**
     * Validate a credential and learn it if it passes certificate and other checks
     */
    Member::AddCredentialResult addCredential(const CallContext& cc, const Identity& sourcePeerIdentity, const RevocationCredential& rev);

    /**
     * Validate a credential and learn it if it passes certificate and other checks
     */
    Member::AddCredentialResult addCredential(const CallContext& cc, const Identity& sourcePeerIdentity, const OwnershipCredential& coo);

    /**
     * Push credentials to a peer if timeouts indicate that we should do so
     *
     * @param to Destination peer
     */
    void pushCredentials(const CallContext& cc, const SharedPtr<Peer>& to);

    /**
     * Destroy this network
     *
     * This sets the network to completely remove itself on delete. This also prevents the
     * call of the normal port shutdown event on delete.
     */
    void destroy();

    /**
     * Get this network's config for export via the ZT core API
     *
     * @param ec Buffer to fill with externally-visible network configuration
     */
    void externalConfig(ZT_VirtualNetworkConfig* ec) const;

    /**
     * Iterate through memberships
     *
     * @param f Function of (const Address,const Membership)
     */
    template <typename F> ZT_INLINE void eachMember(F f)
    {
        Mutex::Lock ml(m_memberships_l);
        for (Map<Address, Member>::iterator i(m_memberships.begin()); i != m_memberships.end(); ++i) {   // NOLINT(modernize-loop-convert,hicpp-use-auto,modernize-use-auto)
            if (! f(i->first, i->second))
                break;
        }
    }

    /**
     * @return Externally usable pointer-to-pointer exported via the core API
     */
    ZT_INLINE void** userPtr() noexcept
    {
        return &m_uPtr;
    }

  private:
    void m_requestConfiguration(const CallContext& cc);
    ZT_VirtualNetworkStatus m_status() const;
    void m_externalConfig(ZT_VirtualNetworkConfig* ec) const;   // assumes _lock is locked
    void m_announceMulticastGroups(void* tPtr, bool force);
    void m_announceMulticastGroupsTo(void* tPtr, const Address& peer, const Vector<MulticastGroup>& allMulticastGroups);
    Vector<MulticastGroup> m_allMulticastGroups() const;

    const Context& m_ctx;
    void* m_uPtr;
    const uint64_t m_id;
    Fingerprint m_controllerFingerprint;
    MAC m_mac;   // local MAC address
    bool m_portInitialized;
    std::atomic<bool> m_destroyed;

    Vector<MulticastGroup> m_myMulticastGroups;               // multicast groups that we belong to (according to tap)
    Map<MulticastGroup, int64_t> m_multicastGroupsBehindMe;   // multicast groups that seem to be behind us and when we
                                                              // last saw them (if we are a bridge)
    Map<MAC, Address> m_remoteBridgeRoutes;                   // remote addresses where given MACs are reachable (for tracking devices
                                                              // behind remote bridges)

    NetworkConfig m_config;
    std::atomic<int64_t> m_lastConfigUpdate;

    volatile enum { NETCONF_FAILURE_NONE, NETCONF_FAILURE_ACCESS_DENIED, NETCONF_FAILURE_NOT_FOUND, NETCONF_FAILURE_INIT_FAILED } _netconfFailure;

    Map<Address, Member> m_memberships;

    Mutex m_myMulticastGroups_l;
    Mutex m_remoteBridgeRoutes_l;
    Mutex m_config_l;
    Mutex m_memberships_l;

    std::atomic<int> __refCount;
};

}   // namespace ZeroTier

#endif
