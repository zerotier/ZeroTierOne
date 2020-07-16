/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include "Constants.hpp"
#include "Topology.hpp"
#include "RuntimeEnvironment.hpp"
#include "Node.hpp"
#include "Network.hpp"
#include "NetworkConfig.hpp"
#include "Buffer.hpp"
#include "Switch.hpp"

namespace ZeroTier {

#define ZT_DEFAULT_WORLD_LENGTH 674
static const unsigned char ZT_DEFAULT_WORLD[ZT_DEFAULT_WORLD_LENGTH] = {0x01,0x00,0x00,0x00,0x00,0x08,0xea,0xc9,0x0a,0x00,0x00,0x01,0x6c,0xf9,0x10,0xd4,0x79,0xb8,0xb3,0x88,0xa4,0x69,0x22,0x14,0x91,0xaa,0x9a,0xcd,0x66,0xcc,0x76,0x4c,0xde,0xfd,0x56,0x03,0x9f,0x10,0x67,0xae,0x15,0xe6,0x9c,0x6f,0xb4,0x2d,0x7b,0x55,0x33,0x0e,0x3f,0xda,0xac,0x52,0x9c,0x07,0x92,0xfd,0x73,0x40,0xa6,0xaa,0x21,0xab,0xa8,0xa4,0x89,0xfd,0xae,0xa4,0x4a,0x39,0xbf,0x2d,0x00,0x65,0x9a,0xc9,0xc8,0x18,0xeb,0x3e,0x3a,0xe9,0xeb,0x4e,0x78,0x27,0xb8,0xeb,0x78,0xe7,0x0f,0x64,0xa0,0x14,0xce,0x3d,0x30,0x21,0x96,0x23,0x9d,0x07,0x85,0xa4,0x0b,0xc6,0xf3,0x03,0x48,0x12,0x66,0x09,0x2a,0x6f,0xa1,0x5b,0x55,0x71,0x43,0xe7,0x2d,0xb3,0xfc,0xfc,0x8e,0x6f,0xe5,0xbb,0x5d,0x80,0x76,0x28,0x8d,0x32,0x87,0x24,0x3e,0x59,0x32,0x3d,0x9f,0xd1,0x00,0x54,0xd4,0xa2,0x90,0x0d,0xfc,0x3a,0xc9,0x5e,0xd8,0x6b,0x11,0x24,0xf9,0x70,0x8b,0x6e,0xd9,0x09,0xec,0xce,0x59,0x06,0xa6,0x73,0xf4,0x46,0x34,0x45,0xcd,0x57,0x44,0x04,0x3a,0x46,0xf1,0xbf,0x30,0x00,0x76,0xe6,0x6f,0xab,0x33,0xe2,0x85,0x49,0xa6,0x2e,0xe2,0x06,0x4d,0x18,0x43,0x27,0x3c,0x2c,0x30,0x0b,0xa4,0x5c,0x3f,0x20,0xbe,0xf0,0x2d,0xba,0xd2,0x25,0x72,0x3b,0xb5,0x9a,0x9b,0xb4,0xb1,0x35,0x35,0x73,0x09,0x61,0xae,0xec,0xf5,0xa1,0x63,0xac,0xe4,0x77,0xcc,0xeb,0x07,0x27,0x02,0x5b,0x99,0xac,0x14,0xa5,0x16,0x6a,0x09,0xa3,0x00,0x04,0x04,0xb9,0xb4,0x0d,0x52,0x27,0x09,0x06,0x2a,0x02,0x6e,0xa0,0xc8,0x15,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x27,0x09,0x04,0xb9,0xb4,0x0d,0x52,0x01,0xbb,0x06,0x2a,0x02,0x6e,0xa0,0xc8,0x15,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0xbb,0xde,0x89,0x50,0xa8,0xb2,0x00,0x1b,0x3a,0xda,0x82,0x51,0xb9,0x1b,0x6b,0x6f,0xa6,0x53,0x5b,0x8c,0x7e,0x24,0x60,0x91,0x8f,0x4f,0x72,0x9a,0xbd,0xec,0x97,0xd3,0xc7,0xf3,0x79,0x68,0x68,0xfb,0x02,0xf0,0xde,0x0b,0x0e,0xe5,0x54,0xb2,0xd5,0x9f,0xc3,0x52,0x47,0x43,0xee,0xbf,0xcf,0x53,0x15,0xe7,0x90,0xed,0x6d,0x92,0xdb,0x5b,0xd1,0x0c,0x28,0xc0,0x9b,0x40,0xef,0x00,0x04,0x04,0xcf,0xf6,0x49,0xf5,0x27,0x09,0x06,0x20,0x01,0x19,0xf0,0x90,0x02,0x05,0xcb,0x0e,0xc4,0x7a,0xff,0xfe,0x8f,0x69,0xd9,0x27,0x09,0x04,0xcf,0xf6,0x49,0xf5,0x01,0xbb,0x06,0x20,0x01,0x19,0xf0,0x90,0x02,0x05,0xcb,0x0e,0xc4,0x7a,0xff,0xfe,0x8f,0x69,0xd9,0x01,0xbb,0x34,0xe0,0xa5,0xe1,0x74,0x00,0x93,0xef,0xb5,0x09,0x34,0x78,0x8f,0x85,0x6d,0x5c,0xfb,0x9c,0xa5,0xbe,0x88,0xe8,0x5b,0x40,0x96,0x55,0x86,0xb7,0x5b,0xef,0xac,0x90,0x0d,0xf7,0x73,0x52,0xc1,0x45,0xa1,0xba,0x70,0x07,0x56,0x9d,0x37,0xc7,0x7b,0xfe,0x52,0xc0,0x99,0x9f,0x3b,0xdc,0x67,0xa4,0x7a,0x4a,0x60,0x00,0xb7,0x20,0xa8,0x83,0xce,0x47,0xaa,0x2f,0xb7,0xf8,0x00,0x04,0x04,0x93,0x4b,0x5c,0x02,0x27,0x09,0x06,0x26,0x04,0x13,0x80,0x30,0x00,0x71,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x27,0x09,0x04,0x93,0x4b,0x5c,0x02,0x01,0xbb,0x06,0x26,0x04,0x13,0x80,0x30,0x00,0x71,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x01,0xbb,0x99,0x2f,0xcf,0x1d,0xb7,0x00,0x20,0x6e,0xd5,0x93,0x50,0xb3,0x19,0x16,0xf7,0x49,0xa1,0xf8,0x5d,0xff,0xb3,0xa8,0x78,0x7d,0xcb,0xf8,0x3b,0x8c,0x6e,0x94,0x48,0xd4,0xe3,0xea,0x0e,0x33,0x69,0x30,0x1b,0xe7,0x16,0xc3,0x60,0x93,0x44,0xa9,0xd1,0x53,0x38,0x50,0xfb,0x44,0x60,0xc5,0x0a,0xf4,0x33,0x22,0xbc,0xfc,0x8e,0x13,0xd3,0x30,0x1a,0x1f,0x10,0x03,0xce,0xb6,0x00,0x04,0x04,0xc3,0xb5,0xad,0x9f,0x27,0x09,0x06,0x2a,0x02,0x6e,0xa0,0xc0,0x24,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x27,0x09,0x04,0xc3,0xb5,0xad,0x9f,0x01,0xbb,0x06,0x2a,0x02,0x6e,0xa0,0xc0,0x24,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0xbb};

Topology::Topology(const RuntimeEnvironment *renv,void *tPtr) :
	RR(renv),
	_numConfiguredPhysicalPaths(0),
	_amUpstream(false)
{
	uint8_t tmp[ZT_WORLD_MAX_SERIALIZED_LENGTH];
	uint64_t idtmp[2];
	idtmp[0] = 0; idtmp[1] = 0;
	int n = RR->node->stateObjectGet(tPtr,ZT_STATE_OBJECT_PLANET,idtmp,tmp,sizeof(tmp));
	if (n > 0) {
		try {
			World cachedPlanet;
			cachedPlanet.deserialize(Buffer<ZT_WORLD_MAX_SERIALIZED_LENGTH>(tmp,(unsigned int)n),0);
			addWorld(tPtr,cachedPlanet,false);
		} catch ( ... ) {} // ignore invalid cached planets
	}

	World defaultPlanet;
	{
		Buffer<ZT_DEFAULT_WORLD_LENGTH> wtmp(ZT_DEFAULT_WORLD,ZT_DEFAULT_WORLD_LENGTH);
		defaultPlanet.deserialize(wtmp,0); // throws on error, which would indicate a bad static variable up top
	}
	addWorld(tPtr,defaultPlanet,false);
}

Topology::~Topology()
{
	Hashtable< Address,SharedPtr<Peer> >::Iterator i(_peers);
	Address *a = (Address *)0;
	SharedPtr<Peer> *p = (SharedPtr<Peer> *)0;
	while (i.next(a,p))
		_savePeer((void *)0,*p);
}

SharedPtr<Peer> Topology::addPeer(void *tPtr,const SharedPtr<Peer> &peer)
{
	SharedPtr<Peer> np;
	{
		Mutex::Lock _l(_peers_m);
		SharedPtr<Peer> &hp = _peers[peer->address()];
		if (!hp)
			hp = peer;
		np = hp;
	}
	return np;
}

SharedPtr<Peer> Topology::getPeer(void *tPtr,const Address &zta)
{
	if (zta == RR->identity.address())
		return SharedPtr<Peer>();

	{
		Mutex::Lock _l(_peers_m);
		const SharedPtr<Peer> *const ap = _peers.get(zta);
		if (ap)
			return *ap;
	}

	try {
		Buffer<ZT_PEER_MAX_SERIALIZED_STATE_SIZE> buf;
		uint64_t idbuf[2]; idbuf[0] = zta.toInt(); idbuf[1] = 0;
		int len = RR->node->stateObjectGet(tPtr,ZT_STATE_OBJECT_PEER,idbuf,buf.unsafeData(),ZT_PEER_MAX_SERIALIZED_STATE_SIZE);
		if (len > 0) {
			buf.setSize(len);
			Mutex::Lock _l(_peers_m);
			SharedPtr<Peer> &ap = _peers[zta];
			if (ap)
				return ap;
			ap = Peer::deserializeFromCache(RR->node->now(),tPtr,buf,RR);
			if (!ap) {
				_peers.erase(zta);
			}
			return SharedPtr<Peer>();
		}
	} catch ( ... ) {} // ignore invalid identities or other strange failures

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
	return Identity();
}

SharedPtr<Peer> Topology::getUpstreamPeer()
{
	const int64_t now = RR->node->now();
	unsigned int bestq = ~((unsigned int)0);
	const SharedPtr<Peer> *best = (const SharedPtr<Peer> *)0;

	Mutex::Lock _l2(_peers_m);
	Mutex::Lock _l1(_upstreams_m);

	for(std::vector<Address>::const_iterator a(_upstreamAddresses.begin());a!=_upstreamAddresses.end();++a) {
		const SharedPtr<Peer> *p = _peers.get(*a);
		if (p) {
			const unsigned int q = (*p)->relayQuality(now);
			if (q <= bestq) {
				bestq = q;
				best = p;
			}
		}
	}

	if (!best)
		return SharedPtr<Peer>();
	return *best;
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
				if (r->stableEndpoints.empty())
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
					if (r->stableEndpoints.empty())
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

	Mutex::Lock _l2(_peers_m);
	Mutex::Lock _l1(_upstreams_m);

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

	try {
		Buffer<ZT_WORLD_MAX_SERIALIZED_LENGTH> sbuf;
		existing->serialize(sbuf,false);
		uint64_t idtmp[2];
		idtmp[0] = existing->id(); idtmp[1] = 0;
		RR->node->stateObjectPut(tPtr,(existing->type() == World::TYPE_PLANET) ? ZT_STATE_OBJECT_PLANET : ZT_STATE_OBJECT_MOON,idtmp,sbuf.data(),sbuf.size());
	} catch ( ... ) {}

	_memoizeUpstreams(tPtr);

	return true;
}

void Topology::addMoon(void *tPtr,const uint64_t id,const Address &seed)
{
	char tmp[ZT_WORLD_MAX_SERIALIZED_LENGTH];
	uint64_t idtmp[2];
	idtmp[0] = id; idtmp[1] = 0;
	int n = RR->node->stateObjectGet(tPtr,ZT_STATE_OBJECT_MOON,idtmp,tmp,sizeof(tmp));
	if (n > 0) {
		try {
			World w;
			w.deserialize(Buffer<ZT_WORLD_MAX_SERIALIZED_LENGTH>(tmp,(unsigned int)n));
			if ((w.type() == World::TYPE_MOON)&&(w.id() == id)) {
				addWorld(tPtr,w,true);
				return;
			}
		} catch ( ... ) {}
	}

	if (seed) {
		Mutex::Lock _l(_upstreams_m);
		if (std::find(_moonSeeds.begin(),_moonSeeds.end(),std::pair<uint64_t,Address>(id,seed)) == _moonSeeds.end())
			_moonSeeds.push_back(std::pair<uint64_t,Address>(id,seed));
	}
}

void Topology::removeMoon(void *tPtr,const uint64_t id)
{
	Mutex::Lock _l2(_peers_m);
	Mutex::Lock _l1(_upstreams_m);

	std::vector<World> nm;
	for(std::vector<World>::const_iterator m(_moons.begin());m!=_moons.end();++m) {
		if (m->id() != id) {
			nm.push_back(*m);
		} else {
			uint64_t idtmp[2];
			idtmp[0] = id; idtmp[1] = 0;
			RR->node->stateObjectDelete(tPtr,ZT_STATE_OBJECT_MOON,idtmp);
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

void Topology::doPeriodicTasks(void *tPtr,int64_t now)
{
	{
		Mutex::Lock _l1(_peers_m);
		Mutex::Lock _l2(_upstreams_m);
		Hashtable< Address,SharedPtr<Peer> >::Iterator i(_peers);
		Address *a = (Address *)0;
		SharedPtr<Peer> *p = (SharedPtr<Peer> *)0;
		while (i.next(a,p)) {
			if ( (!(*p)->isAlive(now)) && (std::find(_upstreamAddresses.begin(),_upstreamAddresses.end(),*a) == _upstreamAddresses.end()) ) {
				_savePeer(tPtr,*p);
				_peers.erase(*a);
			}
		}
	}

	{
		Mutex::Lock _l(_paths_m);
		Hashtable< Path::HashKey,SharedPtr<Path> >::Iterator i(_paths);
		Path::HashKey *k = (Path::HashKey *)0;
		SharedPtr<Path> *p = (SharedPtr<Path> *)0;
		while (i.next(k,p)) {
			if (p->references() <= 1)
				_paths.erase(*k);
		}
	}
}

void Topology::_memoizeUpstreams(void *tPtr)
{
	// assumes _upstreams_m and _peers_m are locked
	_upstreamAddresses.clear();
	_amUpstream = false;

	for(std::vector<World::Root>::const_iterator i(_planet.roots().begin());i!=_planet.roots().end();++i) {
		if (i->identity == RR->identity) {
			_amUpstream = true;
		} else if (std::find(_upstreamAddresses.begin(),_upstreamAddresses.end(),i->identity.address()) == _upstreamAddresses.end()) {
			_upstreamAddresses.push_back(i->identity.address());
			SharedPtr<Peer> &hp = _peers[i->identity.address()];
			if (!hp)
				hp = new Peer(RR,RR->identity,i->identity);
		}
	}

	for(std::vector<World>::const_iterator m(_moons.begin());m!=_moons.end();++m) {
		for(std::vector<World::Root>::const_iterator i(m->roots().begin());i!=m->roots().end();++i) {
			if (i->identity == RR->identity) {
				_amUpstream = true;
			} else if (std::find(_upstreamAddresses.begin(),_upstreamAddresses.end(),i->identity.address()) == _upstreamAddresses.end()) {
				_upstreamAddresses.push_back(i->identity.address());
				SharedPtr<Peer> &hp = _peers[i->identity.address()];
				if (!hp)
					hp = new Peer(RR,RR->identity,i->identity);
			}
		}
	}

	std::sort(_upstreamAddresses.begin(),_upstreamAddresses.end());
}

void Topology::_savePeer(void *tPtr,const SharedPtr<Peer> &peer)
{
	try {
		Buffer<ZT_PEER_MAX_SERIALIZED_STATE_SIZE> buf;
		peer->serializeForCache(buf);
		uint64_t tmpid[2]; tmpid[0] = peer->address().toInt(); tmpid[1] = 0;
		RR->node->stateObjectPut(tPtr,ZT_STATE_OBJECT_PEER,tmpid,buf.data(),buf.size());
	} catch ( ... ) {} // sanity check, discard invalid entries
}

} // namespace ZeroTier
