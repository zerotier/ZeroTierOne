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
#include <node/C25519.hpp>
#include <node/Identity.hpp>
#include <node/InetAddress.hpp>
#include <osdep/OSUtils.hpp>

using namespace ZeroTier;

class WorldMaker : public World
{
public:
	static inline World make(uint64_t id,uint64_t ts,const C25519::Public &sk,const std::vector<World::Root> &roots,const C25519::Pair &signWith)
	{
		WorldMaker w;
		w._id = id;
		w._ts = ts;
		w._updateSigningKey = sk;
		w._roots = roots;

		Buffer<ZT_WORLD_MAX_SERIALIZED_LENGTH> tmp;
		w.serialize(tmp,true);
		w._signature = C25519::sign(signWith,tmp.data(),tmp.size());

		return w;
	}
};

int main(int argc,char **argv)
{
	std::string previous,current;
	if ((!OSUtils::readFile("previous.c25519",previous))||(!OSUtils::readFile("current.c25519",current))) {
		C25519::Pair np(C25519::generate());
		previous = std::string();
		previous.append((const char *)np.pub.data,ZT_C25519_PUBLIC_KEY_LEN);
		previous.append((const char *)np.priv.data,ZT_C25519_PRIVATE_KEY_LEN);
		current = previous;
		OSUtils::writeFile("previous.c25519",previous);
		OSUtils::writeFile("current.c25519",current);
		fprintf(stderr,"INFO: created initial world keys: previous.c25519 and current.c25519 (both initially the same)"ZT_EOL_S);
	}

	if ((previous.length() != (ZT_C25519_PUBLIC_KEY_LEN + ZT_C25519_PRIVATE_KEY_LEN))||(current.length() != (ZT_C25519_PUBLIC_KEY_LEN + ZT_C25519_PRIVATE_KEY_LEN))) {
		fprintf(stderr,"FATAL: previous.c25519 or current.c25519 empty or invalid"ZT_EOL_S);
		return 1;
	}
	C25519::Pair previousKP;
	memcpy(previousKP.pub.data,previous.data(),ZT_C25519_PUBLIC_KEY_LEN);
	memcpy(previousKP.priv.data,previous.data() + ZT_C25519_PUBLIC_KEY_LEN,ZT_C25519_PRIVATE_KEY_LEN);
	C25519::Pair currentKP;
	memcpy(currentKP.pub.data,current.data(),ZT_C25519_PUBLIC_KEY_LEN);
	memcpy(currentKP.priv.data,current.data() + ZT_C25519_PUBLIC_KEY_LEN,ZT_C25519_PRIVATE_KEY_LEN);

	// =========================================================================
	// EDIT BELOW HERE

	std::vector<World::Root> roots;

	//
	// The initial version of the World uses the old root server infrastructure.
	// The new "Alice and Bob" infrastructure will replace this gradually, with
	// Paris probably being the first node to be taken over and clusterized.
	//
	// ZeroTier does actual World generation on an air-gapped machine by copying
	// this code over, building it there and running, then saving the results
	// onto a USB key.
	//

	const uint64_t id = ZT_WORLD_ID_EARTH;
	const uint64_t ts = 1447696577275ULL; // November 16th, 2015 ~9:56AM

	// old US-SFO
	roots.push_back(World::Root());
	roots.back().identity = Identity("7e19876aba:0:2a6e2b2318930f60eb097f70d0f4b028b2cd6d3d0c63c014b9039ff35390e41181f216fb2e6fa8d95c1ee9667156411905c3dccfea78d8c6dfafba688170b3fa");
	roots.back().stableEndpoints.push_back(InetAddress("198.199.97.220/9993"));

	// old EU-PARIS
	roots.push_back(World::Root());
	roots.back().identity = Identity("8841408a2e:0:bb1d31f2c323e264e9e64172c1a74f77899555ed10751cd56e86405cde118d02dffe555d462ccf6a85b5631c12350c8d5dc409ba10b9025d0f445cf449d92b1c");
	roots.back().stableEndpoints.push_back(InetAddress("107.191.46.210/9993"));

	// old US-NYC
	roots.push_back(World::Root());
	roots.back().identity = Identity("8acf059fe3:0:482f6ee5dfe902319b419de5bdc765209c0ecda38c4d6e4fcf0d33658398b4527dcd22f93112fb9befd02fd78bf7261b333fc105d192a623ca9e50fc60b374a5");
	roots.back().stableEndpoints.push_back(InetAddress("162.243.77.111/9993"));

	// old AP-SNG
	roots.push_back(World::Root());
	roots.back().identity = Identity("9d219039f3:0:01f0922a98e3b34ebcbff333269dc265d7a020aab69d72be4d4acc9c8c9294785771256cd1d942a90d1bd1d2dca3ea84ef7d85afe6611fb43ff0b74126d90a6e");
	roots.back().stableEndpoints.push_back(InetAddress("128.199.197.217/9993"));

	// END WORLD DEFINITION
	// =========================================================================

	fprintf(stderr,"INFO: generating and signing id==%llu ts==%llu"ZT_EOL_S,(unsigned long long)id,(unsigned long long)ts);

	World nw = WorldMaker::make(id,ts,currentKP.pub,roots,previousKP);

	Buffer<ZT_WORLD_MAX_SERIALIZED_LENGTH> outtmp;
	nw.serialize(outtmp,false);
	World testw;
	testw.deserialize(outtmp,0);
	if (testw != nw) {
		fprintf(stderr,"FATAL: serialization test failed!"ZT_EOL_S);
		return 1;
	}

	OSUtils::writeFile("world.bin",std::string((const char *)outtmp.data(),outtmp.size()));
	fprintf(stderr,"INFO: world.bin written with %u bytes of binary world data."ZT_EOL_S,outtmp.size());

	fprintf(stdout,ZT_EOL_S);
	fprintf(stdout,"#define ZT_DEFAULT_WORLD_LENGTH %u"ZT_EOL_S,outtmp.size());
	fprintf(stdout,"static const unsigned char ZT_DEFAULT_WORLD[ZT_DEFAULT_WORLD_LENGTH] = {");
	for(unsigned int i=0;i<outtmp.size();++i) {
		const unsigned char *d = (const unsigned char *)outtmp.data();
		if (i > 0)
			fprintf(stdout,",");
		fprintf(stdout,"0x%.2x",(unsigned int)d[i]);
	}
	fprintf(stdout,"};"ZT_EOL_S);

	return 0;
}
