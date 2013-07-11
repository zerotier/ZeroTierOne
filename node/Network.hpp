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

#ifndef _ZT_NETWORK_HPP
#define _ZT_NETWORK_HPP

#include <string>
#include <set>
#include <vector>
#include <stdexcept>
#include "EthernetTap.hpp"
#include "Address.hpp"
#include "Mutex.hpp"
#include "InetAddress.hpp"
#include "Constants.hpp"
#include "SharedPtr.hpp"
#include "AtomicCounter.hpp"
#include "RuntimeEnvironment.hpp"
#include "MulticastGroup.hpp"
#include "NonCopyable.hpp"
#include "MAC.hpp"

namespace ZeroTier {

class NodeConfig;

/**
 * Local network endpoint
 */
class Network : NonCopyable
{
	friend class SharedPtr<Network>;
	friend class NodeConfig;

private:
	Network(const RuntimeEnvironment *renv,uint64_t id)
		throw(std::runtime_error);

	~Network();

public:
	/**
	 * @return Network ID
	 */
	inline uint64_t id() const throw() { return _id; }

	/**
	 * @return Ethernet tap
	 */
	inline EthernetTap &tap() throw() { return _tap; }

	/**
	 * Get this network's members
	 * 
	 * If this is an open network, membership isn't relevant and this doesn't
	 * mean much. If it's a closed network, frames will only be exchanged to/from
	 * members.
	 * 
	 * @return Members of this network
	 */
	inline std::set<Address> members() const
	{
		Mutex::Lock _l(_lock);
		return _members;
	}

	/**
	 * @param addr Address to check
	 * @return True if address is a member
	 */
	inline bool isMember(const Address &addr) const
		throw()
	{
		Mutex::Lock _l(_lock);
		return (_members.count(addr) > 0);
	}

	/**
	 * Shortcut to check open() and then isMember()
	 * 
	 * @param addr Address to check
	 * @return True if network is open or if address is a member
	 */
	inline bool isAllowed(const Address &addr) const
		throw()
	{
		Mutex::Lock _l(_lock);
		return ((_open)||(_members.count(addr) > 0));
	}

	/**
	 * @return True if network is open (no membership required)
	 */
	inline bool open() const
		throw()
	{
		Mutex::Lock _l(_lock);
		return _open;
	}

	/**
	 * Update internal multicast group set and return true if changed
	 *
	 * @return True if internal multicast group set has changed
	 */
	inline bool updateMulticastGroups()
	{
		Mutex::Lock _l(_lock);
		return _tap.updateMulticastGroups(_multicastGroups);
	}

	/**
	 * @return Latest set of multicast groups
	 */
	inline std::set<MulticastGroup> multicastGroups() const
	{
		Mutex::Lock _l(_lock);
		return _multicastGroups;
	}

private:
	static void _CBhandleTapData(void *arg,const MAC &from,const MAC &to,unsigned int etherType,const Buffer<4096> &data);

	const RuntimeEnvironment *_r;
	uint64_t _id;
	EthernetTap _tap;
	std::set<Address> _members;
	std::set<MulticastGroup> _multicastGroups;
	bool _open;
	Mutex _lock;

	AtomicCounter __refCount;
};

} // naemspace ZeroTier

#endif
