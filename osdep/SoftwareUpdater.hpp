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

#ifndef ZT_SOFTWAREUPDATER_HPP
#define ZT_SOFTWAREUPDATER_HPP

#include <stdint.h>

#include <string>

#include "../node/Constants.hpp"
#include "../node/Mutex.hpp"
#include "../node/Address.hpp"

#include "HttpClient.hpp"

/**
 * Delay between fetches of the root topology update URL
 *
 * 86400000 = check once every 24 hours (this doesn't change often)
 */
#define ZT_UPDATE_ROOT_TOPOLOGY_CHECK_INTERVAL 86400000

/**
 * Minimum interval between attempts to do a software update
 */
#define ZT_UPDATE_MIN_INTERVAL 120000

/**
 * Maximum interval between checks for new versions
 */
#define ZT_UPDATE_MAX_INTERVAL 7200000

/**
 * Software update HTTP timeout in seconds
 */
#define ZT_UPDATE_HTTP_TIMEOUT 120

namespace ZeroTier {

/**
 * Software updater
 */
class SoftwareUpdater
{
public:
	SoftwareUpdater();
	~SoftwareUpdater();

	/**
	 * Remove old updates in updates.d
	 */
	void cleanOldUpdates();

	/**
	 * Called on each version message from a peer
	 *
	 * If a peer has a newer version, that causes an update to be started.
	 *
	 * @param vmaj Peer's major version
	 * @param vmin Peer's minor version
	 * @param rev Peer's revision
	 */
	void sawRemoteVersion(unsigned int vmaj,unsigned int vmin,unsigned int rev);

	/**
	 * Check for updates now regardless of last check time or version
	 *
	 * This only starts a check if one is not in progress. Otherwise it does
	 * nothing.
	 */
	void checkNow();

	/**
	 * Check for updates now if it's been longer than ZT_UPDATE_MAX_INTERVAL
	 *
	 * This is called periodically from the main loop.
	 */
	inline void checkIfMaxIntervalExceeded(uint64_t now)
	{
		if ((now - _lastUpdateAttempt) >= ZT_UPDATE_MAX_INTERVAL)
			checkNow();
	}

	/**
	 * Pack three-component version into a 64-bit integer
	 *
	 * @param vmaj Major version (0..65535)
	 * @param vmin Minor version (0..65535)
	 * @param rev Revision (0..65535)
	 * @return Version packed into an easily comparable 64-bit integer
	 */
	static inline uint64_t packVersion(unsigned int vmaj,unsigned int vmin,unsigned int rev)
		throw()
	{
		return ( ((uint64_t)(vmaj & 0xffff) << 32) | ((uint64_t)(vmin & 0xffff) << 16) | (uint64_t)(rev & 0xffff) );
	}

	/**
	 * Parse NFO data from .nfo file on software update site
	 *
	 * The first argument is the NFO data, and all the remaining arguments are
	 * result parameters to be filled with results. If an error is returned the
	 * results in the parameters should be considered undefined.
	 *
	 * @param nfo NFO data
	 * @param vMajor Result: major version
	 * @param vMinor Result: minor version
	 * @param vRevision Result: revision number
	 * @param signedBy Result: signing identity
	 * @param signature Result: Ed25519 signature data
	 * @param url Result: URL of update binary
	 * @return NULL on success or error message on failure
	 */
	static const char *parseNfo(
		const char *nfoText,
		unsigned int &vMajor,
		unsigned int &vMinor,
		unsigned int &vRevision,
		Address &signedBy,
		std::string &signature,
		std::string &url);

	/**
	 * Validate an update once downloaded
	 *
	 * This obtains the identity corresponding to the address from the compiled-in
	 * list of valid signing identities.
	 *
	 * @param data Update data
	 * @param len Length of update data
	 * @param signedBy Signing authority address
	 * @param signature Signing authority signature
	 * @return True on validation success, false if rejected
	 */
	static bool validateUpdate(
		const void *data,
		unsigned int len,
		const Address &signedBy,
		const std::string &signature);

private:
	static void _cbHandleGetLatestVersionInfo(void *arg,int code,const std::string &url,const std::string &body);
	static void _cbHandleGetLatestVersionBinary(void *arg,int code,const std::string &url,const std::string &body);

	HttpClient httpClient;
	const uint64_t _myVersion;
	volatile uint64_t _lastUpdateAttempt;
	volatile enum {
		UPDATE_STATUS_IDLE,
		UPDATE_STATUS_GETTING_NFO,
		UPDATE_STATUS_GETTING_FILE
	} _status;
	volatile bool _die;
	Address _signedBy;
	std::string _signature;
	Mutex _lock;
};

} // namespace ZeroTier

#endif
