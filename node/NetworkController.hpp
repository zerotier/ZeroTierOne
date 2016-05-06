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

#ifndef ZT_NETWORKCONFIGMASTER_HPP
#define ZT_NETWORKCONFIGMASTER_HPP

#include <stdint.h>

#include "Constants.hpp"
#include "InetAddress.hpp"
#include "Address.hpp"
#include "Identity.hpp"
#include "NetworkConfigRequestMetaData.hpp"
#include "Buffer.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Interface for network controller implementations
 */
class NetworkController
{
public:
	/**
	 * Return value of doNetworkConfigRequest
	 */
	enum ResultCode
	{
		NETCONF_QUERY_OK = 0,
		NETCONF_QUERY_OBJECT_NOT_FOUND = 1,
		NETCONF_QUERY_ACCESS_DENIED = 2,
		NETCONF_QUERY_INTERNAL_SERVER_ERROR = 3,
		NETCONF_QUERY_IGNORE = 4
	};

	NetworkController() {}
	virtual ~NetworkController() {}

	/**
	 * Handle a network config request, sending replies if necessary
	 *
	 * This call is permitted to block, and may be called concurrently from more
	 * than one thread. Implementations must use locks if needed.
	 *
	 * On internal server errors, the 'error' field in result can be filled in
	 * to indicate the error.
	 *
	 * @param fromAddr Originating wire address or null address if packet is not direct (or from self)
	 * @param signingId Identity that should be used to sign results -- must include private key
	 * @param identity Originating peer ZeroTier identity
	 * @param nwid 64-bit network ID
	 * @param metaData Meta-data bundled with request (if any)
	 * @param result Buffer to receive serialized network configuration data (any existing data in buffer is preserved)
	 * @return Returns NETCONF_QUERY_OK if result dictionary is valid, or an error code on error
	 */
	virtual NetworkController::ResultCode doNetworkConfigRequest(
		const InetAddress &fromAddr,
		const Identity &signingId,
		const Identity &identity,
		uint64_t nwid,
		const NetworkConfigRequestMetaData &metaData,
		Buffer<8194> &result) = 0;
};

} // namespace ZeroTier

#endif
