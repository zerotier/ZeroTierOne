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

#include <cstdlib>
#include <cstring>

#include <set>

#include "Constants.hpp"
#include "SelfAwareness.hpp"
#include "RuntimeEnvironment.hpp"
#include "Topology.hpp"
#include "Peer.hpp"
#include "Trace.hpp"

// Entry timeout -- make it fairly long since this is just to prevent stale buildup
#define ZT_SELFAWARENESS_ENTRY_TIMEOUT 300000

namespace ZeroTier {

class _ResetWithinScope
{
public:
	ZT_ALWAYS_INLINE _ResetWithinScope(void *tPtr,int64_t now,int inetAddressFamily,InetAddress::IpScope scope) :
		_now(now),
		_tPtr(tPtr),
		_family(inetAddressFamily),
		_scope(scope) {}

	ZT_ALWAYS_INLINE void operator()(const SharedPtr<Peer> &p) { p->resetWithinScope(_tPtr,_scope,_family,_now); }

private:
	int64_t _now;
	void *_tPtr;
	int _family;
	InetAddress::IpScope _scope;
};

SelfAwareness::SelfAwareness(const RuntimeEnvironment *renv) :
	RR(renv),
	_phy(256)
{
}

void SelfAwareness::iam(void *tPtr,const Identity &reporter,const int64_t receivedOnLocalSocket,const InetAddress &reporterPhysicalAddress,const InetAddress &myPhysicalAddress,bool trusted,int64_t now)
{
	const InetAddress::IpScope scope = myPhysicalAddress.ipScope();

	if ((scope != reporterPhysicalAddress.ipScope())||(scope == InetAddress::IP_SCOPE_NONE)||(scope == InetAddress::IP_SCOPE_LOOPBACK)||(scope == InetAddress::IP_SCOPE_MULTICAST))
		return;

	Mutex::Lock l(_phy_l);
	PhySurfaceEntry &entry = _phy[PhySurfaceKey(reporter.address(),receivedOnLocalSocket,reporterPhysicalAddress,scope)];

	if ( (trusted) && ((now - entry.ts) < ZT_SELFAWARENESS_ENTRY_TIMEOUT) && (!entry.mySurface.ipsEqual(myPhysicalAddress)) ) {
		// Changes to external surface reported by trusted peers causes path reset in this scope
		entry.mySurface = myPhysicalAddress;
		entry.ts = now;
		entry.trusted = trusted;

		// Erase all entries in this scope that were not reported from this remote address to prevent 'thrashing'
		// due to multiple reports of endpoint change.
		// Don't use 'entry' after this since hash table gets modified.
		{
			Hashtable< PhySurfaceKey,PhySurfaceEntry >::Iterator i(_phy);
			PhySurfaceKey *k = nullptr;
			PhySurfaceEntry *e = nullptr;
			while (i.next(k,e)) {
				if ((k->scope == scope)&&(k->reporterPhysicalAddress != reporterPhysicalAddress))
					_phy.erase(*k);
			}
		}

		// Reset all paths within this scope and address family
		_ResetWithinScope rset(tPtr,now,myPhysicalAddress.ss_family,(InetAddress::IpScope)scope);
		RR->topology->eachPeer<_ResetWithinScope &>(rset);

		RR->t->resettingPathsInScope(tPtr,0x9afff100,reporter,reporterPhysicalAddress,entry.mySurface,myPhysicalAddress,scope);
	} else {
		// Otherwise just update DB to use to determine external surface info
		entry.mySurface = myPhysicalAddress;
		entry.ts = now;
		entry.trusted = trusted;
	}
}

void SelfAwareness::clean(int64_t now)
{
	Mutex::Lock l(_phy_l);
	Hashtable< PhySurfaceKey,PhySurfaceEntry >::Iterator i(_phy);
	PhySurfaceKey *k = nullptr;
	PhySurfaceEntry *e = nullptr;
	while (i.next(k,e)) {
		if ((now - e->ts) >= ZT_SELFAWARENESS_ENTRY_TIMEOUT)
			_phy.erase(*k);
	}
}

std::multimap<unsigned long,InetAddress> SelfAwareness::externalAddresses(const int64_t now) const
{
	std::multimap<unsigned long,InetAddress> r;
	Hashtable<InetAddress,unsigned long> counts(16);

	{
		Mutex::Lock l(_phy_l);
		Hashtable<PhySurfaceKey,PhySurfaceEntry>::Iterator i(const_cast<SelfAwareness *>(this)->_phy);
		PhySurfaceKey *k = nullptr;
		PhySurfaceEntry *e = nullptr;
		while (i.next(k,e)) {
			if ((now - e->ts) < ZT_SELFAWARENESS_ENTRY_TIMEOUT)
				++counts[e->mySurface];
		}
	}

	Hashtable<InetAddress,unsigned long>::Iterator i(counts);
	InetAddress *k = nullptr;
	unsigned long *c = nullptr;
	while (i.next(k,c))
		r.insert(std::pair<unsigned long,InetAddress>(*c,*k));

	return r;
}

} // namespace ZeroTier
