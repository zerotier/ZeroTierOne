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

#include "Topology.hpp"
#include "NodeConfig.hpp"

namespace ZeroTier {

#define ZT_KISSDB_HASH_TABLE_SIZE 131072
#define ZT_KISSDB_KEY_SIZE ZT_ADDRESS_LENGTH
#define ZT_KISSDB_VALUE_SIZE ZT_PEER_MAX_SERIALIZED_LENGTH

Topology::Topology(const RuntimeEnvironment *renv,const char *dbpath)
	throw(std::runtime_error) :
	Thread(),
	_r(renv)
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

	start();
}

Topology::~Topology()
{
	{
		Mutex::Lock _l(_peerDeepVerifyJobs_m);
		_peerDeepVerifyJobs.push_back(_PeerDeepVerifyJob());
		_peerDeepVerifyJobs.back().type = _PeerDeepVerifyJob::CLEAN_CACHE;
		_peerDeepVerifyJobs.push_back(_PeerDeepVerifyJob());
		_peerDeepVerifyJobs.back().type = _PeerDeepVerifyJob::EXIT_THREAD;
	}
	_peerDeepVerifyJobs_c.signal();

	while (running())
		Thread::sleep(10); // wait for thread to terminate without join()

	KISSDB_close(&_dbm);
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
			if ((!p)||(p->identity() != i->first)) {
				p = SharedPtr<Peer>(new Peer(_r->identity,i->first));
				_reallyAddPeer(p);
			}
			for(std::vector<InetAddress>::const_iterator j(i->second.begin());j!=i->second.end();++j)
				p->setPathAddress(*j,true);
			_supernodePeers.push_back(p);
		}
		_supernodeAddresses.insert(i->first.address());
	}
}

void Topology::addPeer(const SharedPtr<Peer> &candidate,void (*callback)(void *,const SharedPtr<Peer> &,Topology::PeerVerifyResult),void *arg)
{
	if (candidate->address() != _r->identity.address()) {
		Mutex::Lock _l(_peerDeepVerifyJobs_m);
		_peerDeepVerifyJobs.push_back(_PeerDeepVerifyJob());
		_PeerDeepVerifyJob &job = _peerDeepVerifyJobs.back();
		job.callback = callback;
		job.arg = arg;
		job.candidate = candidate;
		job.type = _PeerDeepVerifyJob::VERIFY_PEER;
		_peerDeepVerifyJobs_c.signal();
	} else {
		TRACE("BUG: addPeer() caught and ignored attempt to add peer for self");
		if (callback)
			callback(arg,candidate,PEER_VERIFY_REJECTED_DUPLICATE_TRIAGED);
	}
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

	Buffer<ZT_KISSDB_VALUE_SIZE> b(ZT_KISSDB_VALUE_SIZE);
	_dbm_m.lock();
	if (!KISSDB_get(&_dbm,zta.data(),b.data())) {
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

SharedPtr<Peer> Topology::getBestSupernode(const Address *avoid,unsigned int avoidCount) const
{
	SharedPtr<Peer> bestSupernode;
	unsigned long bestSupernodeLatency = 0xffff;
	uint64_t now = Utils::now();

	Mutex::Lock _l(_supernodes_m);

	for(std::vector< SharedPtr<Peer> >::const_iterator sn=_supernodePeers.begin();sn!=_supernodePeers.end();) {
		for(unsigned int i=0;i<avoidCount;++i) {
			if (avoid[i] == (*sn)->address())
				goto skip_and_try_next_supernode;
		}
		if ((*sn)->hasActiveDirectPath(now)) { // only consider those that responded to pings
			unsigned int l = (*sn)->latency();
			if ((l)&&(l <= bestSupernodeLatency)) {
				bestSupernodeLatency = l;
				bestSupernode = *sn;
			}
		}
skip_and_try_next_supernode:
		++sn;
	}

	if (bestSupernode)
		return bestSupernode;

	for(std::vector< SharedPtr<Peer> >::const_iterator sn=_supernodePeers.begin();sn!=_supernodePeers.end();++sn) {
		if ((*sn)->hasActiveDirectPath(now)) { // only consider those that responded to pings
			unsigned int l = (*sn)->latency();
			if ((l)&&(l <= bestSupernodeLatency)) {
				bestSupernodeLatency = l;
				bestSupernode = *sn;
			}
		}
	}

	if (bestSupernode)
		return bestSupernode;

	uint64_t bestSupernodeLastDirectReceive = 0;
	for(std::vector< SharedPtr<Peer> >::const_iterator sn=_supernodePeers.begin();sn!=_supernodePeers.end();++sn) {
		uint64_t l = (*sn)->lastDirectReceive();
		if (l > bestSupernodeLastDirectReceive) {
			bestSupernodeLastDirectReceive = l;
			bestSupernode = *sn;
		}
	}

	return bestSupernode;
}

void Topology::clean()
{
	{
		Mutex::Lock _l(_peerDeepVerifyJobs_m);
		_peerDeepVerifyJobs.push_back(_PeerDeepVerifyJob());
		_peerDeepVerifyJobs.back().type = _PeerDeepVerifyJob::CLEAN_CACHE;
	}
	_peerDeepVerifyJobs_c.signal();
}

void Topology::likesMulticastGroup(uint64_t nwid,const MulticastGroup &mg,const Address &addr,uint64_t now)
{
	Mutex::Lock _l(_multicastGroupMembers_m);
	_multicastGroupMembers[nwid][mg][addr] = now;
}

struct _PickMulticastPropagationPeersPeerPrioritySortOrder
{
	inline bool operator()(const SharedPtr<Peer> &p1,const SharedPtr<Peer> &p2) const
	{
		return (p1->lastUnicastFrame() >= p2->lastUnicastFrame());
	}
};
#define _MAX_PEERS_TO_CONSIDER 256
unsigned int Topology::pickMulticastPropagationPeers(uint64_t nwid,const Address &exclude,const void *propagationBloom,unsigned int propagationBloomSize,unsigned int count,const MulticastGroup &mg,SharedPtr<Peer> *peers)
{
	SharedPtr<Peer> possiblePeers[_MAX_PEERS_TO_CONSIDER];
	unsigned int numPossiblePeers = 0;

	if (count > _MAX_PEERS_TO_CONSIDER)
		count = _MAX_PEERS_TO_CONSIDER;

	Mutex::Lock _l1(_activePeers_m);
	Mutex::Lock _l2(_supernodes_m);

	// Grab known non-supernode peers in multicast group, excluding 'exclude'
	// Also lazily clean up the _multicastGroupMembers structure
	{
		Mutex::Lock _l3(_multicastGroupMembers_m);
		std::map< uint64_t,std::map< MulticastGroup,std::map< Address,uint64_t > > >::iterator mgm(_multicastGroupMembers.find(nwid));
		if (mgm != _multicastGroupMembers.end()) {
			std::map< MulticastGroup,std::map< Address,uint64_t > >::iterator g(mgm->second.find(mg));
			if (g != mgm->second.end()) {
				uint64_t now = Utils::now();
				for(std::map< Address,uint64_t >::iterator m(g->second.begin());m!=g->second.end();) {
					if (((now - m->second) < ZT_MULTICAST_LIKE_EXPIRE)&&(m->first != exclude)) {
						std::map< Address,SharedPtr<Peer> >::const_iterator p(_activePeers.find(m->first));
						if (p != _activePeers.end()) {
							possiblePeers[numPossiblePeers++] = p->second;
							if (numPossiblePeers > _MAX_PEERS_TO_CONSIDER)
								break;
						}
						++m;
					} else g->second.erase(m++);
				}
				if (!g->second.size())
					mgm->second.erase(g);
			}
		}
	}

	// Sort non-supernode peers in descending order of most recent data
	// exchange timestamp. This sorts by implicit social relationships -- who
	// you are talking to are the people who get multicasts first.
	std::sort(&(possiblePeers[0]),&(possiblePeers[numPossiblePeers]),_PickMulticastPropagationPeersPeerPrioritySortOrder());

	// Tack on a supernode peer to the end if we don't have enough regular
	// peers, using supernodes to bridge gaps in sparse multicast groups.
	if (numPossiblePeers < count) {
		SharedPtr<Peer> bestSupernode;
		unsigned int bestSupernodeLatency = 0xffff;
		for(std::vector< SharedPtr<Peer> >::const_iterator sn(_supernodePeers.begin());sn!=_supernodePeers.end();++sn) {
			if (((*sn)->latency())&&((*sn)->latency() < bestSupernodeLatency)) {
				bestSupernodeLatency = (*sn)->latency();
				bestSupernode = *sn;
			}
		}
		if (bestSupernode)
			possiblePeers[numPossiblePeers++] = bestSupernode;
	}

	unsigned int num = 0;

	// First, try to pick peers not in the propgation bloom filter
	for(unsigned int i=0;i<numPossiblePeers;++i) {
		if (!Utils::bloomContains(propagationBloom,propagationBloomSize,possiblePeers[i]->address().sum())) {
			peers[num++] = possiblePeers[i];
			if (num >= count)
				return num;
		}
	}

	// Next, pick other peers until full (without duplicates)
	for(unsigned int i=0;i<numPossiblePeers;++i) {
		for(unsigned int j=0;j<num;++j) {
			if (peers[j] == possiblePeers[i])
				goto check_next_peer;
		}
		peers[num++] = possiblePeers[i];
		if (num >= count)
			return num;
check_next_peer:
		continue;
	}

	return num;
}

void Topology::main()
	throw()
{
	for(;;) {
		_peerDeepVerifyJobs_m.lock();
		if (_peerDeepVerifyJobs.empty()) {
			_peerDeepVerifyJobs_m.unlock();
			_peerDeepVerifyJobs_c.wait();
			continue;
		}
		_PeerDeepVerifyJob job(_peerDeepVerifyJobs.front());
		_peerDeepVerifyJobs.pop_front();
		unsigned long queueRemaining = _peerDeepVerifyJobs.size();
		_peerDeepVerifyJobs_m.unlock();

		switch(job.type) {
			case _PeerDeepVerifyJob::VERIFY_PEER:
				/* TODO: We should really verify peers every time completely if this
				 * is a supernode, perhaps deferring the expensive part for new
				 * addresses. An attempt at claim jumping should also trigger a
				 * short duration ban of the originating IP address in most cases,
				 * since this means either malicious intent or broken software. */
				TRACE("verifying peer: %s",job.candidate->identity().address().toString().c_str());

				if ((job.candidate->identity())&&(!job.candidate->identity().address().isReserved())&&(job.candidate->identity().locallyValidate(false))) {
					// Peer passes sniff test, so check to see if we've already got
					// one with the same address.

					SharedPtr<Peer> existingPeer(getPeer(job.candidate->identity().address()));

					if (existingPeer) {
						if (existingPeer->identity() == job.candidate->identity()) {
							// It's an *exact* duplicate, so return the existing peer
							if (job.callback)
								job.callback(job.arg,existingPeer,PEER_VERIFY_ACCEPTED_ALREADY_HAVE);
						} else if (queueRemaining > 3) {
							/* Prevents a CPU hog DOS attack, while allowing a very unlikely kind of
							 * DOS attack where someone knows someone else's address prior to their
							 * registering it and claim-jumps them and then floods with bad identities
							 * to hold their claim. Of the two, the latter would be infeasable
							 * without already having cracked the target's machine in which case
							 * the attacker has their private key anyway and can really steal their
							 * identity. So why bother.*/
							TRACE("%s is duplicate, load too high, old won",job.candidate->identity().address().toString().c_str());
							if (job.callback)
								job.callback(job.arg,job.candidate,PEER_VERIFY_REJECTED_DUPLICATE_TRIAGED);
						} else {
							// It's different so deeply validate it first, then the
							// existing claimant, and toss the imposter. If both verify, the
							// one we already have wins.

							if (!job.candidate->identity().locallyValidate(true)) {
								LOG("Topology: IMPOSTER %s rejected",job.candidate->identity().address().toString().c_str());
								if (job.callback)
									job.callback(job.arg,job.candidate,PEER_VERIFY_REJECTED_INVALID_IDENTITY);
							} else if (!existingPeer->identity().locallyValidate(true)) {
								LOG("Topology: previous IMPOSTER %s displaced by valid identity!",job.candidate->identity().address().toString().c_str());
								_reallyAddPeer(job.candidate);
								if (job.callback)
									job.callback(job.arg,job.candidate,PEER_VERIFY_ACCEPTED_DISPLACED_INVALID_ADDRESS);
							} else {
								LOG("Topology: tie between apparently valid claims on %s, oldest won",job.candidate->identity().address().toString().c_str());
								if (job.callback)
									job.callback(job.arg,job.candidate,PEER_VERIFY_REJECTED_DUPLICATE);
							}
						}
					} else {
						TRACE("%s accepted as new",job.candidate->identity().address().toString().c_str());
						_reallyAddPeer(job.candidate);
						if (job.callback)
							job.callback(job.arg,job.candidate,PEER_VERIFY_ACCEPTED_NEW);
					}
				} else {
					TRACE("%s rejected, identity failed initial checks",job.candidate->identity().address().toString().c_str());
					if (job.callback)
						job.callback(job.arg,job.candidate,PEER_VERIFY_REJECTED_INVALID_IDENTITY);
				}
				break;
			case _PeerDeepVerifyJob::CLEAN_CACHE:
				TRACE("cleaning caches and flushing modified peers to disk...");
				{
					Mutex::Lock _l(_activePeers_m);
					for(std::map< Address,SharedPtr<Peer> >::iterator p(_activePeers.begin());p!=_activePeers.end();++p) {
						if (p->second->getAndResetDirty()) {
							try {
								Buffer<ZT_PEER_MAX_SERIALIZED_LENGTH> b;
								p->second->serialize(b);
								b.zeroUnused();
								_dbm_m.lock();
								if (KISSDB_put(&_dbm,p->second->identity().address().data(),b.data())) {
									TRACE("error writing %s to peer.db",p->second->identity().address().toString().c_str());
								}
								_dbm_m.unlock();
							} catch ( ... ) {
								TRACE("unexpected exception flushing %s to peer.db",p->second->identity().address().toString().c_str());
							}
						}
					}
				}
				{
					Mutex::Lock _l(_multicastGroupMembers_m);
					for(std::map< uint64_t,std::map< MulticastGroup,std::map< Address,uint64_t > > >::iterator mgm(_multicastGroupMembers.begin());mgm!=_multicastGroupMembers.end();) {
						if (_r->nc->hasNetwork(mgm->first))
							++mgm;
						else _multicastGroupMembers.erase(mgm++);
					}
				}
				break;
			case _PeerDeepVerifyJob::EXIT_THREAD:
				TRACE("thread terminating...");
				return;
		}
	}
}

void Topology::_reallyAddPeer(const SharedPtr<Peer> &p)
{
	{
		Mutex::Lock _l(_activePeers_m);
		_activePeers[p->identity().address()] = p;
	}
	try {
		Buffer<ZT_PEER_MAX_SERIALIZED_LENGTH> b;
		p->serialize(b);
		b.zeroUnused();
		_dbm_m.lock();
		if (KISSDB_put(&_dbm,p->identity().address().data(),b.data())) {
			TRACE("error writing %s to peerdb",p->address().toString().c_str());
		} else p->getAndResetDirty();
		_dbm_m.unlock();
	} catch ( ... ) {
		TRACE("unexpected exception flushing to peerdb");
	}
}

} // namespace ZeroTier
