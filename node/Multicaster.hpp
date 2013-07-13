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
#include "CMWC4096.hpp"

// Maximum sample size to pick during choice of multicast propagation peers
#define ZT_MULTICAST_PICK_MAX_SAMPLE_SIZE (ZT_MULTICAST_PROPAGATION_BREADTH * 8)

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
	 * @return ECDSA signature
	 */
	static inline std::string signMulticastPacket(const Identity &id,uint64_t nwid,const MAC &from,const MulticastGroup &to,unsigned int etherType,const void *data,unsigned int len)
	{
		unsigned char digest[32];
		_hashMulticastPacketForSig(nwid,from,to,etherType,data,len,digest);
		return id.sign(digest);
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
	 * @param signature ECDSA signature
	 * @param siglen Length of signature in bytes
	 * @return ECDSA signature
	 */
	static bool verifyMulticastPacket(const Identity &id,uint64_t nwid,const MAC &from,const MulticastGroup &to,unsigned int etherType,const void *data,unsigned int len,const void *signature,unsigned int siglen)
	{
		unsigned char digest[32];
		_hashMulticastPacketForSig(nwid,from,to,etherType,data,len,digest);
		return id.verifySignature(digest,signature,siglen);
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
			if ((_multicastHistory[i][0] == crc)&&((now - _multicastHistory[i][1]) < ZT_MULTICAST_DEDUP_HISTORY_EXPIRE))
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
	 * Choose peers to send a propagating multicast to
	 *
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
	inline unsigned int pickNextPropagationPeers(
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
		P toConsider[ZT_MULTICAST_PICK_MAX_SAMPLE_SIZE];
		unsigned int sampleSize = 0;

		// Decay a few random bits in bloom filter to probabilistically eliminate
		// false positives as we go. The odds of decaying an already-set bit
		// increases as the bloom filter saturates, so in the early hops of
		// propagation this likely won't have any effect. This allows peers with
		// bloom filter collisions to be reconsidered, but at positions on the
		// network graph likely to be hops away from the original origin of the
		// message.
		for(unsigned int i=0;i<ZT_MULTICAST_BLOOM_FILTER_DECAY_RATE;++i)
			bf.decay((unsigned int)prng.next32());

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
						if (prng.nextDouble() <= skipWhatFraction) {
							--numEntriesPermittedToSkip;
							++channelMemberEntry;
							continue;
						}
					}

					// If it's not expired and it's from our random sample, add it to the set of peers
					// to consider. Exclude immediate upstream and original submitter, since we know for
					// a fact they've already seen this. Also exclude things in the bloom filter.
					if ((channelMemberEntry->first != originalSubmitter)&&(channelMemberEntry->first != upstream)) {
						if (!bf.contains(channelMemberEntry->first.sum())) {
							P peer = topology.getPeer(channelMemberEntry->first);
							if ((peer)&&(peer->hasActiveDirectPath(now))) {
								toConsider[sampleSize++] = peer;
								if (sampleSize >= ZT_MULTICAST_PICK_MAX_SAMPLE_SIZE)
									break; // abort if we have enough candidates
							}
						}
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
		std::sort(toConsider,toConsider + sampleSize,PeerPropagationPrioritySortOrder<P>());

		// Pick the best N peers
		unsigned int picked = 0;
		for(unsigned int i=0;((i<sampleSize)&&(picked < max));++i) {
			peers[picked++] = toConsider[i];
			bf.set(toConsider[i]->address().sum());
		}

		// Add a supernode if there's nowhere else to go. Supernodes know of all multicast
		// LIKEs and so can act to bridge sparse multicast groups.
		if (!picked) {
			P peer = topology.getBestSupernode(&originalSubmitter,1,true);
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
			return (p1->lastUnicastFrame() > p2->lastUnicastFrame());
		}
	};

	static inline void _hashMulticastPacketForSig(uint64_t nwid,const MAC &from,const MulticastGroup &to,unsigned int etherType,const void *data,unsigned int len,unsigned char *digest)
		throw()
	{
		unsigned char zero = 0;
		SHA256_CTX sha;
		SHA256_Init(&sha);
		uint64_t _nwid = Utils::hton(nwid);
		SHA256_Update(&sha,(unsigned char *)&_nwid,sizeof(_nwid));
		SHA256_Update(&sha,&zero,1);
		SHA256_Update(&sha,(unsigned char *)from.data,6);
		SHA256_Update(&sha,&zero,1);
		SHA256_Update(&sha,(unsigned char *)to.mac().data,6);
		SHA256_Update(&sha,&zero,1);
		uint32_t _adi = Utils::hton(to.adi());
		SHA256_Update(&sha,(unsigned char *)&_adi,sizeof(_adi));
		SHA256_Update(&sha,&zero,1);
		uint16_t _etype = Utils::hton((uint16_t)etherType);
		SHA256_Update(&sha,(unsigned char *)&_etype,sizeof(_etype));
		SHA256_Update(&sha,&zero,1);
		SHA256_Update(&sha,(unsigned char *)data,len);
		SHA256_Final(digest,&sha);
	}

	// ring buffer: [0] - CRC, [1] - timestamp
	uint64_t _multicastHistory[ZT_MULTICAST_DEDUP_HISTORY_LENGTH][2];
	volatile unsigned int _multicastHistoryPtr;

	// A multicast channel, essentially a pub/sub channel. It consists of a
	// network ID and a multicast group within that network.
	typedef std::pair<uint64_t,MulticastGroup> MulticastChannel;

	// Address and time of last LIKE, by network ID and multicast group
	std::map< MulticastChannel,std::map<Address,uint64_t> > _multicastMemberships;
	Mutex _multicastMemberships_m;
};

} // namespace ZeroTier

#endif
