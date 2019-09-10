/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_MULTICASTER_HPP
#define ZT_MULTICASTER_HPP

#include <stdint.h>
#include <string.h>

#include <map>
#include <vector>

#include "Constants.hpp"
#include "Hashtable.hpp"
#include "Address.hpp"
#include "MAC.hpp"
#include "MulticastGroup.hpp"
#include "Utils.hpp"
#include "Mutex.hpp"
#include "SharedPtr.hpp"
#include "Packet.hpp"

// Size in bits -- do not change as this is about as large as we can support
// This leaves room for up to 10000 MTU data (max supported MTU) and header
// information in a maximum supported size packet. Note that data compression
// will practically reduce this size in transit for sparse or saturated fields.
#define ZT_MULTICAST_BLOOM_FILTER_SIZE_BITS 50048

namespace ZeroTier {

class RuntimeEnvironment;
class CertificateOfMembership;
class Packet;
class Network;

/**
 * Multicast database and outbound multicast logic
 */
class Multicaster
{
private:
	// Composite key of network ID and multicast group
	struct _K
	{
		uint64_t nwid;
		MulticastGroup mg;

		ZT_ALWAYS_INLINE _K() : nwid(0),mg() {}
		ZT_ALWAYS_INLINE _K(const uint64_t n,const MulticastGroup &g) : nwid(n),mg(g) {}
		ZT_ALWAYS_INLINE bool operator==(const _K &k) const { return ((nwid == k.nwid)&&(mg == k.mg)); }
		ZT_ALWAYS_INLINE bool operator!=(const _K &k) const { return ((nwid != k.nwid)||(mg != k.mg)); }
		ZT_ALWAYS_INLINE unsigned long hashCode() const { return (mg.hashCode() ^ (unsigned long)(nwid ^ (nwid >> 32))); }
	};

	// Multicast group info
	struct _G
	{
		ZT_ALWAYS_INLINE _G() : lastGather(0),members(16) {}
		int64_t lastGather;
		Hashtable< Address,int64_t > members;
	};

	// Outbound multicast
	struct _OM
	{
		uint64_t nwid;
		MAC src;
		MulticastGroup mg;
		unsigned int etherType;
		unsigned int dataSize;
		unsigned int count;
		unsigned int limit;
		unsigned int bloomFilterMultiplier;
		uint64_t bloomFilter[ZT_MULTICAST_BLOOM_FILTER_SIZE_BITS / 64];
		uint8_t data[ZT_MAX_MTU];
		Mutex lock;
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
	ZT_ALWAYS_INLINE void add(const int64_t now,const uint64_t nwid,const MulticastGroup &mg,const Address &member)
	{
		Mutex::Lock l(_groups_l);
		_groups[_K(nwid,mg)].members.set(member,now);
	}

	/**
	 * Add multiple addresses from a binary array of 5-byte address fields
	 *
	 * It's up to the caller to check bounds on the array before calling this.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param now Current time
	 * @param nwid Network ID
	 * @param mg Multicast group
	 * @param addresses Raw binary addresses in big-endian format, as a series of 5-byte fields
	 * @param count Number of addresses
	 * @param totalKnown Total number of known addresses as reported by peer
	 */
	ZT_ALWAYS_INLINE void addMultiple(const int64_t now,const uint64_t nwid,const MulticastGroup &mg,const void *addresses,unsigned int count,const unsigned int totalKnown)
	{
		Mutex::Lock l(_groups_l);
		const uint8_t *a = (const uint8_t *)addresses;
		_G &g = _groups[_K(nwid,mg)];
		while (count--) {
			g.members.set(Address(a,ZT_ADDRESS_LENGTH),now);
			a += ZT_ADDRESS_LENGTH;
		}
	}

	/**
	 * Remove a multicast group member (if present)
	 *
	 * @param nwid Network ID
	 * @param mg Multicast group
	 * @param member Member to unsubscribe
	 */
	ZT_ALWAYS_INLINE void remove(const uint64_t nwid,const MulticastGroup &mg,const Address &member)
	{
		Mutex::Lock l(_groups_l);
		const _K gk(nwid,mg);
		_G *const g = _groups.get(gk);
		if (g) {
			g->members.erase(member);
			if (g->members.empty())
				_groups.erase(gk);
		}
	}

	/**
	 * Iterate over members of a multicast group until function returns false
	 *
	 * Iteration order is in inverse order of most recent receipt of a LIKE
	 * for a given membership.
	 *
	 * @param nwid Network ID
	 * @param mg Multicast group
	 * @param func f(Address)
	 * @return Total number of known members (regardless of when function aborted)
	 */
	template<typename F>
	ZT_ALWAYS_INLINE unsigned long eachMember(const uint64_t nwid,const MulticastGroup &mg,F func) const
	{
		std::vector< std::pair<int64_t,Address> > sortedByTime;
		{
			Mutex::Lock l(_groups_l);
			const _K gk(nwid,mg);
			const _G *const g = _groups.get(gk);
			if (g) {
				sortedByTime.reserve(g->members.size());
				{
					Hashtable< Address,int64_t >::Iterator mi(const_cast<_G *>(g)->members);
					Address *mik = nullptr;
					int64_t *miv = nullptr;
					while (mi.next(mik,miv))
					sortedByTime.push_back(std::pair<int64_t,Address>(*miv,*mik));
				}
			}
		}
		std::sort(sortedByTime.begin(),sortedByTime.end());
		for(std::vector< std::pair<int64_t,Address> >::const_reverse_iterator i(sortedByTime.begin());i!=sortedByTime.end();++i) {
			if (!func(i->second))
				break;
		}
		return sortedByTime.size();
	}

	/**
	 * Send a multicast
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param now Current time
	 * @param network Network
	 * @param mg Multicast group
	 * @param src Source Ethernet MAC address or NULL to skip in packet and compute from ZT address (non-bridged mode)
	 * @param etherType Ethernet frame type
	 * @param existingBloomMultiplier Existing bloom filter multiplier or 0 if none
	 * @param existingBloom Existing bloom filter or NULL if none
	 * @param data Packet data
	 * @param len Length of packet data
	 * @return Number of known recipients for multicast (including bridges and replicators)
	 */
	unsigned int send(
		void *tPtr,
		int64_t now,
		const SharedPtr<Network> &network,
		const MulticastGroup &mg,
		const MAC &src,
		unsigned int etherType,
		const unsigned int existingBloomMultiplier,
		const uint8_t existingBloom[ZT_MULTICAST_BLOOM_FILTER_SIZE_BITS / 8],
		const void *const data,
		unsigned int len);

	/**
	 * Clean up database
	 *
	 * @param RR Runtime environment
	 * @param now Current time
	 */
	void clean(int64_t now);

private:
	const RuntimeEnvironment *const RR;

	_OM _txQueue[ZT_TX_QUEUE_SIZE];
	unsigned int _txQueuePtr;
	Mutex _txQueue_l;

	Hashtable< _K,_G > _groups;
	Mutex _groups_l;
};

} // namespace ZeroTier

#endif
