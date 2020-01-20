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

#include "Path.hpp"
#include "RuntimeEnvironment.hpp"
#include "Node.hpp"

namespace ZeroTier {

bool Path::send(const RuntimeEnvironment *RR,void *tPtr,const void *data,unsigned int len,int64_t now)
{
	if (RR->node->putPacket(tPtr,_localSocket,_addr,data,len)) {
		_lastOut = now;
		return true;
	}
	return false;
}

bool Path::isAddressValidForPath(const InetAddress &a)
{
	if ((a.ss_family == AF_INET)||(a.ss_family == AF_INET6)) {
		switch(a.ipScope()) {
			/* Note: we don't do link-local at the moment. Unfortunately these
			 * cause several issues. The first is that they usually require a
			 * device qualifier, which we don't handle yet and can't portably
			 * push in PUSH_DIRECT_PATHS. The second is that some OSes assign
			 * these very ephemerally or otherwise strangely. So we'll use
			 * private, pseudo-private, shared (e.g. carrier grade NAT), or
			 * global IP addresses. */
			case InetAddress::IP_SCOPE_PRIVATE:
			case InetAddress::IP_SCOPE_PSEUDOPRIVATE:
			case InetAddress::IP_SCOPE_SHARED:
			case InetAddress::IP_SCOPE_GLOBAL:
				if (a.ss_family == AF_INET6) {
					// TEMPORARY HACK: for now, we are going to blacklist he.net IPv6
					// tunnels due to very spotty performance and low MTU issues over
					// these IPv6 tunnel links.
					const uint8_t *ipd = reinterpret_cast<const uint8_t *>(reinterpret_cast<const struct sockaddr_in6 *>(&a)->sin6_addr.s6_addr);
					if ((ipd[0] == 0x20)&&(ipd[1] == 0x01)&&(ipd[2] == 0x04)&&(ipd[3] == 0x70))
						return false;
				}
				return true;
			default:
				return false;
		}
	}
	return false;
}

} // namespace ZeroTier
