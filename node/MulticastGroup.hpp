/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifndef ZT_MULTICASTGROUP_HPP
#define ZT_MULTICASTGROUP_HPP

#include <stdint.h>

#include "MAC.hpp"
#include "InetAddress.hpp"

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
class MulticastGroup
{
public:
	MulticastGroup() :
		_mac(),
		_adi(0)
	{
	}

	MulticastGroup(const MAC &m,uint32_t a) :
		_mac(m),
		_adi(a)
	{
	}

	/**
	 * Derive the multicast group used for address resolution (ARP/NDP) for an IP
	 *
	 * @param ip IP address (port field is ignored)
	 * @return Multicast group for ARP/NDP
	 */
	static inline MulticastGroup deriveMulticastGroupForAddressResolution(const InetAddress &ip)
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
			const unsigned char *a = (const unsigned char *)ip.rawIpData();
			return MulticastGroup(MAC(0x33,0x33,0xff,a[13],a[14],a[15]),0);
		}
		return MulticastGroup();
	}

	/**
	 * @return Multicast address
	 */
	inline const MAC &mac() const { return _mac; }

	/**
	 * @return Additional distinguishing information
	 */
	inline uint32_t adi() const { return _adi; }

	inline unsigned long hashCode() const { return (_mac.hashCode() ^ (unsigned long)_adi); }

	inline bool operator==(const MulticastGroup &g) const { return ((_mac == g._mac)&&(_adi == g._adi)); }
	inline bool operator!=(const MulticastGroup &g) const { return ((_mac != g._mac)||(_adi != g._adi)); }
	inline bool operator<(const MulticastGroup &g) const
	{
		if (_mac < g._mac)
			return true;
		else if (_mac == g._mac)
			return (_adi < g._adi);
		return false;
	}
	inline bool operator>(const MulticastGroup &g) const { return (g < *this); }
	inline bool operator<=(const MulticastGroup &g) const { return !(g < *this); }
	inline bool operator>=(const MulticastGroup &g) const { return !(*this < g); }

private:
	MAC _mac;
	uint32_t _adi;
};

} // namespace ZeroTier

#endif
