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

#ifndef ZT_NODECONFIG_HPP
#define ZT_NODECONFIG_HPP

#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <vector>
#include <stdexcept>

#include "SharedPtr.hpp"
#include "Network.hpp"
#include "Utils.hpp"
#include "Buffer.hpp"
#include "Dictionary.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Node configuration endpoint
 */
class NodeConfig
{
public:
	/**
	 * @param renv Runtime environment
	 * @throws std::runtime_error Unable to initialize or listen for IPC connections
	 */
	NodeConfig(const RuntimeEnvironment *renv);

	~NodeConfig();

	/**
	 * Store something in local configuration cache
	 *
	 * By convention, keys starting with _ will not be shown in the command bus
	 * local config functions.
	 *
	 * @param key Configuration key
	 * @param value Configuration value
	 */
	void putLocalConfig(const std::string &key,const char *value);
	void putLocalConfig(const std::string &key,const std::string &value);

	/**
	 * @param key Configuration key
	 * @return Value or empty string if not found
	 */
	std::string getLocalConfig(const std::string &key) const;

	/**
	 * @param nwid Network ID
	 * @return Network or NULL if no network for that ID
	 */
	inline SharedPtr<Network> network(uint64_t nwid) const
	{
		Mutex::Lock _l(_networks_m);
		std::map< uint64_t,SharedPtr<Network> >::const_iterator n(_networks.find(nwid));
		return ((n == _networks.end()) ? SharedPtr<Network>() : n->second);
	}

	/**
	 * @return Vector containing all networks
	 */
	inline std::vector< SharedPtr<Network> > networks() const
	{
		std::vector< SharedPtr<Network> > nwlist;
		Mutex::Lock _l(_networks_m);
		for(std::map< uint64_t,SharedPtr<Network> >::const_iterator n(_networks.begin());n!=_networks.end();++n)
			nwlist.push_back(n->second);
		return nwlist;
	}

	/**
	 * Join a network or return existing network if already joined
	 *
	 * @param nwid Network ID to join
	 * @return New network instance
	 */
	inline SharedPtr<Network> join(uint64_t nwid)
	{
		Mutex::Lock _l(_networks_m);
		SharedPtr<Network> &nw = _networks[nwid];
		if (nw)
			return nw;
		else return (nw = Network::newInstance(_r,this,nwid));
	}

	/**
	 * Leave a network
	 *
	 * @param nwid Network ID
	 * @return True if network was left, false if we were not a member of this network
	 */
	inline bool leave(uint64_t nwid)
	{
		Mutex::Lock _l(_networks_m);
		std::map< uint64_t,SharedPtr<Network> >::iterator n(_networks.find(nwid));
		if (n != _networks.end()) {
			n->second->destroy();
			_networks.erase(n);
			return true;
		} else return false;
	}

	/**
	 * Perform cleanup and possibly persist saved state
	 */
	void clean();

	/**
	 * @param nwid Network ID
	 * @return True if this network exists
	 */
	inline bool hasNetwork(uint64_t nwid)
	{
		Mutex::Lock _l(_networks_m);
		return (_networks.count(nwid) > 0);
	}

	/**
	 * @return Sorted vector of network tap device names from our virtual networks (not other taps on system)
	 */
	inline std::vector<std::string> networkTapDeviceNames() const
	{
		std::vector<std::string> tapDevs;
		Mutex::Lock _l(_networks_m);
		for(std::map< uint64_t,SharedPtr<Network> >::const_iterator n(_networks.begin());n!=_networks.end();++n) {
			std::string dn(n->second->tapDeviceName());
			if (dn.length())
				tapDevs.push_back(dn);
		}
		return tapDevs;
	}

	void _readLocalConfig();
	void _writeLocalConfig();

	const RuntimeEnvironment *_r;
	Dictionary _localConfig; // persisted as local.conf
	Mutex _localConfig_m;
	std::map< uint64_t,SharedPtr<Network> > _networks; // persisted in networks.d/
	Mutex _networks_m;
};

} // namespace ZeroTier

#endif
