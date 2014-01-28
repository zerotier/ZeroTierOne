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
#include "UdpSocket.hpp"
#include "Buffer.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Maximum size of a packet for node configuration
 */
#define ZT_NODECONFIG_MAX_PACKET_SIZE 4096

/**
 * Node configuration endpoint
 */
class NodeConfig
{
public:
	/**
	 * @param renv Runtime environment
	 * @param authToken Configuration authentication token
	 * @param controlPort Control port for local control packet I/O
	 * @throws std::runtime_error Unable to bind to local control port
	 */
	NodeConfig(const RuntimeEnvironment *renv,const char *authToken,unsigned int controlPort);

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
	 * Perform cleanup and possibly update saved state
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
	 * @return Set of network tap device names
	 */
	inline std::set<std::string> networkTapDeviceNames() const
	{
		std::set<std::string> tapDevs;
		Mutex::Lock _l(_networks_m);
		for(std::map< uint64_t,SharedPtr<Network> >::const_iterator n(_networks.begin());n!=_networks.end();++n) {
			std::string dn(n->second->tapDeviceName());
			if (dn.length())
				tapDevs.insert(dn);
		}
		return tapDevs;
	}

	/**
	 * Execute a command
	 *
	 * @param command Command and arguments separated by whitespace (must already be trimmed of CR+LF, etc.)
	 * @return One or more command results (lines of output)
	 */
	std::vector<std::string> execute(const char *command);

	/**
	 * Armor payload for control bus
	 *
	 * Note that no single element of payload can be longer than the max packet
	 * size. If this occurs out_of_range is thrown.
	 *
	 * @param key 32 byte key
	 * @param conversationId 32-bit conversation ID (bits beyond 32 are ignored)
	 * @param payload One or more strings to encode in packet
	 * @return One or more transport armored packets (if payload too big)
	 * @throws std::out_of_range An element of payload is too big
	 */
	static std::vector< Buffer<ZT_NODECONFIG_MAX_PACKET_SIZE> > encodeControlMessage(const void *key,unsigned long conversationId,const std::vector<std::string> &payload);

	/**
	 * Decode a packet from the control bus
	 *
	 * Note that 'payload' is appended to. Existing data is not cleared.
	 *
	 * @param key 32 byte key
	 * @param data Packet data
	 * @param len Packet length
	 * @param conversationId Result parameter filled with conversation ID on success
	 * @param payload Result parameter to which results are appended
	 * @return True on success, false on invalid packet or packet that failed authentication
	 */
	static bool decodeControlMessagePacket(const void *key,const void *data,unsigned int len,unsigned long &conversationId,std::vector<std::string> &payload);

private:
	static void _CBcontrolPacketHandler(UdpSocket *sock,void *arg,const InetAddress &remoteAddr,const void *data,unsigned int len);

	const RuntimeEnvironment *_r;

	unsigned char _controlSocketKey[32];
	UdpSocket _controlSocket;

	std::map< uint64_t,SharedPtr<Network> > _networks;
	Mutex _networks_m;
};

} // namespace ZeroTier

#endif
