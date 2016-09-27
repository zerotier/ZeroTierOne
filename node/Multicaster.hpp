/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
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
 */

#ifndef ZT_MULTICASTER_HPP
#define ZT_MULTICASTER_HPP

#include <stdint.h>
#include <string.h>

#include <map>
#include <vector>
#include <list>

#include "Constants.hpp"
#include "Hashtable.hpp"
#include "Address.hpp"
#include "MAC.hpp"
#include "MulticastGroup.hpp"
#include "OutboundMulticast.hpp"
#include "Utils.hpp"
#include "Mutex.hpp"
#include "NonCopyable.hpp"

namespace ZeroTier {

class RuntimeEnvironment;
class CertificateOfMembership;
class Packet;

/**
 * Database of known multicast peers within a network
 */
class Multicaster : NonCopyable
{
private:
	struct Key
	{
		Key() : nwid(0),mg() {}
		Key(uint64_t n,const MulticastGroup &g) : nwid(n),mg(g) {}

		uint64_t nwid;
		MulticastGroup mg;

		inline bool operator==(const Key &k) const throw() { return ((nwid == k.nwid)&&(mg == k.mg)); }
		inline unsigned long hashCode() const throw() { return (mg.hashCode() ^ (unsigned long)(nwid ^ (nwid >> 32))); }
	};

	struct MulticastGroupMember
	{
		MulticastGroupMember() {}
		MulticastGroupMember(const Address &a,uint64_t ts) : address(a),timestamp(ts) {}

		Address address;
		uint64_t timestamp; // time of last notification
	};

	struct MulticastGroupStatus
	{
		MulticastGroupStatus() : lastExplicitGather(0) {}

		uint64_t lastExplicitGather;
		std::list<OutboundMulticast> txQueue; // pending outbound multicasts
		std::vector<MulticastGroupMember> members; // members of this group
	};

public:
	Multicaster(const RuntimeEnvironment *renv);
	~Multicaster();

	/**
	 * Add or update a member in a multicast group
	 *
	 * @param now Current time
	 * @param nwid Network ID
	 * @param mg Multicast group
	 * @param member New member address
	 */
	inline void add(uint64_t now,uint64_t nwid,const MulticastGroup &mg,const Address &member)
	{
		Mutex::Lock _l(_groups_m);
		_add(now,nwid,mg,_groups[Multicaster::Key(nwid,mg)],member);
	}

	/**
	 * Add multiple addresses from a binary array of 5-byte address fields
	 *
	 * It's up to the caller to check bounds on the array before calling this.
	 *
	 * @param now Current time
	 * @param nwid Network ID
	 * @param mg Multicast group
	 * @param addresses Raw binary addresses in big-endian format, as a series of 5-byte fields
	 * @param count Number of addresses
	 * @param totalKnown Total number of known addresses as reported by peer
	 */
	void addMultiple(uint64_t now,uint64_t nwid,const MulticastGroup &mg,const void *addresses,unsigned int count,unsigned int totalKnown);

	/**
	 * Remove a multicast group member (if present)
	 *
	 * @param nwid Network ID
	 * @param mg Multicast group
	 * @param member Member to unsubscribe
	 */
	void remove(uint64_t nwid,const MulticastGroup &mg,const Address &member);

	/**
	 * Append gather results to a packet by choosing registered multicast recipients at random
	 *
	 * This appends the following fields to the packet:
	 *   <[4] 32-bit total number of known members in this multicast group>
	 *   <[2] 16-bit number of members enumerated in this packet>
	 *   <[...] series of 5-byte ZeroTier addresses of enumerated members>
	 *
	 * If zero is returned, the first two fields will still have been appended.
	 *
	 * @param queryingPeer Peer asking for gather (to skip in results)
	 * @param nwid Network ID
	 * @param mg Multicast group
	 * @param appendTo Packet to append to
	 * @param limit Maximum number of 5-byte addresses to append
	 * @return Number of addresses appended
	 * @throws std::out_of_range Buffer overflow writing to packet
	 */
	unsigned int gather(const Address &queryingPeer,uint64_t nwid,const MulticastGroup &mg,Buffer<ZT_PROTO_MAX_PACKET_LENGTH> &appendTo,unsigned int limit) const;

	/**
	 * Get subscribers to a multicast group
	 *
	 * @param nwid Network ID
	 * @param mg Multicast group
	 */
	std::vector<Address> getMembers(uint64_t nwid,const MulticastGroup &mg,unsigned int limit) const;

	/**
	 * Send a multicast
	 *
	 * @param limit Multicast limit
	 * @param now Current time
	 * @param nwid Network ID
	 * @param disableCompression Disable packet payload compression?
	 * @param alwaysSendTo Send to these peers first and even if not included in subscriber list
	 * @param mg Multicast group
	 * @param src Source Ethernet MAC address or NULL to skip in packet and compute from ZT address (non-bridged mode)
	 * @param etherType Ethernet frame type
	 * @param data Packet data
	 * @param len Length of packet data
	 */
	void send(
		unsigned int limit,
		uint64_t now,
		uint64_t nwid,
		bool disableCompression,
		const std::vector<Address> &alwaysSendTo,
		const MulticastGroup &mg,
		const MAC &src,
		unsigned int etherType,
		const void *data,
		unsigned int len);

	/**
	 * Clean up and resort database
	 *
	 * @param RR Runtime environment
	 * @param now Current time
	 */
	void clean(uint64_t now);

	/**
	 * Add an authorization credential
	 *
	 * The Multicaster keeps its own track of when valid credentials of network
	 * membership are presented. This allows it to control MULTICAST_LIKE
	 * GATHER authorization for networks this node does not belong to.
	 *
	 * @param com Certificate of membership
	 * @param alreadyValidated If true, COM has already been checked and found to be valid and signed
	 */
	void addCredential(const CertificateOfMembership &com,bool alreadyValidated);

	/**
	 * Check authorization for GATHER and LIKE for non-network-members
	 *
	 * @param a Address of peer
	 * @param nwid Network ID
	 * @param now Current time
	 * @return True if GATHER and LIKE should be allowed
	 */
	bool cacheAuthorized(const Address &a,const uint64_t nwid,const uint64_t now) const
	{
		Mutex::Lock _l(_gatherAuth_m);
		const uint64_t *p = _gatherAuth.get(_GatherAuthKey(nwid,a));
		return ((p)&&((now - *p) < ZT_MULTICAST_CREDENTIAL_EXPIRATON));
	}

private:
	void _add(uint64_t now,uint64_t nwid,const MulticastGroup &mg,MulticastGroupStatus &gs,const Address &member);

	const RuntimeEnvironment *RR;

	Hashtable<Multicaster::Key,MulticastGroupStatus> _groups;
	Mutex _groups_m;

	struct _GatherAuthKey
	{
		_GatherAuthKey() : member(0),networkId(0) {}
		_GatherAuthKey(const uint64_t nwid,const Address &a) : member(a.toInt()),networkId(nwid) {}
		inline unsigned long hashCode() const { return (member ^ networkId); }
		inline bool operator==(const _GatherAuthKey &k) const { return ((member == k.member)&&(networkId == k.networkId)); }
		uint64_t member;
		uint64_t networkId;
	};
	Hashtable< _GatherAuthKey,uint64_t > _gatherAuth;
	Mutex _gatherAuth_m;
};

} // namespace ZeroTier

#endif
