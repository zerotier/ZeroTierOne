/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2014  ZeroTier Networks LLC
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

#ifndef ZT_MULTICASTTOPOLOGY_HPP
#define ZT_MULTICASTTOPOLOGY_HPP

#include <stdint.h>
#include <string.h>

#include <map>
#include <vector>

#include "Constants.hpp"
#include "Address.hpp"
#include "MulticastGroup.hpp"
#include "Mutex.hpp"
#include "Utils.hpp"

namespace ZeroTier {

class Topology;

/**
 * Database of known multicast peers within a network
 */
class MulticastTopology
{
private:
	struct MulticastGroupMember
	{
		MulticastGroupMember() {}
		MulticastGroupMember(const Address &a,const Address &lf,uint64_t ts) : address(a),learnedFrom(lf),timestamp(ts) {}

		Address address;
		Address learnedFrom; // NULL/0 for addresses directly learned from LIKE
		uint64_t timestamp; // time of last LIKE or OK response to MULTICAST_LONELY
		uint64_t rank; // used by sorting algorithm in clean()

		// for sorting in ascending order of rank
		inline bool operator<(const MulticastGroupMember &m) const throw() { return (rank < m.rank); }
	};

public:
	MulticastTopology();
	~MulticastTopology();

	/**
	 * Add or update a member in a multicast group
	 *
	 * @param mg Multicast group
	 * @param member Member to add/update
	 * @param learnedFrom Address from which we learned this member or NULL/0 Address if direct
	 */
	inline void add(const MulticastGroup &mg,const Address &member,const Address &learnedFrom)
	{
		Mutex::Lock _l(_members_m);
		std::vector<MulticastGroupMember> &mv = _members[mg];
		for(std::vector<MulticastGroupMember>::iterator m(mv.begin());m!=mv.end();++m) {
			if (m->address == member) {
				if (m->learnedFrom) // once a member has been seen directly, we keep its status as direct
					m->learnedFrom = learnedFrom;
				m->timestamp = Utils::now();
				return;
			}
		}
		mv.push_back(MulticastGroupMember(member,learnedFrom,Utils::now()));
	}

	/**
	 * Erase a member from a multicast group (if present)
	 *
	 * @param mg Multicast group
	 * @param member Member to erase
	 */
	inline void erase(const MulticastGroup &mg,const Address &member)
	{
		Mutex::Lock _l(_members_m);
		std::map< MulticastGroup,std::vector<MulticastGroupMember> >::iterator r(_members.find(mg));
		if (r != _members.end()) {
			for(std::vector<MulticastGroupMember>::iterator m(r->second.begin());m!=r->second.end();++m) {
				if (m->address == member) {
					r->second.erase(m);
					return;
				}
			}
		}
	}

	/**
	 * @param mg Multicast group
	 * @return Number of known peers in group
	 */
	inline unsigned int memberCount(const MulticastGroup &mg) const
	{
		Mutex::Lock _l(_members_m);
		std::map< MulticastGroup,std::vector<MulticastGroupMember> >::const_iterator r(_members.find(mg));
		return ((r != _members.end()) ? (unsigned int)r->second.size() : (unsigned int)0);
	}

	/**
	 * Iterate over the known members of a multicast group
	 *
	 * @param mg Multicast group
	 * @param func Function to be called with multicast group and address of member
	 * @tparam F Function type (explicitly template on "FuncObj &" if reference instead of copy should be passed)
	 * @return Number of members in multicast group for which function was called
	 */
	template<typename F>
	inline unsigned int eachMember(const MulticastGroup &mg,F func) const
	{
		Mutex::Lock _l(_members_m);
		std::map< MulticastGroup,std::vector<MulticastGroupMember> >::const_iterator r(_members.find(mg));
		if (r != _members.end()) {
			// We go in reverse order because most recently learned members are pushed to the end
			// of the vector. The priority resort algorithm in clean() sorts in ascending order
			// of propagation priority too.
			for(std::vector<MulticastGroupMember>::const_reverse_iterator m(r->second.rbegin());m!=r->second.rend();++m) {
				func(mg,m->address);
			}
			return (unsigned int)r->second.size();
		} else return 0;
	}

	/**
	 * Clean up and resort database
	 *
	 * @param topology Global peer topology
	 */
	void clean(const Topology &topology);

private:
	std::map< MulticastGroup,std::vector<MulticastGroupMember> > _members;
	Mutex _members_m;
};

} // namespace ZeroTier

#endif
