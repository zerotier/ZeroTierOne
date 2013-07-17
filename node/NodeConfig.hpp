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
#include <vector>
#include <stdexcept>

#include <stdint.h>
#include "SharedPtr.hpp"
#include "Network.hpp"
#include "Utils.hpp"
#include "Http.hpp"
#include "UdpSocket.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Node configuration endpoint
 *
 * Packet format for local UDP configuration packets:
 *  [8] random initialization vector
 *  [16] first 16 bytes of HMAC-SHA-256 of payload
 *  [4] arbitrary tag, echoed in response
 *  [...] payload
 *
 * For requests, the payload consists of a single ASCII command. For
 * responses, the payload consists of one or more response lines delimited
 * by NULL (0) characters. The tag field is replicated in the result
 * packet.
 *
 * TODO: further document use of keys, encryption...
 */
class NodeConfig
{
public:
	/**
	 * @param renv Runtime environment
	 * @param authToken Configuration authentication token
	 * @throws std::runtime_error Unable to bind to local control port
	 */
	NodeConfig(const RuntimeEnvironment *renv,const char *authToken)
		throw(std::runtime_error);

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
	void whackAllTaps();

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
	 * Execute a command
	 *
	 * @param command Command and arguments separated by whitespace (must already be trimmed of CR+LF, etc.)
	 * @return One or more command results (lines of output)
	 */
	std::vector<std::string> execute(const char *command);

private:
	static void _CBcontrolPacketHandler(UdpSocket *sock,void *arg,const InetAddress &remoteAddr,const void *data,unsigned int len);

	const RuntimeEnvironment *_r;

	const std::string _authToken;
	unsigned char _keys[64]; // Salsa20 key, HMAC key

	UdpSocket _controlSocket;
	std::map< uint64_t,SharedPtr<Network> > _networks;
	Mutex _networks_m;
};

} // namespace ZeroTier

#endif
