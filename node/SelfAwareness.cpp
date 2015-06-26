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
	RR(renv)
{
}

SelfAwareness::~SelfAwareness()
{
}

void SelfAwareness::iam(const Address &reporter,const InetAddress &reporterPhysicalAddress,const InetAddress &myPhysicalAddress,bool trusted,uint64_t now)
{
	const InetAddress::IpScope scope = myPhysicalAddress.ipScope();

	switch(scope) {
		case InetAddress::IP_SCOPE_NONE:
		case InetAddress::IP_SCOPE_LOOPBACK:
		case InetAddress::IP_SCOPE_MULTICAST:
			return;
		case InetAddress::IP_SCOPE_GLOBAL:
			if ((!trusted)||(scope != reporterPhysicalAddress.ipScope()))
				return;
			break;
		default:
			if (scope != reporterPhysicalAddress.ipScope())
				return;
			break;
	}

	Mutex::Lock _l(_phy_m);

	PhySurfaceEntry &entry = _phy[PhySurfaceKey(reporter,scope)];

	if ((now - entry.ts) >= ZT_SELFAWARENESS_ENTRY_TIMEOUT) {
		entry.mySurface = myPhysicalAddress;
		entry.ts = now;
		TRACE("learned physical address %s for scope %u as seen from %s(%s) (replaced <null>)",myPhysicalAddress.toString().c_str(),(unsigned int)scope,reporter.toString().c_str(),reporterPhysicalAddress.toString().c_str());
	} else if (entry.mySurface != myPhysicalAddress) {
		entry.mySurface = myPhysicalAddress;
		entry.ts = now;
		TRACE("learned physical address %s for scope %u as seen from %s(%s) (replaced %s, resetting all in scope)",myPhysicalAddress.toString().c_str(),(unsigned int)scope,reporter.toString().c_str(),reporterPhysicalAddress.toString().c_str(),entry.mySurface.toString().c_str());

		// Erase all entries (other than this one) for this scope to prevent thrashing
		// Note: we should probably not use 'entry' after this
		for(std::map< PhySurfaceKey,PhySurfaceEntry >::iterator p(_phy.begin());p!=_phy.end();) {
			if ((p->first.reporter != reporter)&&(p->first.scope == scope))
				_phy.erase(p++);
			else ++p;
		}

		_ResetWithinScope rset(RR,now,(InetAddress::IpScope)scope);
		RR->topology->eachPeer<_ResetWithinScope &>(rset);

		// For all peers for whom we forgot an address, send a packet indirectly if
		// they are still considered alive so that we will re-establish direct links.
		SharedPtr<Peer> sn(RR->topology->getBestRoot());
		if (sn) {
			Path *snp = sn->getBestPath(now);
			if (snp) {
				for(std::vector< SharedPtr<Peer> >::const_iterator p(rset.peersReset.begin());p!=rset.peersReset.end();++p) {
					if ((*p)->alive(now)) {
						TRACE("sending indirect NOP to %s via %s(%s) to re-establish link",(*p)->address().toString().c_str(),sn->address().toString().c_str(),snp->address().toString().c_str());
						Packet outp((*p)->address(),RR->identity.address(),Packet::VERB_NOP);
						outp.armor((*p)->key(),true);
						snp->send(RR,outp.data(),outp.size(),now);
					}
				}
			}
		}
	} else {
		entry.ts = now;
	}
}

void SelfAwareness::clean(uint64_t now)
{
	Mutex::Lock _l(_phy_m);
	for(std::map< PhySurfaceKey,PhySurfaceEntry >::iterator p(_phy.begin());p!=_phy.end();) {
		if ((now - p->second.ts) >= ZT_SELFAWARENESS_ENTRY_TIMEOUT)
			_phy.erase(p++);
		else ++p;
	}
}

} // namespace ZeroTier
