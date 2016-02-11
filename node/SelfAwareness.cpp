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

// Entry timeout -- make it fairly long since this is just to prevent stale buildup
#define ZT_SELFAWARENESS_ENTRY_TIMEOUT 3600000

namespace ZeroTier {

class _ResetWithinScope
{
public:
	_ResetWithinScope(uint64_t now,InetAddress::IpScope scope) :
		_now(now),
		_scope(scope) {}

	inline void operator()(Topology &t,const SharedPtr<Peer> &p)
	{
		if (p->resetWithinScope(_scope,_now))
			peersReset.push_back(p);
	}

	std::vector< SharedPtr<Peer> > peersReset;

private:
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

	if ((scope != reporterPhysicalAddress.ipScope())||(scope == InetAddress::IP_SCOPE_NONE)||(scope == InetAddress::IP_SCOPE_LOOPBACK)||(scope == InetAddress::IP_SCOPE_MULTICAST))
		return;

	Mutex::Lock _l(_phy_m);
	PhySurfaceEntry &entry = _phy[PhySurfaceKey(reporter,reporterPhysicalAddress,scope)];

	if ( (trusted) && ((now - entry.ts) < ZT_SELFAWARENESS_ENTRY_TIMEOUT) && (!entry.mySurface.ipsEqual(myPhysicalAddress)) ) {
		// Changes to external surface reported by trusted peers causes path reset in this scope
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
		_ResetWithinScope rset(now,(InetAddress::IpScope)scope);
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
		// Otherwise just update DB to use to determine external surface info
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

std::vector<InetAddress> SelfAwareness::getSymmetricNatPredictions()
{
	std::set<InetAddress> surfaces;

	// Ideas based on: https://tools.ietf.org/html/draft-takeda-symmetric-nat-traversal-00

	{
		Mutex::Lock _l(_phy_m);
		Hashtable< PhySurfaceKey,PhySurfaceEntry >::Iterator i(_phy);
		PhySurfaceKey *k = (PhySurfaceKey *)0;
		PhySurfaceEntry *e = (PhySurfaceEntry *)0;
		while (i.next(k,e)) {
			if ((e->mySurface.ss_family == AF_INET)&&(e->mySurface.ipScope() == InetAddress::IP_SCOPE_GLOBAL)) {
				surfaces.insert(e->mySurface);
			}
		}
	}

	if (surfaces.size() > 1) {
		// More than one global IPv4 surface means this is a symmetric NAT
		std::vector<InetAddress> r;
		for(std::set<InetAddress>::iterator i(surfaces.begin());i!=surfaces.end();++i) {
			InetAddress ipp(*i);
			unsigned int p = ipp.port();

			// Try 1+ surface ports
			if (p >= 0xffff)
				p = 1025;
			else ++p;
			ipp.setPort(p);
			if ((surfaces.count(ipp) == 0)&&(std::find(r.begin(),r.end(),ipp) == r.end()))
				r.push_back(ipp);

			// Try 2+ surface ports
			if (p >= 0xffff)
				p = 1025;
			else ++p;
			ipp.setPort(p);
			if ((surfaces.count(ipp) == 0)&&(std::find(r.begin(),r.end(),ipp) == r.end()))
				r.push_back(ipp);
		}
		return r;
	}

	return std::vector<InetAddress>();
}

} // namespace ZeroTier
