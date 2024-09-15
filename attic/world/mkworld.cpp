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

/*
 * This utility makes the World from the configuration specified below.
 * It probably won't be much use to anyone outside ZeroTier, Inc. except
 * for testing and experimentation purposes.
 *
 * If you want to make your own World you must edit this file.
 *
 * When run, it expects two files in the current directory:
 *
 * previous.c25519 - key pair to sign this world (key from previous world)
 * current.c25519 - key pair whose public key should be embedded in this world
 *
 * If these files do not exist, they are both created with the same key pair
 * and a self-signed initial World is born.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <string>
#include <vector>
#include <algorithm>

#include <node/Constants.hpp>
#include <node/World.hpp>
#include <node/ECC.hpp>
#include <node/Identity.hpp>
#include <node/InetAddress.hpp>
#include <osdep/OSUtils.hpp>

using namespace ZeroTier;

int main(int argc,char **argv)
{
	std::string previous,current;
	if ((!OSUtils::readFile("previous.c25519",previous))||(!OSUtils::readFile("current.c25519",current))) {
		ECC::Pair np(ECC::generate());
		previous = std::string();
		previous.append((const char *)np.pub.data,ZT_C25519_PUBLIC_KEY_LEN);
		previous.append((const char *)np.priv.data,ZT_C25519_PRIVATE_KEY_LEN);
		current = previous;
		OSUtils::writeFile("previous.c25519",previous);
		OSUtils::writeFile("current.c25519",current);
		fprintf(stderr,"INFO: created initial world keys: previous.c25519 and current.c25519 (both initially the same)" ZT_EOL_S);
	}

	if ((previous.length() != (ZT_C25519_PUBLIC_KEY_LEN + ZT_C25519_PRIVATE_KEY_LEN))||(current.length() != (ZT_C25519_PUBLIC_KEY_LEN + ZT_C25519_PRIVATE_KEY_LEN))) {
		fprintf(stderr,"FATAL: previous.c25519 or current.c25519 empty or invalid" ZT_EOL_S);
		return 1;
	}
	ECC::Pair previousKP;
	memcpy(previousKP.pub.data,previous.data(),ZT_C25519_PUBLIC_KEY_LEN);
	memcpy(previousKP.priv.data,previous.data() + ZT_C25519_PUBLIC_KEY_LEN,ZT_C25519_PRIVATE_KEY_LEN);
	ECC::Pair currentKP;
	memcpy(currentKP.pub.data,current.data(),ZT_C25519_PUBLIC_KEY_LEN);
	memcpy(currentKP.priv.data,current.data() + ZT_C25519_PUBLIC_KEY_LEN,ZT_C25519_PRIVATE_KEY_LEN);

	// =========================================================================
	// EDIT BELOW HERE

	std::vector<World::Root> roots;

	const uint64_t id = ZT_WORLD_ID_EARTH;
	const uint64_t ts = 1567191349589ULL; // August 30th, 2019

	// Los Angeles
	roots.push_back(World::Root());
	roots.back().identity = Identity("3a46f1bf30:0:76e66fab33e28549a62ee2064d1843273c2c300ba45c3f20bef02dbad225723bb59a9bb4b13535730961aeecf5a163ace477cceb0727025b99ac14a5166a09a3");
	roots.back().stableEndpoints.push_back(InetAddress("185.180.13.82/9993"));
	roots.back().stableEndpoints.push_back(InetAddress("2a02:6ea0:c815::/9993"));

	// Miami
	roots.push_back(World::Root());
	roots.back().identity = Identity("de8950a8b2:0:1b3ada8251b91b6b6fa6535b8c7e2460918f4f729abdec97d3c7f3796868fb02f0de0b0ee554b2d59fc3524743eebfcf5315e790ed6d92db5bd10c28c09b40ef");
	roots.back().stableEndpoints.push_back(InetAddress("207.246.73.245/443"));
	roots.back().stableEndpoints.push_back(InetAddress("2001:19f0:9002:5cb:ec4:7aff:fe8f:69d9/443"));

	// Tokyo
	roots.push_back(World::Root());
	roots.back().identity = Identity("34e0a5e174:0:93efb50934788f856d5cfb9ca5be88e85b40965586b75befac900df77352c145a1ba7007569d37c77bfe52c0999f3bdc67a47a4a6000b720a883ce47aa2fb7f8");
	roots.back().stableEndpoints.push_back(InetAddress("147.75.92.2/443"));
	roots.back().stableEndpoints.push_back(InetAddress("2604:1380:3000:7100::1/443"));

	// Amsterdam
	roots.push_back(World::Root());
	roots.back().identity = Identity("992fcf1db7:0:206ed59350b31916f749a1f85dffb3a8787dcbf83b8c6e9448d4e3ea0e3369301be716c3609344a9d1533850fb4460c50af43322bcfc8e13d3301a1f1003ceb6");
	roots.back().stableEndpoints.push_back(InetAddress("195.181.173.159/443"));
	roots.back().stableEndpoints.push_back(InetAddress("2a02:6ea0:c024::/443"));

	// Alice
	//roots.push_back(World::Root());
	//roots.back().identity = Identity("9d219039f3:0:01f0922a98e3b34ebcbff333269dc265d7a020aab69d72be4d4acc9c8c9294785771256cd1d942a90d1bd1d2dca3ea84ef7d85afe6611fb43ff0b74126d90a6e");
	//roots.back().stableEndpoints.push_back(InetAddress("188.166.94.177/9993")); // Amsterdam
	//roots.back().stableEndpoints.push_back(InetAddress("2a03:b0c0:2:d0::7d:1/9993")); // Amsterdam
	//roots.back().stableEndpoints.push_back(InetAddress("154.66.197.33/9993")); // Johannesburg
	//roots.back().stableEndpoints.push_back(InetAddress("2c0f:f850:154:197::33/9993")); // Johannesburg
	//roots.back().stableEndpoints.push_back(InetAddress("159.203.97.171/9993")); // New York
	//roots.back().stableEndpoints.push_back(InetAddress("2604:a880:800:a1::54:6001/9993")); // New York
	//roots.back().stableEndpoints.push_back(InetAddress("131.255.6.16/9993")); // Buenos Aires
	//roots.back().stableEndpoints.push_back(InetAddress("2803:eb80:0:e::2/9993")); // Buenos Aires
	//roots.back().stableEndpoints.push_back(InetAddress("107.170.197.14/9993")); // San Francisco
	//roots.back().stableEndpoints.push_back(InetAddress("2604:a880:1:20::200:e001/9993")); // San Francisco
	//roots.back().stableEndpoints.push_back(InetAddress("128.199.197.217/9993")); // Singapore
	//roots.back().stableEndpoints.push_back(InetAddress("2400:6180:0:d0::b7:4001/9993")); // Singapore

	// Bob
	//roots.push_back(World::Root());
	//roots.back().identity = Identity("8841408a2e:0:bb1d31f2c323e264e9e64172c1a74f77899555ed10751cd56e86405cde118d02dffe555d462ccf6a85b5631c12350c8d5dc409ba10b9025d0f445cf449d92b1c");
	//roots.back().stableEndpoints.push_back(InetAddress("45.32.198.130/9993")); // Dallas
	//roots.back().stableEndpoints.push_back(InetAddress("2001:19f0:6400:81c3:5400:00ff:fe18:1d61/9993")); // Dallas
	//roots.back().stableEndpoints.push_back(InetAddress("46.101.160.249/9993")); // Frankfurt
	//roots.back().stableEndpoints.push_back(InetAddress("2a03:b0c0:3:d0::6a:3001/9993")); // Frankfurt
	//roots.back().stableEndpoints.push_back(InetAddress("107.191.46.210/9993")); // Paris
	//roots.back().stableEndpoints.push_back(InetAddress("2001:19f0:6800:83a4::64/9993")); // Paris
	//roots.back().stableEndpoints.push_back(InetAddress("45.32.246.179/9993")); // Sydney
	//roots.back().stableEndpoints.push_back(InetAddress("2001:19f0:5800:8bf8:5400:ff:fe15:b39a/9993")); // Sydney
	//roots.back().stableEndpoints.push_back(InetAddress("45.32.248.87/9993")); // Tokyo
	//roots.back().stableEndpoints.push_back(InetAddress("2001:19f0:7000:9bc9:5400:00ff:fe15:c4f5/9993")); // Tokyo
	//roots.back().stableEndpoints.push_back(InetAddress("159.203.2.154/9993")); // Toronto
	//roots.back().stableEndpoints.push_back(InetAddress("2604:a880:cad:d0::26:7001/9993")); // Toronto

	// END WORLD DEFINITION
	// =========================================================================

	fprintf(stderr,"INFO: generating and signing id==%llu ts==%llu" ZT_EOL_S,(unsigned long long)id,(unsigned long long)ts);

	World nw = World::make(World::TYPE_PLANET,id,ts,currentKP.pub,roots,previousKP);

	Buffer<ZT_WORLD_MAX_SERIALIZED_LENGTH> outtmp;
	nw.serialize(outtmp,false);
	World testw;
	testw.deserialize(outtmp,0);
	if (testw != nw) {
		fprintf(stderr,"FATAL: serialization test failed!" ZT_EOL_S);
		return 1;
	}

	OSUtils::writeFile("world.bin",std::string((const char *)outtmp.data(),outtmp.size()));
	fprintf(stderr,"INFO: world.bin written with %u bytes of binary world data." ZT_EOL_S,outtmp.size());

	fprintf(stdout,ZT_EOL_S);
	fprintf(stdout,"#define ZT_DEFAULT_WORLD_LENGTH %u" ZT_EOL_S,outtmp.size());
	fprintf(stdout,"static const unsigned char ZT_DEFAULT_WORLD[ZT_DEFAULT_WORLD_LENGTH] = {");
	for(unsigned int i=0;i<outtmp.size();++i) {
		const unsigned char *d = (const unsigned char *)outtmp.data();
		if (i > 0)
			fprintf(stdout,",");
		fprintf(stdout,"0x%.2x",(unsigned int)d[i]);
	}
	fprintf(stdout,"};" ZT_EOL_S);

	return 0;
}
