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
#include "OutboundMulticast.hpp"
#include "Utils.hpp"
#include "Mutex.hpp"
#include "SharedPtr.hpp"

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
	inline void add(const int64_t now,const uint64_t nwid,const MulticastGroup &mg,const Address &member)
	{
		Mutex::Lock l(_groups_l);
		_groups[Multicaster::Key(nwid,mg)].set(member,now);
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
	inline void addMultiple(const int64_t now,const uint64_t nwid,const MulticastGroup &mg,const void *addresses,unsigned int count,const unsigned int totalKnown)
	{
		Mutex::Lock l(_groups_l);
		const uint8_t *a = (const uint8_t *)addresses;
		Hashtable< Address,int64_t > &members = _groups[Multicaster::Key(nwid,mg)];
		while (count--) {
			members.set(Address(a,ZT_ADDRESS_LENGTH),now);
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
	inline void remove(const uint64_t nwid,const MulticastGroup &mg,const Address &member)
	{
		Mutex::Lock l(_groups_l);
		const Multicaster::Key gk(nwid,mg);
		Hashtable< Address,int64_t > *const members = _groups.get(gk);
		if (members) {
			members->erase(member);
			if (members->empty())
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
	inline unsigned long eachMember(const uint64_t nwid,const MulticastGroup &mg,F func) const
	{
		std::vector< std::pair<int64_t,Address> > sortedByTime;
		{
			Mutex::Lock l(_groups_l);
			const Multicaster::Key gk(nwid,mg);
			const Hashtable< Address,int64_t > *const members = _groups.get(gk);
			if (members) {
				totalKnown = members->size();
				sortedByTime.reserve(totalKnown);
				{
					Hashtable< Address,int64_t >::Iterator mi(*const_cast<Hashtable< Address,int64_t > *>(members));
					Address *mik = nullptr;
					int64_t *miv = nullptr;
					while (mi.next(mik,miv))
						sortedByTime.push_back(std::pair<int64_t,Address>(*miv,*mik));
				}
				std::sort(sortedByTime.begin(),sortedByTime.end());
			}
		}
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
	 * @param origin Origin of multicast (to not return to sender) or NULL if none
	 * @param mg Multicast group
	 * @param src Source Ethernet MAC address or NULL to skip in packet and compute from ZT address (non-bridged mode)
	 * @param etherType Ethernet frame type
	 * @param data Packet data
	 * @param len Length of packet data
	 */
	void send(
		void *tPtr,
		int64_t now,
		const SharedPtr<Network> &network,
		const Address &origin,
		const MulticastGroup &mg,
		const MAC &src,
		unsigned int etherType,
		const void *data,
		unsigned int len);

	/**
	 * Clean up database
	 *
	 * @param RR Runtime environment
	 * @param now Current time
	 */
	void clean(int64_t now);

private:
	struct Key
	{
		ZT_ALWAYS_INLINE Key() : nwid(0),mg() {}
		ZT_ALWAYS_INLINE Key(const uint64_t n,const MulticastGroup &g) : nwid(n),mg(g) {}

		uint64_t nwid;
		MulticastGroup mg;

		ZT_ALWAYS_INLINE bool operator==(const Key &k) const { return ((nwid == k.nwid)&&(mg == k.mg)); }
		ZT_ALWAYS_INLINE bool operator!=(const Key &k) const { return ((nwid != k.nwid)||(mg != k.mg)); }

		ZT_ALWAYS_INLINE unsigned long hashCode() const { return (mg.hashCode() ^ (unsigned long)(nwid ^ (nwid >> 32))); }
	};

	const RuntimeEnvironment *const RR;

	OutboundMulticast _txQueue[ZT_TX_QUEUE_SIZE];
	unsigned int _txQueuePtr;
	Mutex _txQueue_l;

	Hashtable< Multicaster::Key,Hashtable< Address,int64_t > > _groups;
	Mutex _groups_l;
};

} // namespace ZeroTier

#endif
