/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
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
 */

#ifndef ZT_SELFAWARENESS_HPP
#define ZT_SELFAWARENESS_HPP

#include "Constants.hpp"
#include "InetAddress.hpp"
#include "Hashtable.hpp"
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

	/**
	 * Called when a trusted remote peer informs us of our external network address
	 *
	 * @param reporter ZeroTier address of reporting peer
	 * @param receivedOnLocalAddress Local address on which report was received
	 * @param reporterPhysicalAddress Physical address that reporting peer seems to have
	 * @param myPhysicalAddress Physical address that peer says we have
	 * @param trusted True if this peer is trusted as an authority to inform us of external address changes
	 * @param now Current time
	 */
	void iam(void *tPtr,const Address &reporter,const InetAddress &receivedOnLocalAddress,const InetAddress &reporterPhysicalAddress,const InetAddress &myPhysicalAddress,bool trusted,uint64_t now);

	/**
	 * Clean up database periodically
	 *
	 * @param now Current time
	 */
	void clean(uint64_t now);

	/**
	 * If we appear to be behind a symmetric NAT, get predictions for possible external endpoints
	 *
	 * @return Symmetric NAT predictions or empty vector if none
	 */
	std::vector<InetAddress> getSymmetricNatPredictions();

private:
	struct PhySurfaceKey
	{
		Address reporter;
		InetAddress receivedOnLocalAddress;
		InetAddress reporterPhysicalAddress;
		InetAddress::IpScope scope;

		PhySurfaceKey() : reporter(),scope(InetAddress::IP_SCOPE_NONE) {}
		PhySurfaceKey(const Address &r,const InetAddress &rol,const InetAddress &ra,InetAddress::IpScope s) : reporter(r),receivedOnLocalAddress(rol),reporterPhysicalAddress(ra),scope(s) {}

		inline unsigned long hashCode() const throw() { return ((unsigned long)reporter.toInt() + (unsigned long)scope); }
		inline bool operator==(const PhySurfaceKey &k) const throw() { return ((reporter == k.reporter)&&(receivedOnLocalAddress == k.receivedOnLocalAddress)&&(reporterPhysicalAddress == k.reporterPhysicalAddress)&&(scope == k.scope)); }
	};
	struct PhySurfaceEntry
	{
		InetAddress mySurface;
		uint64_t ts;
		bool trusted;

		PhySurfaceEntry() : mySurface(),ts(0),trusted(false) {}
		PhySurfaceEntry(const InetAddress &a,const uint64_t t) : mySurface(a),ts(t),trusted(false) {}
	};

	const RuntimeEnvironment *RR;

	Hashtable< PhySurfaceKey,PhySurfaceEntry > _phy;
	Mutex _phy_m;
};

} // namespace ZeroTier

#endif
