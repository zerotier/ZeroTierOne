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

#define ZT_KISSDB_HASH_TABLE_SIZE 131072
#define ZT_KISSDB_KEY_SIZE ZT_ADDRESS_LENGTH
#define ZT_KISSDB_VALUE_SIZE ZT_PEER_MAX_SERIALIZED_LENGTH

Topology::Topology(const RuntimeEnvironment *renv,const char *dbpath)
	throw(std::runtime_error) :
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

	_thread = Thread::start(this);
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
	Thread::join(_thread);
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

	_amSupernode = (_supernodes.find(_r->identity) != _supernodes.end());
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
	{
		Mutex::Lock _l(_peerDeepVerifyJobs_m);
		_peerDeepVerifyJobs.push_back(_PeerDeepVerifyJob());
		_peerDeepVerifyJobs.back().type = _PeerDeepVerifyJob::CLEAN_CACHE;
	}
	_peerDeepVerifyJobs_c.signal();
}

void Topology::threadMain()
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
		uint64_t atmp[ZT_ADDRESS_LENGTH];
		p->address().copyTo(atmp,ZT_ADDRESS_LENGTH);
		Buffer<ZT_PEER_MAX_SERIALIZED_LENGTH> b;
		p->serialize(b);
		b.zeroUnused();
		_dbm_m.lock();
		if (KISSDB_put(&_dbm,atmp,b.data())) {
			TRACE("error writing %s to peerdb",p->address().toString().c_str());
		} else p->getAndResetDirty();
		_dbm_m.unlock();
	} catch ( ... ) {
		TRACE("unexpected exception flushing to peerdb");
	}
}

} // namespace ZeroTier
