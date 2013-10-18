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

#include <algorithm>

#include "Topology.hpp"
#include "NodeConfig.hpp"
#include "CMWC4096.hpp"

namespace ZeroTier {

#define ZT_KISSDB_HASH_TABLE_SIZE 32768
#define ZT_KISSDB_KEY_SIZE ZT_ADDRESS_LENGTH
#define ZT_KISSDB_VALUE_SIZE ZT_PEER_MAX_SERIALIZED_LENGTH

Topology::Topology(const RuntimeEnvironment *renv,const char *dbpath) :
	_r(renv),
	_amSupernode(false)
{
	if (KISSDB_open(&_dbm,dbpath,KISSDB_OPEN_MODE_RWCREAT,ZT_KISSDB_HASH_TABLE_SIZE,ZT_KISSDB_KEY_SIZE,ZT_KISSDB_VALUE_SIZE)) {
		if (KISSDB_open(&_dbm,dbpath,KISSDB_OPEN_MODE_RWREPLACE,ZT_KISSDB_HASH_TABLE_SIZE,ZT_KISSDB_KEY_SIZE,ZT_KISSDB_VALUE_SIZE))
			throw std::runtime_error("unable to open peer database (rw/create)");
	}

	if ((_dbm.key_size != ZT_KISSDB_KEY_SIZE)||(_dbm.value_size != ZT_KISSDB_VALUE_SIZE)||(_dbm.hash_table_size != ZT_KISSDB_HASH_TABLE_SIZE)) {
		KISSDB_close(&_dbm);
		if (KISSDB_open(&_dbm,dbpath,KISSDB_OPEN_MODE_RWREPLACE,ZT_KISSDB_HASH_TABLE_SIZE,ZT_KISSDB_KEY_SIZE,ZT_KISSDB_VALUE_SIZE))
			throw std::runtime_error("unable to open peer database (recreate)");
	}

	Utils::lockDownFile(dbpath,false); // node.db caches secrets
}

Topology::~Topology()
{
	// Flush last changes to disk
	clean();
}

void Topology::setSupernodes(const std::map< Identity,std::vector<InetAddress> > &sn)
{
	Mutex::Lock _l(_supernodes_m);

	_supernodes = sn;
	_supernodeAddresses.clear();
	_supernodePeers.clear();

	for(std::map< Identity,std::vector<InetAddress> >::const_iterator i(sn.begin());i!=sn.end();++i) {
		if (i->first != _r->identity) {
			SharedPtr<Peer> p(getPeer(i->first.address()));
			if (!p)
				p = addPeer(SharedPtr<Peer>(new Peer(_r->identity,i->first)));
			for(std::vector<InetAddress>::const_iterator j(i->second.begin());j!=i->second.end();++j)
				p->setPathAddress(*j,true);
			_supernodePeers.push_back(p);
		}
		_supernodeAddresses.insert(i->first.address());
	}

	_amSupernode = (_supernodes.find(_r->identity) != _supernodes.end());
}

SharedPtr<Peer> Topology::addPeer(const SharedPtr<Peer> &peer)
{
	if (peer->address() == _r->identity.address()) {
		TRACE("BUG: addNewPeer() caught and ignored attempt to add peer for self");
		throw std::logic_error("cannot add peer for self");
	}

	SharedPtr<Peer> actualPeer;
	{
		Mutex::Lock _l(_activePeers_m);
		actualPeer = _activePeers.insert(std::pair< Address,SharedPtr<Peer> >(peer->address(),peer)).first->second;
	}

	uint64_t atmp[ZT_ADDRESS_LENGTH];
	actualPeer->address().copyTo(atmp,ZT_ADDRESS_LENGTH);

	Buffer<ZT_PEER_MAX_SERIALIZED_LENGTH> b;
	actualPeer->serialize(b);
	b.zeroUnused();

	_dbm_m.lock();
	if (KISSDB_put(&_dbm,atmp,b.data())) {
		TRACE("error writing %s to peerdb",actualPeer->address().toString().c_str());
	} else actualPeer->getAndResetDirty();
	_dbm_m.unlock();

	return actualPeer;
}

SharedPtr<Peer> Topology::getPeer(const Address &zta)
{
	if (zta == _r->identity.address()) {
		TRACE("BUG: ignored attempt to getPeer() for self, returned NULL");
		return SharedPtr<Peer>();
	}

	{
		Mutex::Lock _l(_activePeers_m);
		std::map< Address,SharedPtr<Peer> >::const_iterator ap(_activePeers.find(zta));
		if ((ap != _activePeers.end())&&(ap->second))
			return ap->second;
	}

	unsigned char ztatmp[ZT_ADDRESS_LENGTH];
	zta.copyTo(ztatmp,ZT_ADDRESS_LENGTH);

	Buffer<ZT_KISSDB_VALUE_SIZE> b(ZT_KISSDB_VALUE_SIZE);
	_dbm_m.lock();
	if (!KISSDB_get(&_dbm,ztatmp,b.data())) {
		_dbm_m.unlock();

		SharedPtr<Peer> p(new Peer());
		try {
			p->deserialize(b,0);
			Mutex::Lock _l(_activePeers_m);
			_activePeers[zta] = p;
			return p;
		} catch ( ... ) {
			TRACE("unexpected exception deserializing peer %s from peerdb",zta.toString().c_str());
			return SharedPtr<Peer>();
		}
	} else _dbm_m.unlock();

	return SharedPtr<Peer>();
}

SharedPtr<Peer> Topology::getBestSupernode(const Address *avoid,unsigned int avoidCount,bool strictAvoid) const
{
	SharedPtr<Peer> bestSupernode;
	unsigned int bestSupernodeLatency = 0xffff;
	uint64_t now = Utils::now();

	Mutex::Lock _l(_supernodes_m);

	if (_supernodePeers.empty())
		return bestSupernode;

	for(std::vector< SharedPtr<Peer> >::const_iterator sn=_supernodePeers.begin();sn!=_supernodePeers.end();) {
		for(unsigned int i=0;i<avoidCount;++i) {
			if (avoid[i] == (*sn)->address())
				goto skip_and_try_next_supernode;
		}
		if ((*sn)->hasActiveDirectPath(now)) {
			unsigned int l = (*sn)->latency();
			if (bestSupernode) {
				if ((l)&&(l < bestSupernodeLatency)) {
					bestSupernodeLatency = l;
					bestSupernode = *sn;
				}
			} else {
				if (l)
					bestSupernodeLatency = l;
				bestSupernode = *sn;
			}
		}
skip_and_try_next_supernode:
		++sn;
	}

	if ((bestSupernode)||(strictAvoid))
		return bestSupernode;

	for(std::vector< SharedPtr<Peer> >::const_iterator sn=_supernodePeers.begin();sn!=_supernodePeers.end();++sn) {
		if ((*sn)->hasActiveDirectPath(now)) {
			unsigned int l = (*sn)->latency();
			if (bestSupernode) {
				if ((l)&&(l < bestSupernodeLatency)) {
					bestSupernodeLatency = l;
					bestSupernode = *sn;
				}
			} else {
				if (l)
					bestSupernodeLatency = l;
				bestSupernode = *sn;
			}
		}
	}

	if (bestSupernode)
		return bestSupernode;

	return _supernodePeers[_r->prng->next32() % _supernodePeers.size()];
}

void Topology::clean()
{
	TRACE("cleaning caches and flushing modified peers to disk...");

	Mutex::Lock _l(_activePeers_m);
	for(std::map< Address,SharedPtr<Peer> >::iterator p(_activePeers.begin());p!=_activePeers.end();++p) {
		if (p->second->getAndResetDirty()) {
			try {
				uint64_t atmp[ZT_ADDRESS_LENGTH];
				p->second->identity().address().copyTo(atmp,ZT_ADDRESS_LENGTH);

				Buffer<ZT_PEER_MAX_SERIALIZED_LENGTH> b;
				p->second->serialize(b);
				b.zeroUnused();

				_dbm_m.lock();
				if (KISSDB_put(&_dbm,atmp,b.data())) {
					TRACE("error writing %s to peer.db",p->second->identity().address().toString().c_str());
				}
				_dbm_m.unlock();
			} catch ( ... ) {
				TRACE("unexpected exception flushing %s to peer.db",p->second->identity().address().toString().c_str());
			}
		}
	}
}

} // namespace ZeroTier
