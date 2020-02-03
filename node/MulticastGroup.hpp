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

#ifndef ZT_MULTICASTGROUP_HPP
#define ZT_MULTICASTGROUP_HPP

#include <cstdint>

#include "Constants.hpp"
#include "MAC.hpp"
#include "InetAddress.hpp"
#include "Utils.hpp"
#include "TriviallyCopyable.hpp"

namespace ZeroTier {

/**
 * A multicast group composed of a multicast MAC and a 32-bit ADI field
 *
 * ADI stands for additional distinguishing information. ADI is primarily for
 * adding additional information to broadcast (ff:ff:ff:ff:ff:ff) memberships,
 * since straight-up broadcast won't scale. Right now it's zero except for
 * IPv4 ARP, where it holds the IPv4 address itself to make ARP into a
 * selective multicast query that can scale.
 *
 * In the future we might add some kind of plugin architecture that can add
 * ADI for things like mDNS (multicast DNS) to improve the selectivity of
 * those protocols.
 *
 * MulticastGroup behaves as an immutable value object.
 */
class MulticastGroup : public TriviallyCopyable
{
public:
	ZT_ALWAYS_INLINE MulticastGroup() : _mac(),_adi(0) {}
	ZT_ALWAYS_INLINE MulticastGroup(const MAC &m,uint32_t a) : _mac(m),_adi(a) {}

	/**
	 * Derive the multicast group used for address resolution (ARP/NDP) for an IP
	 *
	 * @param ip IP address (port field is ignored)
	 * @return Multicast group for ARP/NDP
	 */
	static ZT_ALWAYS_INLINE MulticastGroup deriveMulticastGroupForAddressResolution(const InetAddress &ip)
	{
		if (ip.isV4()) {
			// IPv4 wants broadcast MACs, so we shove the V4 address itself into
			// the Multicast Group ADI field. Making V4 ARP work is basically why
			// ADI was added, as well as handling other things that want mindless
			// Ethernet broadcast to all.
			return MulticastGroup(MAC(0xffffffffffffULL),Utils::ntoh(*((const uint32_t *)ip.rawIpData())));
		} else if (ip.isV6()) {
			// IPv6 is better designed in this respect. We can compute the IPv6
			// multicast address directly from the IP address, and it gives us
			// 24 bits of uniqueness. Collisions aren't likely to be common enough
			// to care about.
			const uint8_t *a = (const uint8_t *)ip.rawIpData();
			return MulticastGroup(MAC(0x33,0x33,0xff,a[13],a[14],a[15]),0);
		}
		return MulticastGroup();
	}

	/**
	 * @return Ethernet MAC portion of multicast group
	 */
	ZT_ALWAYS_INLINE const MAC &mac() const { return _mac; }

	/**
	 * @return Additional distinguishing information, which is normally zero except for IPv4 ARP where it's the IPv4 address
	 */
	ZT_ALWAYS_INLINE uint32_t adi() const { return _adi; }

	ZT_ALWAYS_INLINE bool operator==(const MulticastGroup &g) const { return ((_mac == g._mac)&&(_adi == g._adi)); }
	ZT_ALWAYS_INLINE bool operator!=(const MulticastGroup &g) const { return ((_mac != g._mac)||(_adi != g._adi)); }
	ZT_ALWAYS_INLINE bool operator<(const MulticastGroup &g) const
	{
		if (_mac < g._mac)
			return true;
		else if (_mac == g._mac)
			return (_adi < g._adi);
		return false;
	}
	ZT_ALWAYS_INLINE bool operator>(const MulticastGroup &g) const { return (g < *this); }
	ZT_ALWAYS_INLINE bool operator<=(const MulticastGroup &g) const { return !(g < *this); }
	ZT_ALWAYS_INLINE bool operator>=(const MulticastGroup &g) const { return !(*this < g); }

	ZT_ALWAYS_INLINE unsigned long hashCode() const { return (_mac.hashCode() ^ (unsigned long)_adi); }

private:
	MAC _mac;
	uint32_t _adi;
};

} // namespace ZeroTier

#endif
