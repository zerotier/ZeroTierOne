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

#include <openssl/sha.h>

#include <utility>
#include <algorithm>
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

// Maximum sample size to pick during choice of multicast propagation peers
#define ZT_MULTICAST_PICK_MAX_SAMPLE_SIZE 64

namespace ZeroTier {

/**
 * Multicast propagation engine
 *
 * This is written as a generic class so that it can be mocked and tested
 * in simulation. It also always takes 'now' as an argument, permitting
 * running in simulated time.
 */
class Multicaster
{
public:
	/**
	 * 256-bit simple bloom filter included with multicast frame packets
	 */
	typedef BloomFilter<ZT_PROTO_VERB_MULTICAST_FRAME_BLOOM_FILTER_SIZE_BITS> MulticastBloomFilter;

	Multicaster()
		throw()
	{
		memset(_multicastHistory,0,sizeof(_multicastHistory));
	}

	/**
	 * Generate a signature of a multicast packet using an identity
	 *
	 * @param id Identity to sign with (must have secret key portion)
	 * @param from MAC address of sender
	 * @param to Multicast group
	 * @param etherType 16-bit ethernet type
	 * @param data Ethernet frame data
	 * @param len Length of frame
	 * @return ECDSA signature
	 */
	static inline std::string signMulticastPacket(const Identity &id,const MAC &from,const MulticastGroup &to,unsigned int etherType,const void *data,unsigned int len)
	{
		unsigned char digest[32];
		_hashMulticastPacketForSig(from,to,etherType,data,len,digest);
		return id.sign(digest);
	}

	/**
	 * Verify a signature from a multicast packet
	 *
	 * @param id Identity of original signer
	 * @param from MAC address of sender
	 * @param to Multicast group
	 * @param etherType 16-bit ethernet type
	 * @param data Ethernet frame data
	 * @param len Length of frame
	 * @param signature ECDSA signature
	 * @param siglen Length of signature in bytes
	 * @return ECDSA signature
	 */
	static bool verifyMulticastPacket(const Identity &id,const MAC &from,const MulticastGroup &to,unsigned int etherType,const void *data,unsigned int len,const void *signature,unsigned int siglen)
	{
		unsigned char digest[32];
		_hashMulticastPacketForSig(from,to,etherType,data,len,digest);
		return id.verify(digest,signature,siglen);
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
		_multicastMemberships[MulticastChannel(nwid,mg)][addr] = now;
	}

	/**
	 * Check multicast history to see if this is a duplicate, and add/update entry
	 *
	 * @param from Ultimate sending MAC address
	 * @param to Destination multicast group
	 * @param payload Multicast packet payload
	 * @param len Length of packet
	 * @param nwid Network ID
	 * @param now Current time
	 * @return True if this appears to be a duplicate to within history expiration time
	 */
	inline bool checkAndUpdateMulticastHistory(
		const MAC &from,
		const MulticastGroup &to,
		const void *payload,
		unsigned int len,
		const uint64_t nwid,
		const uint64_t now)
		throw()
	{
		// Note: CRCs aren't transmitted over the network, so portability and
		// byte order don't matter. This calculation can be changed. We just
		// want a unique code.
		uint64_t crc = Utils::crc64(0,from.data,6);
		crc = Utils::crc64(crc,to.mac().data,6);
		crc ^= (uint64_t)to.adi();
		crc = Utils::crc64(crc,payload,len);
		crc ^= nwid; // also include network ID in CRC

		// Replace existing entry or pick one to replace with new entry
		uint64_t earliest = 0xffffffffffffffffULL;
		unsigned long earliestIdx = 0;
		for(unsigned int i=0;i<ZT_MULTICAST_DEDUP_HISTORY_LENGTH;++i) {
			if (_multicastHistory[i][0] == crc) {
				uint64_t then = _multicastHistory[i][1];
				_multicastHistory[i][1] = now;
				return ((now - then) < ZT_MULTICAST_DEDUP_HISTORY_EXPIRE);
			} else if (_multicastHistory[i][1] < earliest) {
				earliest = _multicastHistory[i][1];
				earliestIdx = i;
			}
		}

		_multicastHistory[earliestIdx][0] = crc; // replace oldest entry
		_multicastHistory[earliestIdx][1] = now;

		return false;
	}

	/**
	 * Choose peers to send a propagating multicast to
	 *
	 * @param topology Topology object or mock thereof
	 * @param nwid Network ID
	 * @param mg Multicast group
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
	inline unsigned int pickNextPropagationPeers(
		T &topology,
		uint64_t nwid,
		const MulticastGroup &mg,
		const Address &upstream,
		MulticastBloomFilter &bf,
		unsigned int max,
		P *peers,
		uint64_t now)
	{
		P toConsider[ZT_MULTICAST_PICK_MAX_SAMPLE_SIZE];
		unsigned int sampleSize = 0;

		{
			Mutex::Lock _l(_multicastMemberships_m);

			// Sample a random subset of peers that we know have LIKEd this multicast
			// group on this network.
			std::map< MulticastChannel,std::map<Address,uint64_t> >::iterator channelMembers(_multicastMemberships.find(MulticastChannel(nwid,mg)));
			if ((channelMembers != _multicastMemberships.end())&&(!channelMembers->second.empty())) {
				unsigned long numEntriesPermittedToSkip = (channelMembers->second.size() > ZT_MULTICAST_PICK_MAX_SAMPLE_SIZE) ? (unsigned long)(channelMembers->second.size() - ZT_MULTICAST_PICK_MAX_SAMPLE_SIZE) : (unsigned long)0;
				double skipWhatFraction = (double)numEntriesPermittedToSkip / (double)channelMembers->second.size();

				std::map<Address,uint64_t>::iterator channelMemberEntry(channelMembers->second.begin());

				while (channelMemberEntry != channelMembers->second.end()) {
					// Auto-clean the channel members map if their LIKEs are expired. This will
					// technically skew the random distribution of chosen members just a little, but
					// it's unlikely that enough will expire in any single pick to make much of a
					// difference overall.
					if ((now - channelMemberEntry->second) > ZT_MULTICAST_LIKE_EXPIRE) {
						channelMembers->second.erase(channelMemberEntry++);
						continue;
					}

					// Skip some fraction of entries so that our sampling will be randomly distributed,
					// since there is no other good way to sample randomly from a map.
					if (numEntriesPermittedToSkip) {
						double skipThis = (double)(Utils::randomInt<uint32_t>()) / 4294967296.0;
						if (skipThis <= skipWhatFraction) {
							--numEntriesPermittedToSkip;
							++channelMemberEntry;
							continue;
						}
					}

					// If it's not expired and it's from our random sample, add it to the set of peers
					// to consider.
					P peer = topology.getPeer(channelMemberEntry->first);
					if (peer) {
						toConsider[sampleSize++] = peer;
						if (sampleSize >= ZT_MULTICAST_PICK_MAX_SAMPLE_SIZE)
							break; // abort if we have enough candidates
					}
					++channelMemberEntry;
				}

				// Auto-clean: erase whole map if there are no more LIKEs for this channel
				if (channelMembers->second.empty())
					_multicastMemberships.erase(channelMembers);
			}
		}

		// Sort in descending order of most recent direct unicast frame, picking
		// peers with whom we have recently communicated. This is "implicit social
		// switching."
		std::sort(&(toConsider[0]),&(toConsider[sampleSize]),PeerPropagationPrioritySortOrder<P>());

		// Decay a few random bits in bloom filter to probabilistically eliminate
		// false positives as we go. The odds of decaying an already-set bit
		// increases as the bloom filter saturates, so in the early hops of
		// propagation this likely won't have any effect.
		for(unsigned int i=0;i<ZT_MULTICAST_BLOOM_FILTER_DECAY_RATE;++i)
			bf.decay();

		// Pick peers not in the bloom filter, setting bloom filter bits accordingly to
		// remember and pass on these picks.
		unsigned int picked = 0;
		for(unsigned int i=0;((i<sampleSize)&&(picked < max));++i) {
			if (!bf.set(toConsider[i]->address().sum()))
				peers[picked++] = toConsider[i];
		}

		// Add a supernode if there's nowhere else to go. Supernodes know of all multicast
		// LIKEs and so can act to bridge sparse multicast groups. We do not remember them
		// in the bloom filter, since such bridging may very well need to happen more than
		// once.
		if (!picked) {
			P peer = topology.getBestSupernode();
			if (peer)
				peers[picked++] = peer;
		}

		return picked;
	}

private:
	// Sort order for chosen propagation peers
	template<typename P>
	struct PeerPropagationPrioritySortOrder
	{
		inline bool operator()(const P &p1,const P &p2) const
		{
			return (p1->lastUnicastFrame() >= p2->lastUnicastFrame());
		}
	};

	static inline void _hashMulticastPacketForSig(const MAC &from,const MulticastGroup &to,unsigned int etherType,const void *data,unsigned int len,unsigned char *digest)
		throw()
	{
		unsigned char zero = 0;
		SHA256_CTX sha;
		SHA256_Init(&sha);
		uint64_t _nwid = Utils::hton(network->id());
		SHA256_Update(&sha,(unsigned char *)&_nwid,sizeof(_nwid));
		SHA256_Update(&sha,&zero,1);
		SHA256_Update(&sha,(unsigned char *)from.data,6);
		SHA256_Update(&sha,&zero,1);
		SHA256_Update(&sha,(unsigned char *)mg.mac().data,6);
		SHA256_Update(&sha,&zero,1);
		uint32_t _adi = Utils::hton(mg.adi());
		SHA256_Update(&sha,(unsigned char *)&_adi,sizeof(_adi));
		SHA256_Update(&sha,&zero,1);
		uint16_t _etype = Utils::hton((uint16_t)etherType);
		SHA256_Update(&sha,(unsigned char *)&_etype,sizeof(_etype));
		SHA256_Update(&sha,&zero,1);
		SHA256_Update(&sha,(unsigned char *)data,len);
		SHA256_Final(digest,&sha);
	}

	// [0] - CRC, [1] - timestamp
	uint64_t _multicastHistory[ZT_MULTICAST_DEDUP_HISTORY_LENGTH][2];

	// A multicast channel, essentially a pub/sub channel. It consists of a
	// network ID and a multicast group within that network.
	typedef std::pair<uint64_t,MulticastGroup> MulticastChannel;

	// Address and time of last LIKE, by network ID and multicast group
	std::map< MulticastChannel,std::map<Address,uint64_t> > _multicastMemberships;
	Mutex _multicastMemberships_m;
};

} // namespace ZeroTier

#endif
