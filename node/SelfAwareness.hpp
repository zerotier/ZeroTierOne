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

#ifndef ZT_SELFAWARENESS_HPP
#define ZT_SELFAWARENESS_HPP

#include "Constants.hpp"
#include "InetAddress.hpp"
#include "FlatMap.hpp"
#include "Address.hpp"
#include "Mutex.hpp"

#include <map>

namespace ZeroTier {

class Identity;
class RuntimeEnvironment;

/**
 * SelfAwareness manages awareness of this peer's external address(es) and NAT situation.
 *
 * This code should not be capable of achieving sentience and triggering the Terminator wars.
 */
class SelfAwareness
{
public:
	explicit SelfAwareness(const RuntimeEnvironment *renv);

	/**
	 * Called when a remote peer informs us of our external network address
	 *
	 * @param reporter Identity of reporting peer
	 * @param receivedOnLocalAddress Local address on which report was received
	 * @param reporterPhysicalAddress Physical address that reporting peer seems to have
	 * @param myPhysicalAddress Physical address that peer says we have
	 * @param trusted True if this peer is trusted as an authority to inform us of external address changes
	 * @param now Current time
	 */
	void iam(void *tPtr,const Identity &reporter,int64_t receivedOnLocalSocket,const InetAddress &reporterPhysicalAddress,const InetAddress &myPhysicalAddress,bool trusted,int64_t now);

	/**
	 * Clean up database periodically
	 *
	 * @param now Current time
	 */
	void clean(int64_t now);

	/**
	 * Get external address consensus, which is the statistical "mode" of external addresses.
	 *
	 * @param now Current time
	 * @return Map of count to IP/port representing how many endpoints reported each address
	 */
	std::multimap<unsigned long,InetAddress> externalAddresses(int64_t now) const;

private:
	struct PhySurfaceKey
	{
		Address reporter;
		int64_t receivedOnLocalSocket;
		InetAddress reporterPhysicalAddress;
		InetAddress::IpScope scope;

		ZT_INLINE PhySurfaceKey() {}
		ZT_INLINE PhySurfaceKey(const Address &r,const int64_t rol,const InetAddress &ra,InetAddress::IpScope s) : reporter(r),receivedOnLocalSocket(rol),reporterPhysicalAddress(ra),scope(s) {}

		ZT_INLINE unsigned long hashCode() const { return ((unsigned long)reporter.toInt() + (unsigned long)receivedOnLocalSocket + (unsigned long)scope); }

		ZT_INLINE bool operator==(const PhySurfaceKey &k) const { return ((reporter == k.reporter) && (receivedOnLocalSocket == k.receivedOnLocalSocket) && (reporterPhysicalAddress == k.reporterPhysicalAddress) && (scope == k.scope)); }
		ZT_INLINE bool operator!=(const PhySurfaceKey &k) const { return (!(*this == k)); }
	};

	struct PhySurfaceEntry
	{
		InetAddress mySurface;
		uint64_t ts;
		bool trusted;

		ZT_INLINE PhySurfaceEntry() : mySurface(),ts(0),trusted(false) {}
		ZT_INLINE PhySurfaceEntry(const InetAddress &a,const uint64_t t) : mySurface(a),ts(t),trusted(false) {}
	};

	const RuntimeEnvironment *RR;
	FlatMap< PhySurfaceKey,PhySurfaceEntry > _phy;
	Mutex _phy_l;
};

} // namespace ZeroTier

#endif
