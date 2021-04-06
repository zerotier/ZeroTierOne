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
#include "SelfAwareness.hpp"
#include "RuntimeEnvironment.hpp"
#include "Topology.hpp"
#include "Peer.hpp"
#include "Trace.hpp"
#include "Containers.hpp"

// Entry timeout -- make it fairly long since this is just to prevent stale buildup
#define ZT_SELFAWARENESS_ENTRY_TIMEOUT 300000

namespace ZeroTier {

SelfAwareness::SelfAwareness(const RuntimeEnvironment *renv) :
	RR(renv)
{
}

void SelfAwareness::iam(CallContext &cc, const Identity &reporter, const int64_t receivedOnLocalSocket, const InetAddress &reporterPhysicalAddress, const InetAddress &myPhysicalAddress, bool trusted)
{
	const InetAddress::IpScope scope = myPhysicalAddress.ipScope();

	if ((scope != reporterPhysicalAddress.ipScope()) || (scope == ZT_IP_SCOPE_NONE) || (scope == ZT_IP_SCOPE_LOOPBACK) || (scope == ZT_IP_SCOPE_MULTICAST))
		return;

	Mutex::Lock l(m_phy_l);
	p_PhySurfaceEntry &entry = m_phy[p_PhySurfaceKey(reporter.address(), receivedOnLocalSocket, reporterPhysicalAddress, scope)];

	if ((trusted) && ((cc.ticks - entry.timestampTicks) < ZT_SELFAWARENESS_ENTRY_TIMEOUT) && (!entry.mySurface.ipsEqual(myPhysicalAddress))) {
		// Changes to external surface reported by trusted peers causes path reset in this scope
		entry.mySurface = myPhysicalAddress;
		entry.timestampTicks = cc.ticks;
		entry.trusted = trusted;

		// Erase all entries in this scope that were not reported from this remote address to prevent 'thrashing'
		// due to multiple reports of endpoint change.
		// Don't use 'entry' after this since hash table gets modified.
		for (Map< p_PhySurfaceKey, p_PhySurfaceEntry >::iterator i(m_phy.begin()); i != m_phy.end();) {
			if ((i->first.scope == scope) && (i->first.reporterPhysicalAddress != reporterPhysicalAddress))
				m_phy.erase(i++);
			else ++i;
		}

		// Reset all paths within this scope and address family
		Vector< SharedPtr< Peer > > peers, rootPeers;
		RR->topology->allPeers(peers, rootPeers);
		for(Vector< SharedPtr< Peer > >::const_iterator p(peers.begin());p!=peers.end();++p)
			(*p)->resetWithinScope(cc, (InetAddress::IpScope)scope, myPhysicalAddress.as.sa.sa_family);

		RR->t->resettingPathsInScope(cc, 0x9afff100, reporter, reporterPhysicalAddress, entry.mySurface, myPhysicalAddress, scope);
	} else {
		// Otherwise just update DB to use to determine external surface info
		entry.mySurface = myPhysicalAddress;
		entry.timestampTicks = cc.ticks;
		entry.trusted = trusted;
	}
}

void SelfAwareness::clean(CallContext &cc)
{
	Mutex::Lock l(m_phy_l);
	for (Map< p_PhySurfaceKey, p_PhySurfaceEntry >::iterator i(m_phy.begin()); i != m_phy.end();) {
		if ((cc.ticks - i->second.timestampTicks) >= ZT_SELFAWARENESS_ENTRY_TIMEOUT)
			m_phy.erase(i++);
		else ++i;
	}
}

MultiMap< unsigned int, InetAddress > SelfAwareness::externalAddresses(CallContext &cc) const
{
	MultiMap< unsigned int, InetAddress > r;

	// Count endpoints reporting each IP/port combo
	Map< InetAddress, unsigned long > counts;
	{
		Mutex::Lock l(m_phy_l);
		for (Map< p_PhySurfaceKey, p_PhySurfaceEntry >::const_iterator i(m_phy.begin()); i != m_phy.end(); ++i) {
			if ((cc.ticks - i->second.timestampTicks) < ZT_SELFAWARENESS_ENTRY_TIMEOUT)
				++counts[i->second.mySurface];
		}
	}

	// Invert to create a map from count to address
	for (Map< InetAddress, unsigned long >::iterator i(counts.begin()); i != counts.end(); ++i)
		r.insert(std::pair< unsigned long, InetAddress >(i->second, i->first));

	return r;
}

} // namespace ZeroTier
