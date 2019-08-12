/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifndef ZT_NETWORKCONFIGMASTER_HPP
#define ZT_NETWORKCONFIGMASTER_HPP

#include <stdint.h>

#include "Constants.hpp"
#include "Dictionary.hpp"
#include "NetworkConfig.hpp"
#include "Revocation.hpp"
#include "Address.hpp"

namespace ZeroTier {

class Identity;
struct InetAddress;

/**
 * Interface for network controller implementations
 */
class NetworkController
{
public:
	enum ErrorCode
	{
		NC_ERROR_NONE = 0,
		NC_ERROR_OBJECT_NOT_FOUND = 1,
		NC_ERROR_ACCESS_DENIED = 2,
		NC_ERROR_INTERNAL_SERVER_ERROR = 3
	};

	/**
	 * Interface for sender used to send pushes and replies
	 */
	class Sender
	{
	public:
		/**
		 * Send a configuration to a remote peer
		 *
		 * @param nwid Network ID
		 * @param requestPacketId Request packet ID to send OK(NETWORK_CONFIG_REQUEST) or 0 to send NETWORK_CONFIG (push)
		 * @param destination Destination peer Address
		 * @param nc Network configuration to send
		 * @param sendLegacyFormatConfig If true, send an old-format network config
		 */
		virtual void ncSendConfig(uint64_t nwid,uint64_t requestPacketId,const Address &destination,const NetworkConfig &nc,bool sendLegacyFormatConfig) = 0;

		/**
		 * Send revocation to a node
		 *
		 * @param destination Destination node address
		 * @param rev Revocation to send
		 */
		virtual void ncSendRevocation(const Address &destination,const Revocation &rev) = 0;

		/**
		 * Send a network configuration request error
		 *
		 * @param nwid Network ID
		 * @param requestPacketId Request packet ID or 0 if none
		 * @param destination Destination peer Address
		 * @param errorCode Error code
		 */
		virtual void ncSendError(uint64_t nwid,uint64_t requestPacketId,const Address &destination,NetworkController::ErrorCode errorCode) = 0;
	};

	NetworkController() {}
	virtual ~NetworkController() {}

	/**
	 * Called when this is added to a Node to initialize and supply info
	 *
	 * @param signingId Identity for signing of network configurations, certs, etc.
	 * @param sender Sender implementation for sending replies or config pushes
	 */
	virtual void init(const Identity &signingId,Sender *sender) = 0;

	/**
	 * Handle a network configuration request
	 *
	 * @param nwid 64-bit network ID
	 * @param fromAddr Originating wire address or null address if packet is not direct (or from self)
	 * @param requestPacketId Packet ID of request packet or 0 if not initiated by remote request
	 * @param identity ZeroTier identity of originating peer
	 * @param metaData Meta-data bundled with request (if any)
	 * @return Returns NETCONF_QUERY_OK if result 'nc' is valid, or an error code on error
	 */
	virtual void request(
		uint64_t nwid,
		const InetAddress &fromAddr,
		uint64_t requestPacketId,
		const Identity &identity,
		const Dictionary<ZT_NETWORKCONFIG_METADATA_DICT_CAPACITY> &metaData) = 0;
};

} // namespace ZeroTier

#endif
