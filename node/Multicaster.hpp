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

#ifndef _ZT_MULTICASTER_HPP
#define _ZT_MULTICASTER_HPP

#include <stdint.h>
#include <string.h>

#include <utility>
#include <algorithm>
#include <stdexcept>
#include <map>
#include <set>
#include <vector>
#include <string>

#include "Constants.hpp"
#include "Buffer.hpp"
#include "Packet.hpp"
#include "MulticastGroup.hpp"
#include "Utils.hpp"
#include "MAC.hpp"
#include "Address.hpp"
#include "SharedPtr.hpp"
#include "BloomFilter.hpp"
#include "Identity.hpp"
#include "CMWC4096.hpp"
#include "C25519.hpp"

namespace ZeroTier {

/**
 * Multicast propagation engine
 *
 * This is written as a generic class so that it can be mocked and tested
 * in simulation. It also always takes 'now' as an argument, permitting
 * running in simulated time.
 *
 * This does not handle network permission or rate limiting, only the
 * propagation algorithm.
 */
class Multicaster
{
public:
	/**
	 * Simple bit field bloom filter included with multicast frame packets
	 */
	typedef BloomFilter<ZT_PROTO_VERB_MULTICAST_FRAME_BLOOM_FILTER_SIZE_BITS> MulticastBloomFilter;

	Multicaster()
		throw()
	{
		memset(_multicastHistory,0,sizeof(_multicastHistory));
		_multicastHistoryPtr = 0;
	}

	/**
	 * Generate a signature of a multicast packet using an identity
	 *
	 * @param id Identity to sign with (must have secret key portion)
	 * @param nwid Network ID
	 * @param from MAC address of sender
	 * @param to Multicast group
	 * @param etherType 16-bit ethernet type
	 * @param data Ethernet frame data
	 * @param len Length of frame
	 * @return Signature of packet data and attributes
	 * @throws std::runtime_error Cannot sign, e.g. identity has no private key
	 */
	static inline C25519::Signature signMulticastPacket(const Identity &id,uint64_t nwid,const MAC &from,const MulticastGroup &to,unsigned int etherType,const void *data,unsigned int len)
		throw(std::runtime_error)
	{
		char tmp[65536];
		void *tmp2 = (void *)tmp;
		*((uint64_t *)tmp2) = Utils::hton((uint64_t)nwid);
		memcpy(tmp + 8,from.data,6);
		memcpy(tmp + 14,to.mac().data,6);
		*((uint32_t *)(tmp + 20)) = Utils::hton((uint32_t)to.adi());
		*((uint16_t *)(tmp + 24)) = Utils::hton((uint16_t)etherType);
		memcpy(tmp + 26,data,std::min((unsigned int)(sizeof(tmp) - 26),len)); // min() is a sanity check here, no packet is that big
		return id.sign(tmp,len + 26);
	}

	/**
	 * Verify a signature from a multicast packet
	 *
	 * @param id Identity of original signer
	 * @param nwid Network ID
	 * @param from MAC address of sender
	 * @param to Multicast group
	 * @param etherType 16-bit ethernet type
	 * @param data Ethernet frame data
	 * @param len Length of frame
	 * @param signature Signature
	 * @param siglen Length of signature in bytes
	 * @return True if signature verification was successful
	 */
	static bool verifyMulticastPacket(const Identity &id,uint64_t nwid,const MAC &from,const MulticastGroup &to,unsigned int etherType,const void *data,unsigned int len,const void *signature,unsigned int siglen)
	{
		char tmp[65536];
		void *tmp2 = (void *)tmp;
		*((uint64_t *)tmp2) = Utils::hton(nwid);
		memcpy(tmp + 8,from.data,6);
		memcpy(tmp + 14,to.mac().data,6);
		*((uint32_t *)(tmp + 20)) = Utils::hton(to.adi());
		*((uint16_t *)(tmp + 24)) = Utils::hton((uint16_t)etherType);
		memcpy(tmp + 26,data,std::min((unsigned int)(sizeof(tmp) - 26),len)); // min() is a sanity check here, no packet is that big
		return id.verify(tmp,len + 26,signature,siglen);
	}

	/**
	 * Compute the CRC64 code for multicast deduplication
	 *
	 * @param nwid Network ID
	 * @param from Sender MAC
	 * @param to Destination multicast group
	 * @param etherType Ethernet frame type
	 * @param payload Multicast frame data
	 * @param len Length of frame
	 */
	static inline uint64_t computeMulticastDedupCrc(
		uint64_t nwid,
		const MAC &from,
		const MulticastGroup &to,
		unsigned int etherType,
		const void *payload,
		unsigned int len)
		throw()
	{
		// This CRC is only used locally, so byte order issues and
		// such don't matter. It can also be changed without protocol
		// impact.
		uint64_t crc = Utils::crc64(0,from.data,6);
		crc = Utils::crc64(crc,to.mac().data,6);
		crc ^= (uint64_t)to.adi();
		crc ^= (uint64_t)etherType;
		crc = Utils::crc64(crc,payload,len);
		crc ^= nwid; // also include network ID in CRC
		return crc;
	}

	/**
	 * Check multicast history to see if this is a duplicate
	 *
	 * @param crc Multicast CRC
	 * @param now Current time
	 * @return True if this appears to be a duplicate to within history expiration time
	 */
	inline bool checkDuplicate(uint64_t crc,uint64_t now) const
		throw()
	{
		for(unsigned int i=0;i<ZT_MULTICAST_DEDUP_HISTORY_LENGTH;++i) {
			if ((_multicastHistory[i][0] == crc)&&((now - _multicastHistory[i][1]) <= ZT_MULTICAST_DEDUP_HISTORY_EXPIRE))
				return true;
		}
		return false;
	}

	/**
	 * Add a multicast CRC to the multicast deduplication history
	 *
	 * @param crc Multicast CRC
	 * @param now Current time
	 */
	inline void addToDedupHistory(uint64_t crc,uint64_t now)
		throw()
	{
		unsigned int mhi = ++_multicastHistoryPtr % ZT_MULTICAST_DEDUP_HISTORY_LENGTH;
		_multicastHistory[mhi][0] = crc;
		_multicastHistory[mhi][1] = now;
	}

	/**
	 * Update the most recent LIKE time for an address in a given multicast group on a given network
	 *
	 * @param nwid Network ID
	 * @param mg Multicast group
	 * @param addr Address that likes group on given network
	 * @param now Current timestamp
	 */
	inline void likesMulticastGroup(const uint64_t nwid,const MulticastGroup &mg,const Address &addr,const uint64_t now)
	{
		Mutex::Lock _l(_multicastMemberships_m);
		std::vector<MulticastMembership> &memberships = _multicastMemberships[MulticastChannel(nwid,mg)];
		for(std::vector<MulticastMembership>::iterator mm(memberships.begin());mm!=memberships.end();++mm) {
			if (mm->first == addr) {
				mm->second = now;
				return;
			}
		}
		memberships.push_back(MulticastMembership(addr,now));
	}

	/**
	 * Choose peers for multicast propagation via random selection
	 *
	 * @param prng Random source
	 * @param topology Topology object or mock thereof
	 * @param nwid Network ID
	 * @param mg Multicast group
	 * @param originalSubmitter Original submitter of multicast message to network
	 * @param upstream Address from which message originated, or null (0) address if none
	 * @param bf Bloom filter, updated in place with sums of addresses in chosen peers and/or decay
	 * @param max Maximum number of peers to pick
	 * @param peers Array of objects of type P to fill with up to [max] peers
	 * @param now Current timestamp
	 * @return Number of peers actually stored in peers array
	 * @tparam T Type of topology, which is Topology in running code or a mock in simulation
	 * @tparam P Type of peers, which is SharedPtr<Peer> in running code or a mock in simulation (mock must behave like a pointer type)
	 */
	template<typename T,typename P>
	inline unsigned int pickRandomPropagationPeers(
		CMWC4096 &prng,
		T &topology,
		uint64_t nwid,
		const MulticastGroup &mg,
		const Address &originalSubmitter,
		const Address &upstream,
		MulticastBloomFilter &bf,
		unsigned int max,
		P *peers,
		uint64_t now)
	{
		unsigned int chosen = 0;
		Mutex::Lock _l(_multicastMemberships_m);
		std::map< MulticastChannel,std::vector<MulticastMembership> >::iterator mm(_multicastMemberships.find(MulticastChannel(nwid,mg)));
		if ((mm != _multicastMemberships.end())&&(!mm->second.empty())) {
			for(unsigned int stries=0,stmax=(max*10);((stries<stmax)&&(chosen < max));++stries) {
				MulticastMembership &m = mm->second[prng.next32() % mm->second.size()];
				unsigned int sum = m.first.sum();
				if (
				     ((now - m.second) < ZT_MULTICAST_LIKE_EXPIRE)&& /* LIKE is not expired */
				     (!bf.contains(sum))&&                           /* Not in propagation bloom */
				     (m.first != originalSubmitter)&&                /* Not the original submitter */
				     (m.first != upstream) ) {                       /* Not where the frame came from */
					P peer(topology.getPeer(m.first));
					if (peer) {
						unsigned int chk = 0;
						while (chk < chosen) {
							if (peers[chk] == peer)
								break;
							++chk;
						}
						if (chk == chosen) { /* not already picked */
							peers[chosen++] = peer;
							bf.set(sum);
						}
					}
				}
			}
		}
		return chosen;
	}

	/**
	 * Choose peers for multicast propagation via implicit social switching
	 *
	 * @param prng Random source
	 * @param topology Topology object or mock thereof
	 * @param nwid Network ID
	 * @param mg Multicast group
	 * @param originalSubmitter Original submitter of multicast message to network
	 * @param upstream Address from which message originated, or null (0) address if none
	 * @param bf Bloom filter, updated in place with sums of addresses in chosen peers and/or decay
	 * @param max Maximum number of peers to pick
	 * @param peers Array of objects of type P to fill with up to [max] peers
	 * @param now Current timestamp
	 * @return Number of peers actually stored in peers array
	 * @tparam T Type of topology, which is Topology in running code or a mock in simulation
	 * @tparam P Type of peers, which is SharedPtr<Peer> in running code or a mock in simulation (mock must behave like a pointer type)
	 */
	template<typename T,typename P>
	inline unsigned int pickSocialPropagationPeers(
		CMWC4096 &prng,
		T &topology,
		uint64_t nwid,
		const MulticastGroup &mg,
		const Address &originalSubmitter,
		const Address &upstream,
		MulticastBloomFilter &bf,
		unsigned int max,
		P *peers,
		uint64_t now)
	{
		typename std::set< P,_PeerPropagationPrioritySortOrder<P> > toConsider;

		/* Pick up to ZT_MULTICAST_PICK_MAX_SAMPLE_SIZE peers that meet
		 * our minimal criteria for this multicast group and place them
		 * into a set that is sorted in descending order of time of most
		 * recent unicast frame transfer (implicit social ordering). */
		{
			Mutex::Lock _l(_multicastMemberships_m);
			std::map< MulticastChannel,std::vector<MulticastMembership> >::iterator mm(_multicastMemberships.find(MulticastChannel(nwid,mg)));
			if ((mm != _multicastMemberships.end())&&(!mm->second.empty())) {
				for(unsigned int stries=0,stmax=(max*10);stries<stmax;++stries) {
					MulticastMembership &m = mm->second[prng.next32() % mm->second.size()];
					if (
					     ((now - m.second) < ZT_MULTICAST_LIKE_EXPIRE)&& /* LIKE is not expired */
					     (!bf.contains(m.first.sum()))&&                 /* Not in propagation bloom */
					     (m.first != originalSubmitter)&&                /* Not the original submitter */
					     (m.first != upstream) ) {                       /* Not where the frame came from */
						P peer(topology.getPeer(m.first));
						if (peer)
							toConsider.insert(peer); /* Consider propagating to this peer */
					}
				}
			}
		}

		/* The first peers in toConsider will be the "best" */
		unsigned int chosen = 0;
		for(typename std::set< P,_PeerPropagationPrioritySortOrder<P> >::iterator i(toConsider.begin());((i!=toConsider.end())&&(chosen < max));++i)
			bf.set((peers[chosen++] = *i)->address().sum());

		/* Tack on a supernode if we have no next hops */
		if (!chosen) {
			Address exclude[1];
			exclude[0] = originalSubmitter; // if it came from a supernode, don't boomerang
			P peer = topology.getBestSupernode(exclude,1,true);
			if (peer)
				peers[chosen++] = peer;
		}

		return chosen;
	}

private:
	// Sort order for chosen propagation peers
	template<typename P>
	struct _PeerPropagationPrioritySortOrder
	{
		inline bool operator()(const P &p1,const P &p2) const
		{
			return (p1->lastUnicastFrame() > p2->lastUnicastFrame());
		}
	};

	// ring buffer: [0] - CRC, [1] - timestamp
	uint64_t _multicastHistory[ZT_MULTICAST_DEDUP_HISTORY_LENGTH][2];
	volatile unsigned int _multicastHistoryPtr;

	// A multicast channel, essentially a pub/sub channel. It consists of a
	// network ID and a multicast group within that network.
	typedef std::pair<uint64_t,MulticastGroup> MulticastChannel;

	// A membership in a multicast channel, an address and time of last LIKE
	typedef std::pair<Address,uint64_t> MulticastMembership;

	// Network : MulticastGroup -> vector<Address : time of last LIKE>
	std::map< MulticastChannel,std::vector<MulticastMembership> > _multicastMemberships;
	Mutex _multicastMemberships_m;
};

} // namespace ZeroTier

#endif
