/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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

#include "Defaults.hpp"
#include "Constants.hpp"

namespace ZeroTier {

const Defaults ZT_DEFAULTS;

static inline std::map< Identity,std::vector<InetAddress> > _mkSupernodeMap()
	throw(std::runtime_error)
{
	std::map< Identity,std::vector<InetAddress> > sn;
	Identity id;
	std::vector<InetAddress> addrs;

	// Nothing special about a supernode... except that they are
	// designated as such.

	// cthulhu.zerotier.com - New York, New York, USA
	addrs.clear();
	if (!id.fromString("271ee006a0:1:AgGXs3I+9CWrEmGMxc50x3E+trwtaa2ZMXDU6ezz92fFJXzlhRKGUY/uAToHDdH9XiLxtcA+kUQAZdC4Dy2xtqXxjw==:QgH5Nlx4oWEGVrwhNocqem+3VNd4qzt7RLrmuvqZvKPRS9R70LJYJQLlKZj0ri55Pzg+Mlwy4a4nAgfnRAWA+TW6R0EjSmq72MG585XGNfWBVk3LxMvxlNWErnVNFr2BQS9yzVp4pRjPLdCW4RB3dwEHBUgJ78rwMxQ6IghVCl8CjkDapg=="))
		throw std::runtime_error("invalid identity in Defaults");
	addrs.push_back(InetAddress("198.199.73.93",ZT_DEFAULT_UDP_PORT));
	sn[id] = addrs;

	// nyarlathotep.zerotier.com - San Francisco, California, USA
	addrs.clear();
	if (!id.fromString("fa9be4008b:1:AwCHXEi/PJuhtOPUZxnBSMiuGvj6XeRMWu9R9aLR3JD1qluADLQzUPSP2+81Dqvgi2wkQ2cqEpOlDPeUCvtlZwdXEA==:QgH4usG/wzsoUCtO2LL3qkwugtoXEz1PUJbmUzY8vbwzc5bckmVPjMqb4q2CF71+QVPV1K6shIV2EKkBMRSS/D/44EGEwC6tjFGZqmmogaC0P1uQeukTAF4qta46YgC4YQx54/Vd/Yfl8n1Bwmgm0gBB4W1ZQir3p+wp37MGlEN0rlXxqA=="))
		throw std::runtime_error("invalid identity in Defaults");
	addrs.push_back(InetAddress("198.199.97.220",ZT_DEFAULT_UDP_PORT));
	sn[id] = addrs;

	// shub-niggurath.zerotier.com - Amsterdam, Netherlands
	addrs.clear();
	if (!id.fromString("48099ecd05:1:AwHO7o1FdDj1nEArfchTDa6EG7Eh2GLdiH86BhcoNv0BHJN4tmrf0Y7/2SZiQFpTTwJf93iph84Dci5+k52u/qkHTQ==:QgGbir8CNxBFFPPj8Eo3Bnp2UmbnZxu/pOq3Ke0WaLBBhHzVuwM+88g7CaDxbZ0AY2VkFc9hmE3VG+xi7g0H86yfVUIBHZnb7N+DCtf8/mphZIHNgmasakRi4hU11kGyLi1nTVTnrmCfAb7w+8SCp64Q5RNvBC/Pvz7pxSwSdjIHkVqRaeo="))
		throw std::runtime_error("invalid identity in Defaults");
	addrs.push_back(InetAddress("198.211.127.172",ZT_DEFAULT_UDP_PORT));
	sn[id] = addrs;

	return sn;
}

Defaults::Defaults()
	throw(std::runtime_error) :
	supernodes(_mkSupernodeMap())
{
}

} // namespace ZeroTier
