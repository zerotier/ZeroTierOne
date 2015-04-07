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

namespace ZeroTier {

class _ResetWithinScope
{
public:
	_ResetWithinScope(const RuntimeEnvironment *renv,uint64_t now,InetAddress::IpScope scope) :
		RR(renv),
		_now(now),
		_scope(scope) {}
	inline void operator()(Topology &t,const SharedPtr<Peer> &p) { p->resetWithinScope(RR,_scope,_now); }
private:
	const RuntimeEnvironment *RR;
	uint64_t _now;
	InetAddress::IpScope _scope;
};

SelfAwareness::SelfAwareness(const RuntimeEnvironment *renv) :
	RR(renv)
{
	memset(_lastPhysicalAddress,0,sizeof(_lastPhysicalAddress));
}

SelfAwareness::~SelfAwareness()
{
}

void SelfAwareness::iam(const InetAddress &reporterPhysicalAddress,const InetAddress &myPhysicalAddress,bool trusted)
{
	// This code depends on the numeric values assigned to scopes in InetAddress.hpp
	const unsigned int scope = (unsigned int)myPhysicalAddress.ipScope();
	if ((scope > 0)&&(scope < (unsigned int)InetAddress::IP_SCOPE_LOOPBACK)) {
		/* For now only trusted peers are permitted to inform us of changes to
		 * our global Internet IP or to changes of NATed IPs. We'll let peers on
		 * private, shared, or link-local networks inform us of changes as long
		 * as they too are at the same scope. This discrimination avoids a DoS
		 * attack in which an attacker could force us to reset our connections. */
		if ( (!trusted) && ((scope == (unsigned int)InetAddress::IP_SCOPE_GLOBAL)||(scope != (unsigned int)reporterPhysicalAddress.ipScope())) )
			return;
		else {
			Mutex::Lock _l(_lock);
			InetAddress &lastPhy = _lastPhysicalAddress[scope - 1];
			if (!lastPhy) {
				lastPhy = myPhysicalAddress;
			} else if (lastPhy != myPhysicalAddress) {
				lastPhy = myPhysicalAddress;
				_ResetWithinScope rset(RR,RR->node->now(),(InetAddress::IpScope)scope);
				RR->topology->eachPeer<_ResetWithinScope &>(rset);
			}
		}
	}
}

} // namespace ZeroTier
