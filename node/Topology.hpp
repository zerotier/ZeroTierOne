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
class Topology : protected Thread
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

	virtual ~Topology();

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
		return getBestSupernode((const Address *)0,0);
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
	 * @return Supernode or NULL if none
	 */
	SharedPtr<Peer> getBestSupernode(const Address *avoid,unsigned int avoidCount) const;

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
	 * Clean and flush database now (runs in the background)
	 */
	void clean();

	/**
	 * Pick peers for multicast propagation
	 *
	 * @param nwid Network ID
	 * @param exclude Peer to exclude or zero address for none
	 * @param propagationBloom Propgation bloom filter
	 * @param propagationBloomSize Size of propagation bloom filter in BITS
	 * @param count Number of peers desired (propagation breadth)
	 * @param mg Multicast group
	 * @param peers Array to receive peers (must be at least [count])
	 * @return Number of peers actually picked
	 */
	unsigned int pickMulticastPropagationPeers(uint64_t nwid,const Address &exclude,const void *propagationBloom,unsigned int propagationBloomSize,unsigned int count,const MulticastGroup &mg,SharedPtr<Peer> *peers);

	/**
	 * Add or update last 'like' time for an address's membership in a multicast group
	 *
	 * @param nwid Network ID
	 * @param mg Multicast group
	 * @param addr ZeroTier address
	 * @param now Current time
	 */
	void likesMulticastGroup(uint64_t nwid,const MulticastGroup &mg,const Address &addr,uint64_t now);

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
			if ((p->hasDirectPath())&&((_now - p->lastFirewallOpener()) >= ZT_FIREWALL_OPENER_DELAY))
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
			if (((p->hasActiveDirectPath(_now))||(t.isSupernode(p->address())))&&((_now - p->lastDirectSend()) >= ZT_PEER_DIRECT_PING_DELAY))
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

protected:
	virtual void main()
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

	std::map< Address,SharedPtr<Peer> > _activePeers;
	Mutex _activePeers_m;

	std::list< _PeerDeepVerifyJob > _peerDeepVerifyJobs;
	Mutex _peerDeepVerifyJobs_m;
	Condition _peerDeepVerifyJobs_c;

	std::map< Identity,std::vector<InetAddress> > _supernodes;
	std::set< Address > _supernodeAddresses;
	std::vector< SharedPtr<Peer> > _supernodePeers;
	Mutex _supernodes_m;

	KISSDB _dbm;
	Mutex _dbm_m;

	// Multicast group members by network ID, then multicast group
	std::map< uint64_t,std::map< MulticastGroup,std::map< Address,uint64_t > > > _multicastGroupMembers;
	Mutex _multicastGroupMembers_m;
};

} // namespace ZeroTier

#endif
