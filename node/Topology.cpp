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

#include "Constants.hpp"
#include "Topology.hpp"
#include "RuntimeEnvironment.hpp"
#include "Node.hpp"
#include "Network.hpp"
#include "NetworkConfig.hpp"
#include "Buffer.hpp"
#include "Switch.hpp"

namespace ZeroTier {

/*
 * 2016-01-13 ZeroTier planet definition for the third planet of Sol:
 *
 * There are two roots, each of which is a cluster spread across multiple
 * continents and providers. They are named Alice and Bob after the
 * canonical example names used in cryptography.
 *
 * Alice:
 *
 * root-alice-ams-01: Amsterdam, Netherlands
 * root-alice-joh-01: Johannesburg, South Africa
 * root-alice-nyc-01: New York, New York, USA
 * root-alice-sao-01: Sao Paolo, Brazil
 * root-alice-sfo-01: San Francisco, California, USA
 * root-alice-sgp-01: Singapore
 *
 * Bob:
 *
 * root-bob-dfw-01: Dallas, Texas, USA
 * root-bob-fra-01: Frankfurt, Germany
 * root-bob-par-01: Paris, France
 * root-bob-syd-01: Sydney, Australia
 * root-bob-tok-01: Tokyo, Japan
 * root-bob-tor-01: Toronto, Canada
 */
#define ZT_DEFAULT_WORLD_LENGTH 634
static const unsigned char ZT_DEFAULT_WORLD[ZT_DEFAULT_WORLD_LENGTH] = {0x01,0x00,0x00,0x00,0x00,0x08,0xea,0xc9,0x0a,0x00,0x00,0x01,0x52,0x3c,0x32,0x50,0x1a,0xb8,0xb3,0x88,0xa4,0x69,0x22,0x14,0x91,0xaa,0x9a,0xcd,0x66,0xcc,0x76,0x4c,0xde,0xfd,0x56,0x03,0x9f,0x10,0x67,0xae,0x15,0xe6,0x9c,0x6f,0xb4,0x2d,0x7b,0x55,0x33,0x0e,0x3f,0xda,0xac,0x52,0x9c,0x07,0x92,0xfd,0x73,0x40,0xa6,0xaa,0x21,0xab,0xa8,0xa4,0x89,0xfd,0xae,0xa4,0x4a,0x39,0xbf,0x2d,0x00,0x65,0x9a,0xc9,0xc8,0x18,0xeb,0x4a,0xf7,0x86,0xa8,0x40,0xd6,0x52,0xea,0xae,0x9e,0x7a,0xbf,0x4c,0x97,0x66,0xab,0x2d,0x6f,0xaf,0xc9,0x2b,0x3a,0xff,0xed,0xd6,0x30,0x3e,0xc4,0x6a,0x65,0xf2,0xbd,0x83,0x52,0xf5,0x40,0xe9,0xcc,0x0d,0x6e,0x89,0x3f,0x9a,0xa0,0xb8,0xdf,0x42,0xd2,0x2f,0x84,0xe6,0x03,0x26,0x0f,0xa8,0xe3,0xcc,0x05,0x05,0x03,0xef,0x12,0x80,0x0d,0xce,0x3e,0xb6,0x58,0x3b,0x1f,0xa8,0xad,0xc7,0x25,0xf9,0x43,0x71,0xa7,0x5c,0x9a,0xc7,0xe1,0xa3,0xb8,0x88,0xd0,0x71,0x6c,0x94,0x99,0x73,0x41,0x0b,0x1b,0x48,0x84,0x02,0x9d,0x21,0x90,0x39,0xf3,0x00,0x01,0xf0,0x92,0x2a,0x98,0xe3,0xb3,0x4e,0xbc,0xbf,0xf3,0x33,0x26,0x9d,0xc2,0x65,0xd7,0xa0,0x20,0xaa,0xb6,0x9d,0x72,0xbe,0x4d,0x4a,0xcc,0x9c,0x8c,0x92,0x94,0x78,0x57,0x71,0x25,0x6c,0xd1,0xd9,0x42,0xa9,0x0d,0x1b,0xd1,0xd2,0xdc,0xa3,0xea,0x84,0xef,0x7d,0x85,0xaf,0xe6,0x61,0x1f,0xb4,0x3f,0xf0,0xb7,0x41,0x26,0xd9,0x0a,0x6e,0x00,0x0c,0x04,0xbc,0xa6,0x5e,0xb1,0x27,0x09,0x06,0x2a,0x03,0xb0,0xc0,0x00,0x02,0x00,0xd0,0x00,0x00,0x00,0x00,0x00,0x7d,0x00,0x01,0x27,0x09,0x04,0x9a,0x42,0xc5,0x21,0x27,0x09,0x06,0x2c,0x0f,0xf8,0x50,0x01,0x54,0x01,0x97,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x33,0x27,0x09,0x04,0x9f,0xcb,0x61,0xab,0x27,0x09,0x06,0x26,0x04,0xa8,0x80,0x08,0x00,0x00,0xa1,0x00,0x00,0x00,0x00,0x00,0x54,0x60,0x01,0x27,0x09,0x04,0xa9,0x39,0x8f,0x68,0x27,0x09,0x06,0x26,0x07,0xf0,0xd0,0x1d,0x01,0x00,0x57,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x27,0x09,0x04,0x6b,0xaa,0xc5,0x0e,0x27,0x09,0x06,0x26,0x04,0xa8,0x80,0x00,0x01,0x00,0x20,0x00,0x00,0x00,0x00,0x02,0x00,0xe0,0x01,0x27,0x09,0x04,0x80,0xc7,0xc5,0xd9,0x27,0x09,0x06,0x24,0x00,0x61,0x80,0x00,0x00,0x00,0xd0,0x00,0x00,0x00,0x00,0x00,0xb7,0x40,0x01,0x27,0x09,0x88,0x41,0x40,0x8a,0x2e,0x00,0xbb,0x1d,0x31,0xf2,0xc3,0x23,0xe2,0x64,0xe9,0xe6,0x41,0x72,0xc1,0xa7,0x4f,0x77,0x89,0x95,0x55,0xed,0x10,0x75,0x1c,0xd5,0x6e,0x86,0x40,0x5c,0xde,0x11,0x8d,0x02,0xdf,0xfe,0x55,0x5d,0x46,0x2c,0xcf,0x6a,0x85,0xb5,0x63,0x1c,0x12,0x35,0x0c,0x8d,0x5d,0xc4,0x09,0xba,0x10,0xb9,0x02,0x5d,0x0f,0x44,0x5c,0xf4,0x49,0xd9,0x2b,0x1c,0x00,0x0c,0x04,0x2d,0x20,0xc6,0x82,0x27,0x09,0x06,0x20,0x01,0x19,0xf0,0x64,0x00,0x81,0xc3,0x54,0x00,0x00,0xff,0xfe,0x18,0x1d,0x61,0x27,0x09,0x04,0x2e,0x65,0xa0,0xf9,0x27,0x09,0x06,0x2a,0x03,0xb0,0xc0,0x00,0x03,0x00,0xd0,0x00,0x00,0x00,0x00,0x00,0x6a,0x30,0x01,0x27,0x09,0x04,0x6b,0xbf,0x2e,0xd2,0x27,0x09,0x06,0x20,0x01,0x19,0xf0,0x68,0x00,0x83,0xa4,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x64,0x27,0x09,0x04,0x2d,0x20,0xf6,0xb3,0x27,0x09,0x06,0x20,0x01,0x19,0xf0,0x58,0x00,0x8b,0xf8,0x54,0x00,0x00,0xff,0xfe,0x15,0xb3,0x9a,0x27,0x09,0x04,0x2d,0x20,0xf8,0x57,0x27,0x09,0x06,0x20,0x01,0x19,0xf0,0x70,0x00,0x9b,0xc9,0x54,0x00,0x00,0xff,0xfe,0x15,0xc4,0xf5,0x27,0x09,0x04,0x9f,0xcb,0x02,0x9a,0x27,0x09,0x06,0x26,0x04,0xa8,0x80,0x0c,0xad,0x00,0xd0,0x00,0x00,0x00,0x00,0x00,0x26,0x70,0x01,0x27,0x09};

Topology::Topology(const RuntimeEnvironment *renv,void *tPtr) :
	RR(renv),
	_trustedPathCount(0),
	_amRoot(false)
{
	try {
		World cachedPlanet;
		std::string buf(RR->node->dataStoreGet(tPtr,"planet"));
		if (buf.length() > 0) {
			Buffer<ZT_WORLD_MAX_SERIALIZED_LENGTH> dswtmp(buf.data(),(unsigned int)buf.length());
			cachedPlanet.deserialize(dswtmp,0);
		}
		addWorld(tPtr,cachedPlanet,false);
	} catch ( ... ) {}

	World defaultPlanet;
	{
		Buffer<ZT_DEFAULT_WORLD_LENGTH> wtmp(ZT_DEFAULT_WORLD,ZT_DEFAULT_WORLD_LENGTH);
		defaultPlanet.deserialize(wtmp,0); // throws on error, which would indicate a bad static variable up top
	}
	addWorld(tPtr,defaultPlanet,false);
}

SharedPtr<Peer> Topology::addPeer(void *tPtr,const SharedPtr<Peer> &peer)
{
#ifdef ZT_TRACE
	if ((!peer)||(peer->address() == RR->identity.address())) {
		if (!peer)
			fprintf(stderr,"FATAL BUG: addPeer() caught attempt to add NULL peer" ZT_EOL_S);
		else fprintf(stderr,"FATAL BUG: addPeer() caught attempt to add peer for self" ZT_EOL_S);
		abort();
	}
#endif

	SharedPtr<Peer> np;
	{
		Mutex::Lock _l(_peers_m);
		SharedPtr<Peer> &hp = _peers[peer->address()];
		if (!hp)
			hp = peer;
		np = hp;
	}

	saveIdentity(tPtr,np->identity());

	return np;
}

SharedPtr<Peer> Topology::getPeer(void *tPtr,const Address &zta)
{
	if (zta == RR->identity.address()) {
		TRACE("BUG: ignored attempt to getPeer() for self, returned NULL");
		return SharedPtr<Peer>();
	}

	{
		Mutex::Lock _l(_peers_m);
		const SharedPtr<Peer> *const ap = _peers.get(zta);
		if (ap)
			return *ap;
	}

	try {
		Identity id(_getIdentity(tPtr,zta));
		if (id) {
			SharedPtr<Peer> np(new Peer(RR,RR->identity,id));
			{
				Mutex::Lock _l(_peers_m);
				SharedPtr<Peer> &ap = _peers[zta];
				if (!ap)
					ap.swap(np);
				return ap;
			}
		}
	} catch ( ... ) {} // invalid identity on disk?

	return SharedPtr<Peer>();
}

Identity Topology::getIdentity(void *tPtr,const Address &zta)
{
	if (zta == RR->identity.address()) {
		return RR->identity;
	} else {
		Mutex::Lock _l(_peers_m);
		const SharedPtr<Peer> *const ap = _peers.get(zta);
		if (ap)
			return (*ap)->identity();
	}
	return _getIdentity(tPtr,zta);
}

void Topology::saveIdentity(void *tPtr,const Identity &id)
{
	if (id) {
		char p[128];
		Utils::snprintf(p,sizeof(p),"iddb.d/%.10llx",(unsigned long long)id.address().toInt());
		RR->node->dataStorePut(tPtr,p,id.toString(false),false);
	}
}

SharedPtr<Peer> Topology::getUpstreamPeer(const Address *avoid,unsigned int avoidCount,bool strictAvoid)
{
	const uint64_t now = RR->node->now();
	unsigned int bestQualityOverall = ~((unsigned int)0);
	unsigned int bestQualityNotAvoid = ~((unsigned int)0);
	const SharedPtr<Peer> *bestOverall = (const SharedPtr<Peer> *)0;
	const SharedPtr<Peer> *bestNotAvoid = (const SharedPtr<Peer> *)0;

	Mutex::Lock _l1(_peers_m);
	Mutex::Lock _l2(_upstreams_m);

	for(std::vector<Address>::const_iterator a(_upstreamAddresses.begin());a!=_upstreamAddresses.end();++a) {
		const SharedPtr<Peer> *p = _peers.get(*a);
		if (p) {
			bool avoiding = false;
			for(unsigned int i=0;i<avoidCount;++i) {
				if (avoid[i] == (*p)->address()) {
					avoiding = true;
					break;
				}
			}
			const unsigned int q = (*p)->relayQuality(now);
			if (q <= bestQualityOverall) {
				bestQualityOverall = q;
				bestOverall = &(*p);
			}
			if ((!avoiding)&&(q <= bestQualityNotAvoid)) {
				bestQualityNotAvoid = q;
				bestNotAvoid = &(*p);
			}
		}
	}

	if (bestNotAvoid) {
		return *bestNotAvoid;
	} else if ((!strictAvoid)&&(bestOverall)) {
		return *bestOverall;
	}

	return SharedPtr<Peer>();
}

bool Topology::isUpstream(const Identity &id) const
{
	Mutex::Lock _l(_upstreams_m);
	return (std::find(_upstreamAddresses.begin(),_upstreamAddresses.end(),id.address()) != _upstreamAddresses.end());
}

bool Topology::shouldAcceptWorldUpdateFrom(const Address &addr) const
{
	Mutex::Lock _l(_upstreams_m);
	if (std::find(_upstreamAddresses.begin(),_upstreamAddresses.end(),addr) != _upstreamAddresses.end())
		return true;
	for(std::vector< std::pair< uint64_t,Address> >::const_iterator s(_moonSeeds.begin());s!=_moonSeeds.end();++s) {
		if (s->second == addr)
			return true;
	}
	return false;
}

ZT_PeerRole Topology::role(const Address &ztaddr) const
{
	Mutex::Lock _l(_upstreams_m);
	if (std::find(_upstreamAddresses.begin(),_upstreamAddresses.end(),ztaddr) != _upstreamAddresses.end()) {
		for(std::vector<World::Root>::const_iterator i(_planet.roots().begin());i!=_planet.roots().end();++i) {
			if (i->identity.address() == ztaddr)
				return ZT_PEER_ROLE_PLANET;
		}
		return ZT_PEER_ROLE_MOON;
	}
	return ZT_PEER_ROLE_LEAF;
}

bool Topology::isProhibitedEndpoint(const Address &ztaddr,const InetAddress &ipaddr) const
{
	Mutex::Lock _l(_upstreams_m);

	// For roots the only permitted addresses are those defined. This adds just a little
	// bit of extra security against spoofing, replaying, etc.
	if (std::find(_upstreamAddresses.begin(),_upstreamAddresses.end(),ztaddr) != _upstreamAddresses.end()) {
		for(std::vector<World::Root>::const_iterator r(_planet.roots().begin());r!=_planet.roots().end();++r) {
			if (r->identity.address() == ztaddr) {
				if (r->stableEndpoints.size() == 0)
					return false; // no stable endpoints specified, so allow dynamic paths
				for(std::vector<InetAddress>::const_iterator e(r->stableEndpoints.begin());e!=r->stableEndpoints.end();++e) {
					if (ipaddr.ipsEqual(*e))
						return false;
				}
			}
		}
		for(std::vector<World>::const_iterator m(_moons.begin());m!=_moons.end();++m) {
			for(std::vector<World::Root>::const_iterator r(m->roots().begin());r!=m->roots().end();++r) {
				if (r->identity.address() == ztaddr) {
					if (r->stableEndpoints.size() == 0)
						return false; // no stable endpoints specified, so allow dynamic paths
					for(std::vector<InetAddress>::const_iterator e(r->stableEndpoints.begin());e!=r->stableEndpoints.end();++e) {
						if (ipaddr.ipsEqual(*e))
							return false;
					}
				}
			}
		}
		return true;
	}

	return false;
}

bool Topology::addWorld(void *tPtr,const World &newWorld,bool alwaysAcceptNew)
{
	if ((newWorld.type() != World::TYPE_PLANET)&&(newWorld.type() != World::TYPE_MOON))
		return false;

	Mutex::Lock _l1(_upstreams_m);
	Mutex::Lock _l2(_peers_m);

	World *existing = (World *)0;
	switch(newWorld.type()) {
		case World::TYPE_PLANET:
			existing = &_planet;
			break;
		case World::TYPE_MOON:
			for(std::vector< World >::iterator m(_moons.begin());m!=_moons.end();++m) {
				if (m->id() == newWorld.id()) {
					existing = &(*m);
					break;
				}
			}
			break;
		default:
			return false;
	}

	if (existing) {
		if (existing->shouldBeReplacedBy(newWorld))
			*existing = newWorld;
		else return false;
	} else if (newWorld.type() == World::TYPE_MOON) {
		if (alwaysAcceptNew) {
			_moons.push_back(newWorld);
			existing = &(_moons.back());
		} else {
			for(std::vector< std::pair<uint64_t,Address> >::iterator m(_moonSeeds.begin());m!=_moonSeeds.end();++m) {
				if (m->first == newWorld.id()) {
					for(std::vector<World::Root>::const_iterator r(newWorld.roots().begin());r!=newWorld.roots().end();++r) {
						if (r->identity.address() == m->second) {
							_moonSeeds.erase(m);
							_moons.push_back(newWorld);
							existing = &(_moons.back());
							break;
						}
					}
					if (existing)
						break;
				}
			}
		}
		if (!existing)
			return false;
	} else {
		return false;
	}

	char savePath[64];
	if (existing->type() == World::TYPE_MOON) {
		Utils::snprintf(savePath,sizeof(savePath),"moons.d/%.16llx.moon",existing->id());
	} else {
		Utils::scopy(savePath,sizeof(savePath),"planet");
	}
	try {
		Buffer<ZT_WORLD_MAX_SERIALIZED_LENGTH> dswtmp;
		existing->serialize(dswtmp,false);
		RR->node->dataStorePut(tPtr,savePath,dswtmp.data(),dswtmp.size(),false);
	} catch ( ... ) {
		RR->node->dataStoreDelete(tPtr,savePath);
	}

	_memoizeUpstreams(tPtr);

	return true;
}

void Topology::addMoon(void *tPtr,const uint64_t id,const Address &seed)
{
	char savePath[64];
	Utils::snprintf(savePath,sizeof(savePath),"moons.d/%.16llx.moon",id);

	try {
		std::string moonBin(RR->node->dataStoreGet(tPtr,savePath));
		if (moonBin.length() > 1) {
			Buffer<ZT_WORLD_MAX_SERIALIZED_LENGTH> wtmp(moonBin.data(),(unsigned int)moonBin.length());
			World w;
			w.deserialize(wtmp);
			if ((w.type() == World::TYPE_MOON)&&(w.id() == id)) {
				addWorld(tPtr,w,true);
				return;
			}
		}
	} catch ( ... ) {}

	if (seed) {
		Mutex::Lock _l(_upstreams_m);
		if (std::find(_moonSeeds.begin(),_moonSeeds.end(),std::pair<uint64_t,Address>(id,seed)) == _moonSeeds.end())
			_moonSeeds.push_back(std::pair<uint64_t,Address>(id,seed));
	}
}

void Topology::removeMoon(void *tPtr,const uint64_t id)
{
	Mutex::Lock _l1(_upstreams_m);
	Mutex::Lock _l2(_peers_m);

	std::vector<World> nm;
	for(std::vector<World>::const_iterator m(_moons.begin());m!=_moons.end();++m) {
		if (m->id() != id) {
			nm.push_back(*m);
		} else {
			char savePath[64];
			Utils::snprintf(savePath,sizeof(savePath),"moons.d/%.16llx.moon",id);
			RR->node->dataStoreDelete(tPtr,savePath);
		}
	}
	_moons.swap(nm);

	std::vector< std::pair<uint64_t,Address> > cm;
	for(std::vector< std::pair<uint64_t,Address> >::const_iterator m(_moonSeeds.begin());m!=_moonSeeds.end();++m) {
		if (m->first != id)
			cm.push_back(*m);
	}
	_moonSeeds.swap(cm);

	_memoizeUpstreams(tPtr);
}

void Topology::clean(uint64_t now)
{
	{
		Mutex::Lock _l1(_peers_m);
		Mutex::Lock _l2(_upstreams_m);
		Hashtable< Address,SharedPtr<Peer> >::Iterator i(_peers);
		Address *a = (Address *)0;
		SharedPtr<Peer> *p = (SharedPtr<Peer> *)0;
		while (i.next(a,p)) {
			if ( (!(*p)->isAlive(now)) && (std::find(_upstreamAddresses.begin(),_upstreamAddresses.end(),*a) == _upstreamAddresses.end()) )
				_peers.erase(*a);
		}
	}
	{
		Mutex::Lock _l(_paths_m);
		Hashtable< Path::HashKey,SharedPtr<Path> >::Iterator i(_paths);
		Path::HashKey *k = (Path::HashKey *)0;
		SharedPtr<Path> *p = (SharedPtr<Path> *)0;
		while (i.next(k,p)) {
			if (p->reclaimIfWeak())
				_paths.erase(*k);
		}
	}
}

Identity Topology::_getIdentity(void *tPtr,const Address &zta)
{
	char p[128];
	Utils::snprintf(p,sizeof(p),"iddb.d/%.10llx",(unsigned long long)zta.toInt());
	std::string ids(RR->node->dataStoreGet(tPtr,p));
	if (ids.length() > 0) {
		try {
			return Identity(ids);
		} catch ( ... ) {} // ignore invalid IDs
	}
	return Identity();
}

void Topology::_memoizeUpstreams(void *tPtr)
{
	// assumes _upstreams_m and _peers_m are locked
	_upstreamAddresses.clear();
	_amRoot = false;

	for(std::vector<World::Root>::const_iterator i(_planet.roots().begin());i!=_planet.roots().end();++i) {
		if (i->identity == RR->identity) {
			_amRoot = true;
		} else if (std::find(_upstreamAddresses.begin(),_upstreamAddresses.end(),i->identity.address()) == _upstreamAddresses.end()) {
			_upstreamAddresses.push_back(i->identity.address());
			SharedPtr<Peer> &hp = _peers[i->identity.address()];
			if (!hp) {
				hp = new Peer(RR,RR->identity,i->identity);
				saveIdentity(tPtr,i->identity);
			}
		}
	}

	for(std::vector<World>::const_iterator m(_moons.begin());m!=_moons.end();++m) {
		for(std::vector<World::Root>::const_iterator i(m->roots().begin());i!=m->roots().end();++i) {
			if (i->identity == RR->identity) {
				_amRoot = true;
			} else if (std::find(_upstreamAddresses.begin(),_upstreamAddresses.end(),i->identity.address()) == _upstreamAddresses.end()) {
				_upstreamAddresses.push_back(i->identity.address());
				SharedPtr<Peer> &hp = _peers[i->identity.address()];
				if (!hp) {
					hp = new Peer(RR,RR->identity,i->identity);
					saveIdentity(tPtr,i->identity);
				}
			}
		}
	}

	std::sort(_upstreamAddresses.begin(),_upstreamAddresses.end());

	_cor.clear();
	for(std::vector<Address>::const_iterator a(_upstreamAddresses.begin());a!=_upstreamAddresses.end();++a) {
		if (!_cor.addRepresentative(*a))
			break;
	}
	_cor.sign(RR->identity,RR->node->now());
}

} // namespace ZeroTier
