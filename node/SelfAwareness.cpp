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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Constants.hpp"
#include "SelfAwareness.hpp"
#include "RuntimeEnvironment.hpp"
#include "Node.hpp"
#include "Topology.hpp"
#include "Packet.hpp"
#include "Peer.hpp"
#include "Switch.hpp"

// Entry timeout -- make it fairly long since this is just to prevent stale buildup
#define ZT_SELFAWARENESS_ENTRY_TIMEOUT 3600000

namespace ZeroTier {

class _ResetWithinScope
{
public:
	_ResetWithinScope(const RuntimeEnvironment *renv,uint64_t now,InetAddress::IpScope scope) :
		RR(renv),
		_now(now),
		_scope(scope) {}

	inline void operator()(Topology &t,const SharedPtr<Peer> &p)
	{
		if (p->resetWithinScope(RR,_scope,_now))
			peersReset.push_back(p);
	}

	std::vector< SharedPtr<Peer> > peersReset;

private:
	const RuntimeEnvironment *RR;
	uint64_t _now;
	InetAddress::IpScope _scope;
};

SelfAwareness::SelfAwareness(const RuntimeEnvironment *renv) :
	RR(renv),
	_phy(32)
{
}

SelfAwareness::~SelfAwareness()
{
}

void SelfAwareness::iam(const Address &reporter,const InetAddress &reporterPhysicalAddress,const InetAddress &myPhysicalAddress,bool trusted,uint64_t now)
{
	const InetAddress::IpScope scope = myPhysicalAddress.ipScope();

	// This would be weird, e.g. a public IP talking to 10.0.0.1, so just ignore it.
	// If your network is this weird it's probably not reliable information.
	if (scope != reporterPhysicalAddress.ipScope())
		return;

	// Some scopes we ignore, and global scope IPs are only used for this
	// mechanism if they come from someone we trust (e.g. a root).
	switch(scope) {
		case InetAddress::IP_SCOPE_NONE:
		case InetAddress::IP_SCOPE_LOOPBACK:
		case InetAddress::IP_SCOPE_MULTICAST:
			return;
		case InetAddress::IP_SCOPE_GLOBAL:
			if (!trusted)
				return;
			break;
		default:
			break;
	}

	Mutex::Lock _l(_phy_m);
	PhySurfaceEntry &entry = _phy[PhySurfaceKey(reporter,reporterPhysicalAddress,scope)];

	if ( ((now - entry.ts) < ZT_SELFAWARENESS_ENTRY_TIMEOUT) && (!entry.mySurface.ipsEqual(myPhysicalAddress)) ) {
		entry.mySurface = myPhysicalAddress;
		entry.ts = now;
		TRACE("physical address %s for scope %u as seen from %s(%s) differs from %s, resetting paths in scope",myPhysicalAddress.toString().c_str(),(unsigned int)scope,reporter.toString().c_str(),reporterPhysicalAddress.toString().c_str(),entry.mySurface.toString().c_str());

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

		// Reset all paths within this scope
		_ResetWithinScope rset(RR,now,(InetAddress::IpScope)scope);
		RR->topology->eachPeer<_ResetWithinScope &>(rset);

		// Send a NOP to all peers for whom we forgot a path. This will cause direct
		// links to be re-established if possible, possibly using a root server or some
		// other relay.
		for(std::vector< SharedPtr<Peer> >::const_iterator p(rset.peersReset.begin());p!=rset.peersReset.end();++p) {
			if ((*p)->activelyTransferringFrames(now)) {
				Packet outp((*p)->address(),RR->identity.address(),Packet::VERB_NOP);
				RR->sw->send(outp,true,0);
			}
		}
	} else {
		entry.mySurface = myPhysicalAddress;
		entry.ts = now;
	}
}

void SelfAwareness::clean(uint64_t now)
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
