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
	void iam(void *tPtr,const Address &reporter,const int64_t receivedOnLocalSocket,const InetAddress &reporterPhysicalAddress,const InetAddress &myPhysicalAddress,bool trusted,int64_t now);

	/**
	 * Clean up database periodically
	 *
	 * @param now Current time
	 */
	void clean(int64_t now);

#if 0
	/**
	 * If we appear to be behind a symmetric NAT, get predictions for possible external endpoints
	 *
	 * @return Symmetric NAT predictions or empty vector if none
	 */
	std::vector<InetAddress> getSymmetricNatPredictions();
#endif

private:
	struct PhySurfaceKey
	{
		Address reporter;
		int64_t receivedOnLocalSocket;
		InetAddress reporterPhysicalAddress;
		InetAddress::IpScope scope;

		PhySurfaceKey() : reporter(),scope(InetAddress::IP_SCOPE_NONE) {}
		PhySurfaceKey(const Address &r,const int64_t rol,const InetAddress &ra,InetAddress::IpScope s) : reporter(r),receivedOnLocalSocket(rol),reporterPhysicalAddress(ra),scope(s) {}

		inline unsigned long hashCode() const { return ((unsigned long)reporter.toInt() + (unsigned long)scope); }
		inline bool operator==(const PhySurfaceKey &k) const { return ((reporter == k.reporter)&&(receivedOnLocalSocket == k.receivedOnLocalSocket)&&(reporterPhysicalAddress == k.reporterPhysicalAddress)&&(scope == k.scope)); }
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
