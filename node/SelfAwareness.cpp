/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <set>
#include <vector>

#include "Constants.hpp"
#include "SelfAwareness.hpp"
#include "RuntimeEnvironment.hpp"
#include "Node.hpp"
#include "Topology.hpp"
#include "Packet.hpp"
#include "Peer.hpp"
#include "Switch.hpp"
#include "Trace.hpp"

// Entry timeout -- make it fairly long since this is just to prevent stale buildup
#define ZT_SELFAWARENESS_ENTRY_TIMEOUT 600000

namespace ZeroTier {

class _ResetWithinScope
{
public:
	_ResetWithinScope(void *tPtr,int64_t now,int inetAddressFamily,InetAddress::IpScope scope) :
		_now(now),
		_tPtr(tPtr),
		_family(inetAddressFamily),
		_scope(scope) {}

	inline void operator()(Topology &t,const SharedPtr<Peer> &p) { p->resetWithinScope(_tPtr,_scope,_family,_now); }

private:
	uint64_t _now;
	void *_tPtr;
	int _family;
	InetAddress::IpScope _scope;
};

SelfAwareness::SelfAwareness(const RuntimeEnvironment *renv) :
	RR(renv),
	_phy(128)
{
}

void SelfAwareness::iam(void *tPtr,const Address &reporter,const int64_t receivedOnLocalSocket,const InetAddress &reporterPhysicalAddress,const InetAddress &myPhysicalAddress,bool trusted,int64_t now)
{
	const InetAddress::IpScope scope = myPhysicalAddress.ipScope();

	if ((scope != reporterPhysicalAddress.ipScope())||(scope == InetAddress::IP_SCOPE_NONE)||(scope == InetAddress::IP_SCOPE_LOOPBACK)||(scope == InetAddress::IP_SCOPE_MULTICAST))
		return;

	Mutex::Lock _l(_phy_m);
	PhySurfaceEntry &entry = _phy[PhySurfaceKey(reporter,receivedOnLocalSocket,reporterPhysicalAddress,scope)];

	if ( (trusted) && ((now - entry.ts) < ZT_SELFAWARENESS_ENTRY_TIMEOUT) && (!entry.mySurface.ipsEqual(myPhysicalAddress)) ) {
		// Changes to external surface reported by trusted peers causes path reset in this scope
		RR->t->resettingPathsInScope(tPtr,reporter,reporterPhysicalAddress,myPhysicalAddress,scope);

		entry.mySurface = myPhysicalAddress;
		entry.ts = now;
		entry.trusted = trusted;

		// Erase all entries in this scope that were not reported from this remote address to prevent 'thrashing'
		// due to multiple reports of endpoint change.
		// Don't use 'entry' after this since hash table gets modified.
		{
			Hashtable< PhySurfaceKey,PhySurfaceEntry >::Iterator i(_phy);
			PhySurfaceKey *k = (PhySurfaceKey *)0;
			PhySurfaceEntry *e = (PhySurfaceEntry *)0;
			while (i.next(k,e)) {
				if ((k->reporterPhysicalAddress != reporterPhysicalAddress)&&(k->scope == scope))
					_phy.erase(*k);
			}
		}

		// Reset all paths within this scope and address family
		_ResetWithinScope rset(tPtr,now,myPhysicalAddress.ss_family,(InetAddress::IpScope)scope);
		RR->topology->eachPeer<_ResetWithinScope &>(rset);
	} else {
		// Otherwise just update DB to use to determine external surface info
		entry.mySurface = myPhysicalAddress;
		entry.ts = now;
		entry.trusted = trusted;
	}
}

void SelfAwareness::clean(int64_t now)
{
	Mutex::Lock _l(_phy_m);
	Hashtable< PhySurfaceKey,PhySurfaceEntry >::Iterator i(_phy);
	PhySurfaceKey *k = (PhySurfaceKey *)0;
	PhySurfaceEntry *e = (PhySurfaceEntry *)0;
	while (i.next(k,e)) {
		if ((now - e->ts) >= ZT_SELFAWARENESS_ENTRY_TIMEOUT)
			_phy.erase(*k);
	}
}

} // namespace ZeroTier
