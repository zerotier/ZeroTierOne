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

#ifndef ZT_TOPOLOGY_HPP
#define ZT_TOPOLOGY_HPP

#include <stdio.h>
#include <string.h>

#include <map>
#include <vector>
#include <stdexcept>
#include <algorithm>

#include "Constants.hpp"

#include "Address.hpp"
#include "Identity.hpp"
#include "Peer.hpp"
#include "Mutex.hpp"
#include "InetAddress.hpp"
#include "Utils.hpp"
#include "Packet.hpp"
#include "Logger.hpp"
#include "Dictionary.hpp"
#include "ExternalSurface.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Database of network topology
 */
class Topology
{
public:
	Topology(const RuntimeEnvironment *renv);
	~Topology();

	/**
	 * Set up supernodes for this network
	 * 
	 * @param sn Supernodes for this network
	 */
	void setSupernodes(const std::map< Identity,std::vector< std::pair<InetAddress,bool> > > &sn);

	/**
	 * Set up supernodes for this network
	 *
	 * This performs no signature verification of any kind. The caller must
	 * check the signature of the root topology dictionary first.
	 *
	 * @param sn Supernodes dictionary from root-topology
	 */
	void setSupernodes(const Dictionary &sn);

	/**
	 * Add a peer to database
	 *
	 * This will not replace existing peers. In that case the existing peer
	 * record is returned.
	 *
	 * @param peer Peer to add
	 * @return New or existing peer (should replace 'peer')
	 */
	SharedPtr<Peer> addPeer(const SharedPtr<Peer> &peer);

	/**
	 * Get a peer from its address
	 * 
	 * @param zta ZeroTier address of peer
	 * @return Peer or NULL if not found
	 */
	SharedPtr<Peer> getPeer(const Address &zta);

	/**
	 * @return Vector of peers that are supernodes
	 */
	inline std::vector< SharedPtr<Peer> > supernodePeers() const
	{
		Mutex::Lock _l(_lock);
		return _supernodePeers;
	}

	/**
	 * @return Number of supernodes
	 */
	inline unsigned int numSupernodes() const
	{
		Mutex::Lock _l(_lock);
		return (unsigned int)_supernodePeers.size();
	}

	/**
	 * Get the current favorite supernode
	 * 
	 * @return Supernode with lowest latency or NULL if none
	 */
	inline SharedPtr<Peer> getBestSupernode()
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
	SharedPtr<Peer> getBestSupernode(const Address *avoid,unsigned int avoidCount,bool strictAvoid);

	/**
	 * @param zta ZeroTier address
	 * @return True if this is a designated supernode
	 */
	inline bool isSupernode(const Address &zta) const
		throw()
	{
		Mutex::Lock _l(_lock);
		return (std::find(_supernodeAddresses.begin(),_supernodeAddresses.end(),zta) != _supernodeAddresses.end());
	}

	/**
	 * @return Vector of supernode addresses
	 */
	inline std::vector<Address> supernodeAddresses() const
	{
		Mutex::Lock _l(_lock);
		return _supernodeAddresses;
	}

	/**
	 * @return True if this node's identity is in the supernode set
	 */
	inline bool amSupernode() const { return _amSupernode; }

	/**
	 * Clean and flush database
	 */
	void clean(uint64_t now);

	/**
	 * Apply a function or function object to all peers
	 *
	 * Note: explicitly template this by reference if you want the object
	 * passed by reference instead of copied.
	 *
	 * Warning: be careful not to use features in these that call any other
	 * methods of Topology that may lock _lock, otherwise a recursive lock
	 * and deadlock or lock corruption may occur.
	 *
	 * @param f Function to apply
	 * @tparam F Function or function object type
	 */
	template<typename F>
	inline void eachPeer(F f)
	{
		Mutex::Lock _l(_lock);
		for(std::map< Address,SharedPtr<Peer> >::const_iterator p(_activePeers.begin());p!=_activePeers.end();++p)
			f(*this,p->second);
	}

	/**
	 * Apply a function or function object to all supernode peers
	 *
	 * Note: explicitly template this by reference if you want the object
	 * passed by reference instead of copied.
	 *
	 * Warning: be careful not to use features in these that call any other
	 * methods of Topology that may lock _lock, otherwise a recursive lock
	 * and deadlock or lock corruption may occur.
	 *
	 * @param f Function to apply
	 * @tparam F Function or function object type
	 */
	template<typename F>
	inline void eachSupernodePeer(F f)
	{
		Mutex::Lock _l(_lock);
		for(std::vector< SharedPtr<Peer> >::const_iterator p(_supernodePeers.begin());p!=_supernodePeers.end();++p)
			f(*this,*p);
	}

	/**
	 * Pings all peers that need a ping sent, excluding supernodes
	 *
	 * Ordinary peers are pinged if we haven't heard from them recently. Receive
	 * time rather than send time as OK is returned on success and we want to
	 * keep trying if a packet is lost. Ordinary peers are subject to a frame
	 * inactivity timeout. We give up if we haven't actually transferred any
	 * data to them recently, and eventually Topology purges them from memory.
	 */
	class PingPeersThatNeedPing
	{
	public:
		PingPeersThatNeedPing(const RuntimeEnvironment *renv,uint64_t now) throw() :
			_now(now),
			_supernodeAddresses(renv->topology->supernodeAddresses()),
			RR(renv) {}

		inline void operator()(Topology &t,const SharedPtr<Peer> &p)
		{
			/* For ordinary nodes we ping if they've sent us a frame recently,
			 * otherwise they are stale and we let the link die.
			 *
			 * Note that we measure ping time from time of last receive rather
			 * than time of last send in order to only count full round trips. */
			if ( (std::find(_supernodeAddresses.begin(),_supernodeAddresses.end(),p->address()) == _supernodeAddresses.end()) &&
			     ((_now - p->lastFrame()) < ZT_PEER_PATH_ACTIVITY_TIMEOUT) &&
			     ((_now - p->lastDirectReceive()) >= ZT_PEER_DIRECT_PING_DELAY) ) {
				p->sendPing(RR,_now);
			}
		}

	private:
		uint64_t _now;
		std::vector<Address> _supernodeAddresses;
		const RuntimeEnvironment *RR;
	};

	/**
	 * Ping peers that need ping according to supernode rules
	 *
	 * Supernodes ping aggressively if a ping is unanswered and they are not
	 * subject to the activity timeout. In other words: we assume they are
	 * always there and always try to reach them.
	 *
	 * The ultimate rate limit for this is controlled up in the Node main loop.
	 */
	class PingSupernodesThatNeedPing
	{
	public:
		PingSupernodesThatNeedPing(const RuntimeEnvironment *renv,uint64_t now) throw() :
			_now(now),
			RR(renv) {}

		inline void operator()(Topology &t,const SharedPtr<Peer> &p)
		{
			/* For supernodes we always ping even if no frames have been seen, and
			 * we ping aggressively if pings are unanswered. The limit to this
			 * frequency is set in the main loop to no more than ZT_STARTUP_AGGRO. */

			uint64_t lp = 0;
			uint64_t lr = 0;
			p->lastPingAndDirectReceive(lp,lr);
			if ( (lr < RR->timeOfLastResynchronize) || ((lr < lp)&&((lp - lr) >= ZT_PING_UNANSWERED_AFTER)) || ((_now - lr) >= ZT_PEER_DIRECT_PING_DELAY) )
				p->sendPing(RR,_now);
		}

	private:
		uint64_t _now;
		const RuntimeEnvironment *RR;
	};

	/**
	 * Computes most recent timestamp of direct packet receive over a list of peers
	 */
	class FindMostRecentDirectReceiveTimestamp
	{
	public:
		FindMostRecentDirectReceiveTimestamp(uint64_t &ts) throw() : _ts(ts) {}
		inline void operator()(Topology &t,const SharedPtr<Peer> &p) throw() { _ts = std::max(p->lastDirectReceive(),_ts); }
	private:
		uint64_t &_ts;
	};

	/**
	 * Function object to forget direct links to active peers and then ping them indirectly
	 */
	/*
	class ResetActivePeers
	{
	public:
		ResetActivePeers(const RuntimeEnvironment *renv,uint64_t now) throw() :
			_now(now),
			_supernode(renv->topology->getBestSupernode()),
			_supernodeAddresses(renv->topology->supernodeAddresses()),
			RR(renv) {}

		inline void operator()(Topology &t,const SharedPtr<Peer> &p)
		{
			p->clearPaths(false); // false means don't forget 'fixed' paths e.g. supernodes

			Packet outp(p->address(),RR->identity.address(),Packet::VERB_NOP);
			outp.armor(p->key(),false); // no need to encrypt a NOP

			if (std::find(_supernodeAddresses.begin(),_supernodeAddresses.end(),p->address()) != _supernodeAddresses.end()) {
				// Send NOP directly to supernodes
				p->send(RR,outp.data(),outp.size(),_now);
			} else {
				// Send NOP indirectly to regular peers if still active, triggering a new RENDEZVOUS
				if (((_now - p->lastFrame()) < ZT_PEER_PATH_ACTIVITY_TIMEOUT)&&(_supernode)) {
					TRACE("sending reset NOP to %s",p->address().toString().c_str());
					_supernode->send(RR,outp.data(),outp.size(),_now);
				}
			}
		}

	private:
		uint64_t _now;
		SharedPtr<Peer> _supernode;
		std::vector<Address> _supernodeAddresses;
		const RuntimeEnvironment *RR;
	};
	*/

	/**
	 * Function object to collect peers with any known direct path
	 */
	class CollectPeersWithActiveDirectPath
	{
	public:
		CollectPeersWithActiveDirectPath(std::vector< SharedPtr<Peer> > &v,uint64_t now) throw() :
			_now(now),
			_v(v) {}

		inline void operator()(Topology &t,const SharedPtr<Peer> &p)
		{
			if (p->hasActiveDirectPath(_now))
				_v.push_back(p);
		}

	private:
		uint64_t _now;
		std::vector< SharedPtr<Peer> > &_v;
	};

	/**
	 * Update our knowledge of exterior network addresses
	 *
	 * If the remote peer in question is trusted, this will update our internal
	 * instance of ExternalSurface. If our surface has changed, this triggers a
	 * partial or total reset of ephemeral peer addresses and a renegotiation of
	 * new ones using supernodes / relays.
	 *
	 * @param remotePeer Remote peer address
	 * @param mirroredAddress Real-world network address the remote peer told us we have
	 * @param now Current time
	 */
	bool updateSurface(const SharedPtr<Peer> &remotePeer,const InetAddress &mirroredAddress,uint64_t now);

	/**
	 * Validate a root topology dictionary against the identities specified in Defaults
	 *
	 * @param rt Root topology dictionary
	 * @return True if dictionary signature is valid
	 */
	static bool authenticateRootTopology(const Dictionary &rt);

private:
	Identity _getIdentity(const Address &zta);
	void _saveIdentity(const Identity &id);

	const RuntimeEnvironment *RR;

	std::string _idCacheBase;

	std::map< Address,SharedPtr<Peer> > _activePeers;
	std::map< Identity,std::vector< std::pair<InetAddress,bool> > > _supernodes;
	std::vector< Address > _supernodeAddresses;
	std::vector< SharedPtr<Peer> > _supernodePeers;

	ExternalSurface _surface;

	Mutex _lock;

	// Set to true if my identity is in _supernodes
	volatile bool _amSupernode;
};

} // namespace ZeroTier

#endif
