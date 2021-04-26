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

#ifndef ZT_SELFAWARENESS_HPP
#define ZT_SELFAWARENESS_HPP

#include "Address.hpp"
#include "CallContext.hpp"
#include "Constants.hpp"
#include "Containers.hpp"
#include "InetAddress.hpp"
#include "Mutex.hpp"

namespace ZeroTier {

class Identity;
class Context;

/**
 * SelfAwareness manages awareness of this peer's external address(es) and NAT situation.
 *
 * Name aside, it shouldn't be capable of achieving sentience.
 */
class SelfAwareness {
  public:
    explicit SelfAwareness(const Context& ctx);

    /**
     * Called when a remote peer informs us of our external network address
     *
     * @param reporter Identity of reporting peer
     * @param receivedOnLocalAddress Local address on which report was received
     * @param reporterPhysicalAddress Physical address that reporting peer seems to have
     * @param myPhysicalAddress Physical address that peer says we have
     * @param trusted True if this peer is trusted as an authority to inform us of external address changes
     */
    void
    iam(const CallContext& cc,
        const Identity& reporter,
        int64_t receivedOnLocalSocket,
        const InetAddress& reporterPhysicalAddress,
        const InetAddress& myPhysicalAddress,
        bool trusted);

    /**
     * Clean up database periodically
     */
    void clean(const CallContext& cc);

    /**
     * Get external address consensus, which is the statistical "mode" of external addresses.
     *
     * @return Map of count to IP/port representing how many endpoints reported each address
     */
    MultiMap<unsigned int, InetAddress> externalAddresses(CallContext& cc) const;

  private:
    struct p_PhySurfaceKey {
        Address reporter;
        int64_t receivedOnLocalSocket;
        InetAddress reporterPhysicalAddress;
        InetAddress::IpScope scope;

        ZT_INLINE p_PhySurfaceKey() noexcept
        {
        }

        ZT_INLINE
        p_PhySurfaceKey(const Address& r, const int64_t rol, const InetAddress& ra, InetAddress::IpScope s) noexcept
            : reporter(r)
            , receivedOnLocalSocket(rol)
            , reporterPhysicalAddress(ra)
            , scope(s)
        {
        }

        ZT_INLINE unsigned long hashCode() const noexcept
        {
            return ((unsigned long)reporter.toInt() + (unsigned long)receivedOnLocalSocket + (unsigned long)scope);
        }

        ZT_INLINE bool operator==(const p_PhySurfaceKey& k) const noexcept
        {
            return (
                (reporter == k.reporter) && (receivedOnLocalSocket == k.receivedOnLocalSocket)
                && (reporterPhysicalAddress == k.reporterPhysicalAddress) && (scope == k.scope));
        }

        ZT_INLINE bool operator!=(const p_PhySurfaceKey& k) const noexcept
        {
            return (! (*this == k));
        }

        ZT_INLINE bool operator<(const p_PhySurfaceKey& k) const noexcept
        {
            if (reporter < k.reporter) {
                return true;
            }
            else if (reporter == k.reporter) {
                if (receivedOnLocalSocket < k.receivedOnLocalSocket) {
                    return true;
                }
                else if (receivedOnLocalSocket == k.receivedOnLocalSocket) {
                    if (reporterPhysicalAddress < k.reporterPhysicalAddress) {
                        return true;
                    }
                    else if (reporterPhysicalAddress == k.reporterPhysicalAddress) {
                        return scope < k.scope;
                    }
                }
            }
            return false;
        }
    };

    struct p_PhySurfaceEntry {
        InetAddress mySurface;
        int64_t timestampTicks;
        bool trusted;

        ZT_INLINE p_PhySurfaceEntry() noexcept
            : mySurface()
            , timestampTicks(0)
            , trusted(false)
        {
        }

        ZT_INLINE p_PhySurfaceEntry(const InetAddress& a, const int64_t t) noexcept
            : mySurface(a)
            , timestampTicks(t)
            , trusted(false)
        {
        }
    };

    const Context& m_ctx;
    Map<p_PhySurfaceKey, p_PhySurfaceEntry> m_phy;
    Mutex m_phy_l;
};

}   // namespace ZeroTier

#endif
