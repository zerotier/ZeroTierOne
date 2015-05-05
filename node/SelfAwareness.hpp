/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#ifndef ZT_SELFAWARENESS_HPP
#define ZT_SELFAWARENESS_HPP

#include <map>

#include "InetAddress.hpp"
#include "Address.hpp"
#include "Mutex.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Tracks changes to this peer's real world addresses
 */
class SelfAwareness
{
public:
	SelfAwareness(const RuntimeEnvironment *renv);
	~SelfAwareness();

	/**
	 * Called when a trusted remote peer informs us of our external network address
	 *
	 * @param reporter ZeroTier address of reporting peer
	 * @param reporterPhysicalAddress Physical address that reporting peer seems to have
	 * @param myPhysicalAddress Physical address that peer says we have
	 * @param trusted True if this peer is trusted as an authority to inform us of external address changes
	 * @param now Current time
	 */
	void iam(const Address &reporter,const InetAddress &reporterPhysicalAddress,const InetAddress &myPhysicalAddress,bool trusted,uint64_t now);

	/**
	 * Clean up database periodically
	 *
	 * @param now Current time
	 */
	void clean(uint64_t now);

private:
	struct PhySurfaceKey
	{
		Address reporter;
		InetAddress::IpScope scope;

		PhySurfaceKey() : reporter(),scope(InetAddress::IP_SCOPE_NONE) {}
		PhySurfaceKey(const Address &r,InetAddress::IpScope s) : reporter(r),scope(s) {}
		inline bool operator<(const PhySurfaceKey &k) const throw() { return ((reporter < k.reporter) ? true : ((reporter == k.reporter) ? ((int)scope < (int)k.scope) : false)); }
		inline bool operator==(const PhySurfaceKey &k) const throw() { return ((reporter == k.reporter)&&(scope == k.scope)); }
	};
	struct PhySurfaceEntry
	{
		InetAddress mySurface;
		uint64_t ts;

		PhySurfaceEntry() : mySurface(),ts(0) {}
		PhySurfaceEntry(const InetAddress &a,const uint64_t t) : mySurface(a),ts(t) {}
	};

	const RuntimeEnvironment *RR;

	std::map< PhySurfaceKey,PhySurfaceEntry > _phy;
	Mutex _phy_m;
};

} // namespace ZeroTier

#endif
