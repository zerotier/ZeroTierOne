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
