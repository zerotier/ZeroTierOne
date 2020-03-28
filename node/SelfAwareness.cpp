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

#include "Constants.hpp"
#include "SelfAwareness.hpp"
#include "RuntimeEnvironment.hpp"
#include "Topology.hpp"
#include "Peer.hpp"
#include "Trace.hpp"

#include <cstdlib>
#include <cstring>
#include <set>

// Entry timeout -- make it fairly long since this is just to prevent stale buildup
#define ZT_SELFAWARENESS_ENTRY_TIMEOUT 300000

namespace ZeroTier {

class _ResetWithinScope
{
public:
	ZT_INLINE _ResetWithinScope(void *tPtr,int64_t now,int inetAddressFamily,InetAddress::IpScope scope) :
		_now(now),
		_tPtr(tPtr),
		_family(inetAddressFamily),
		_scope(scope) {}

	ZT_INLINE void operator()(const SharedPtr<Peer> &p) { p->resetWithinScope(_tPtr,_scope,_family,_now); }

private:
	int64_t _now;
	void *_tPtr;
	int _family;
	InetAddress::IpScope _scope;
};

SelfAwareness::SelfAwareness(const RuntimeEnvironment *renv) :
	RR(renv)
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
		for(FlatMap<PhySurfaceKey,PhySurfaceEntry>::iterator i(_phy.begin());i!=_phy.end();) {
			if ((i->first.scope == scope)&&(i->first.reporterPhysicalAddress != reporterPhysicalAddress))
				_phy.erase(i++);
			else ++i;
		}

		// Reset all paths within this scope and address family
		_ResetWithinScope rset(tPtr,now,myPhysicalAddress.family(),(InetAddress::IpScope)scope);
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
	for(FlatMap<PhySurfaceKey,PhySurfaceEntry>::iterator i(_phy.begin());i!=_phy.end();) {
		if ((now - i->second.ts) >= ZT_SELFAWARENESS_ENTRY_TIMEOUT)
			_phy.erase(i++);
		else ++i;
	}
}

std::multimap<unsigned long,InetAddress> SelfAwareness::externalAddresses(const int64_t now) const
{
	std::multimap<unsigned long,InetAddress> r;
	FlatMap<InetAddress,unsigned long,256> counts;

	{
		Mutex::Lock l(_phy_l);
		for(FlatMap<PhySurfaceKey,PhySurfaceEntry>::const_iterator i(_phy.begin());i!=_phy.end();++i) {
			if ((now - i->second.ts) < ZT_SELFAWARENESS_ENTRY_TIMEOUT)
				++counts[i->second.mySurface];
		}
	}

	for(FlatMap<InetAddress,unsigned long,256>::iterator i(counts.begin());i!=counts.end();++i)
		r.insert(std::pair<unsigned long,InetAddress>(i->second,i->first));

	return r;
}

} // namespace ZeroTier
