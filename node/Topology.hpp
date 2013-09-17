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

#ifndef _ZT_TOPOLOGY_HPP
#define _ZT_TOPOLOGY_HPP

#include <stdio.h>
#include <string.h>

#include <map>
#include <set>
#include <list>
#include <vector>
#include <stdexcept>

#include "Address.hpp"
#include "Peer.hpp"
#include "Mutex.hpp"
#include "Condition.hpp"
#include "InetAddress.hpp"
#include "Constants.hpp"
#include "Thread.hpp"
#include "MulticastGroup.hpp"
#include "Utils.hpp"

#include "../ext/kissdb/kissdb.h"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Database of network topology
 */
class Topology
{
public:
	/**
	 * Result of peer add/verify
	 */
	enum PeerVerifyResult
	{
		PEER_VERIFY_ACCEPTED_NEW,                       /* new peer */
		PEER_VERIFY_ACCEPTED_ALREADY_HAVE,              /* we already knew ye */
		PEER_VERIFY_ACCEPTED_DISPLACED_INVALID_ADDRESS, /* you booted out an impostor */
		PEER_VERIFY_REJECTED_INVALID_IDENTITY,          /* identity is invalid or validation failed */
		PEER_VERIFY_REJECTED_DUPLICATE,                 /* someone equally valid already has your address */
		PEER_VERIFY_REJECTED_DUPLICATE_TRIAGED          /* you look duplicate and I'm too busy to deep verify */
	};

	Topology(const RuntimeEnvironment *renv,const char *dbpath)
		throw(std::runtime_error);

	~Topology();

	/**
	 * Set up supernodes for this network
	 * 
	 * @param sn Supernodes for this network
	 */
	void setSupernodes(const std::map< Identity,std::vector<InetAddress> > &sn);

	/**
	 * Add a peer to this network
	 * 
	 * Verification and adding actually occurs in the background, since in
	 * rare cases it can be somewhat CPU-intensive. The callback will be
	 * called (from the background thread) when add is complete.
	 * 
	 * The peer given to the callback may not be the same object provided
	 * as a candidate if the candidate was an exact duplicate of a peer we
	 * already have.
	 *
	 * @param candidate New candidate peer to be added
	 * @param callback Callback to call when peer verification is complete
	 * @param arg First argument to callback
	 * @return Verification result or PEER_VERIFY__IN_PROGRESS if occurring in background
	 */
	void addPeer(const SharedPtr<Peer> &candidate,void (*callback)(void *,const SharedPtr<Peer> &,PeerVerifyResult),void *arg);

	/**
	 * Get a peer from its address
	 * 
	 * @param zta ZeroTier address of peer
	 * @return Peer or NULL if not found
	 */
	SharedPtr<Peer> getPeer(const Address &zta);

	/**
	 * @return Current network supernodes
	 */
	inline std::map< Identity,std::vector<InetAddress> > supernodes() const
	{
		Mutex::Lock _l(_supernodes_m);
		return _supernodes;
	}

	/**
	 * @return Vector of peers that are supernodes
	 */
	inline std::vector< SharedPtr<Peer> > supernodePeers() const
	{
		Mutex::Lock _l(_supernodes_m);
		return _supernodePeers;
	}

	/**
	 * Get the current favorite supernode
	 * 
	 * @return Supernode with lowest latency or NULL if none
	 */
	inline SharedPtr<Peer> getBestSupernode() const
	{
		return getBestSupernode((const Address *)0,0,false);
	}

	/**
	 * Get the best supernode, avoiding supernodes listed in an array
	 * 
	 * This will get the best supernode (lowest latency, etc.) but will
	 * try to avoid the listed supernodes, only using them if no others
	 * are available.
	 * 
	 * @param avoid Nodes to avoid
	 * @param avoidCount Number of nodes to avoid
	 * @param strictAvoid If false, consider avoided supernodes anyway if no non-avoid supernodes are available
	 * @return Supernode or NULL if none
	 */
	SharedPtr<Peer> getBestSupernode(const Address *avoid,unsigned int avoidCount,bool strictAvoid) const;

	/**
	 * @param zta ZeroTier address
	 * @return True if this is a designated supernode
	 */
	inline bool isSupernode(const Address &zta) const
		throw()
	{
		Mutex::Lock _l(_supernodes_m);
		return (_supernodeAddresses.count(zta) > 0);
	}

	/**
	 * @return True if this node's identity is in the supernode set
	 */
	inline bool amSupernode() const { return _amSupernode; }

	/**
	 * Clean and flush database now (runs in the background)
	 */
	void clean();

	/**
	 * Apply a function or function object to all peers
	 *
	 * @param f Function to apply
	 * @tparam F Function or function object type
	 */
	template<typename F>
	inline void eachPeer(F f)
	{
		Mutex::Lock _l(_activePeers_m);
		for(std::map< Address,SharedPtr<Peer> >::const_iterator p(_activePeers.begin());p!=_activePeers.end();++p)
			f(*this,p->second);
	}

	/**
	 * Function object to collect peers that need a firewall opener sent
	 */
	class CollectPeersThatNeedFirewallOpener
	{
	public:
		CollectPeersThatNeedFirewallOpener(std::vector< SharedPtr<Peer> > &v) :
			_now(Utils::now()),
			_v(v)
		{
		}

		inline void operator()(Topology &t,const SharedPtr<Peer> &p)
		{
			if ((p->hasDirectPath())&&((_now - std::max(p->lastFirewallOpener(),p->lastDirectSend())) >= ZT_FIREWALL_OPENER_DELAY))
				_v.push_back(p);
		}

	private:
		uint64_t _now;
		std::vector< SharedPtr<Peer> > &_v;
	};

	/**
	 * Function object to collect peers that need a ping sent
	 */
	class CollectPeersThatNeedPing
	{
	public:
		CollectPeersThatNeedPing(std::vector< SharedPtr<Peer> > &v) :
			_now(Utils::now()),
			_v(v)
		{
		}

		inline void operator()(Topology &t,const SharedPtr<Peer> &p)
		{
			if ( ((t.isSupernode(p->address()))&&((_now - p->lastDirectReceive()) >= ZT_PEER_DIRECT_PING_DELAY)) || ((p->hasActiveDirectPath(_now))&&((_now - p->lastDirectSend()) >= ZT_PEER_DIRECT_PING_DELAY)) )
				_v.push_back(p);
		}

	private:
		uint64_t _now;
		std::vector< SharedPtr<Peer> > &_v;
	};

	/**
	 * Function object to collect peers with active links (and supernodes)
	 */
	class CollectPeersWithActiveDirectPath
	{
	public:
		CollectPeersWithActiveDirectPath(std::vector< SharedPtr<Peer> > &v) :
			_now(Utils::now()),
			_v(v)
		{
		}

		inline void operator()(Topology &t,const SharedPtr<Peer> &p)
		{
			if ((p->hasActiveDirectPath(_now))||(t.isSupernode(p->address())))
				_v.push_back(p);
		}

	private:
		uint64_t _now;
		std::vector< SharedPtr<Peer> > &_v;
	};

	/**
	 * Function object to collect peers with any known direct path
	 */
	class CollectPeersWithDirectPath
	{
	public:
		CollectPeersWithDirectPath(std::vector< SharedPtr<Peer> > &v) :
			_v(v)
		{
		}

		inline void operator()(Topology &t,const SharedPtr<Peer> &p)
		{
			if (p->hasDirectPath())
				_v.push_back(p);
		}

	private:
		std::vector< SharedPtr<Peer> > &_v;
	};

	/**
	 * Thread main method; do not call elsewhere
	 */
	void threadMain()
		throw();

private:
	void _reallyAddPeer(const SharedPtr<Peer> &p);

	// A job for the background deep verify thread (also does cache cleaning, flushing, etc.)
	struct _PeerDeepVerifyJob
	{
		void (*callback)(void *,const SharedPtr<Peer> &,Topology::PeerVerifyResult);
		void *arg;
		SharedPtr<Peer> candidate;
		enum {
			VERIFY_PEER,
			CLEAN_CACHE,
			EXIT_THREAD
		} type;
	};

	const RuntimeEnvironment *const _r;
	Thread _thread;

	std::map< Address,SharedPtr<Peer> > _activePeers;
	Mutex _activePeers_m;

	std::list< _PeerDeepVerifyJob > _peerDeepVerifyJobs;
	Mutex _peerDeepVerifyJobs_m;
	Condition _peerDeepVerifyJobs_c;

	std::map< Identity,std::vector<InetAddress> > _supernodes;
	std::set< Address > _supernodeAddresses;
	std::vector< SharedPtr<Peer> > _supernodePeers;
	Mutex _supernodes_m;

	// Set to true if my identity is in _supernodes
	volatile bool _amSupernode;

	KISSDB _dbm;
	Mutex _dbm_m;
};

} // namespace ZeroTier

#endif
