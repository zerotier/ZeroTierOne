/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
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

#ifndef ZT_NETWORKCONFIGMASTER_HPP
#define ZT_NETWORKCONFIGMASTER_HPP

#include <stdint.h>

#include "Constants.hpp"
#include "InetAddress.hpp"
#include "Dictionary.hpp"
#include "Address.hpp"
#include "Identity.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Interface for network configuration (netconf) master implementations
 */
class NetworkConfigMaster
{
public:
	/**
	 * Return value of doNetworkConfigRequest
	 */
	enum ResultCode
	{
		NETCONF_QUERY_OK = 0,
		NETCONF_QUERY_OK_BUT_NOT_NEWER = 1,
		NETCONF_QUERY_OBJECT_NOT_FOUND = 2,
		NETCONF_QUERY_ACCESS_DENIED = 3,
		NETCONF_QUERY_INTERNAL_SERVER_ERROR = 4
	};

	NetworkConfigMaster() {}
	virtual ~NetworkConfigMaster() {}

	/**
	 * Handle a network config request, sending replies if necessary
	 *
	 * This call is permitted to block, and may be called concurrently from more
	 * than one thread. Implementations must use locks if needed.
	 *
	 * On internal server errors, the 'error' field in result can be filled in
	 * to indicate the error.
	 *
	 * @param fromAddr Originating IP address
	 * @param packetId 64-bit packet ID
	 * @param member Originating peer ZeroTier identity
	 * @param nwid 64-bit network ID
	 * @param metaData Meta-data bundled with request (empty if none)
	 * @param haveRevision Network revision ID sent by requesting peer or 0 if none
	 * @param result Dictionary to receive resulting signed netconf on success
	 * @return Returns NETCONF_QUERY_OK if result dictionary is valid, or an error code on error
	 */
	virtual NetworkConfigMaster::ResultCode doNetworkConfigRequest(
		const InetAddress &fromAddr,
		uint64_t packetId,
		const Identity &identity,
		uint64_t nwid,
		const Dictionary &metaData,
		uint64_t haveRevision,
		Dictionary &result) = 0;
};

} // namespace ZeroTier

#endif
