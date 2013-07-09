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

#ifndef _ZT_NODECONFIG_HPP
#define _ZT_NODECONFIG_HPP

#include <map>
#include <set>
#include <string>
#include <stdint.h>
#include "SharedPtr.hpp"
#include "Network.hpp"
#include "Utils.hpp"
#include "Http.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Node configuration holder and fetcher
 */
class NodeConfig
{
public:
	/**
	 * @param renv Runtime environment
	 * @param url Autoconfiguration URL (http:// or file://)
	 */
	NodeConfig(const RuntimeEnvironment *renv,const std::string &url);

	~NodeConfig();

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
	 * Call whack() on all networks' tap devices
	 */
	inline void whackAllTaps()
	{
		std::vector< SharedPtr<Network> > nwlist;
		Mutex::Lock _l(_networks_m);
		for(std::map< uint64_t,SharedPtr<Network> >::const_iterator n(_networks.begin());n!=_networks.end();++n)
			n->second->tap().whack();
	}

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
	 * @return Set of network tap device names
	 */
	inline std::set<std::string> networkTapDeviceNames() const
	{
		std::set<std::string> tapDevs;
		Mutex::Lock _l(_networks_m);
		for(std::map< uint64_t,SharedPtr<Network> >::const_iterator n(_networks.begin());n!=_networks.end();++n)
			tapDevs.insert(n->second->tap().deviceName());
		return tapDevs;
	}

	/**
	 * @return Time of last successful autoconfigure or refresh
	 */
	inline uint64_t lastAutoconfigure() const { return _lastAutoconfigure; }

	/**
	 * @return Autoconfiguration URL
	 */
	inline const std::string &url() const { return _url; }

	/**
	 * Refresh configuration from autoconf URL
	 */
	void refreshConfiguration();

private:
	void __CBautoconfHandler(const std::string &lastModified,const std::string &body);
	static bool _CBautoconfHandler(Http::Request *req,void *arg,const std::string &url,int code,const std::map<std::string,std::string> &headers,const std::string &body);

	const RuntimeEnvironment *_r;

	volatile uint64_t _lastAutoconfigure;

	std::string _lastAutoconfigureLastModified;
	std::string _url;
	Mutex _autoconfigureLock;

	std::map< uint64_t,SharedPtr<Network> > _networks;
	Mutex _networks_m;
};

} // namespace ZeroTier

#endif
