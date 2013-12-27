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

#ifndef ZT_SOFTWAREUPDATER_HPP
#define ZT_SOFTWAREUPDATER_HPP

#include <stdint.h>

#include "Constants.hpp"
#include "Mutex.hpp"
#include "Utils.hpp"
#include "HttpClient.hpp"
#include "Defaults.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Software updater
 */
class SoftwareUpdater
{
public:
	SoftwareUpdater(const RuntimeEnvironment *renv);
	~SoftwareUpdater();

	/**
	 * Called on each version message from a peer
	 *
	 * If a peer has a newer version, that causes an update to be started.
	 *
	 * @param vmaj Peer's major version
	 * @param vmin Peer's minor version
	 * @param rev Peer's revision
	 */
	inline void sawRemoteVersion(unsigned int vmaj,unsigned int vmin,unsigned int rev)
	{
		const uint64_t tmp = packVersion(vmaj,vmin,rev);
		if (tmp > _myVersion) {
			Mutex::Lock _l(_lock);
			if ((_status == UPDATE_STATUS_IDLE)&&(!_die)&&(ZT_DEFAULTS.updateLatestNfoURL.length())) {
				const uint64_t now = Utils::now();
				if ((now - _lastUpdateAttempt) >= ZT_UPDATE_MIN_INTERVAL) {
					_lastUpdateAttempt = now;
					_status = UPDATE_STATUS_GETTING_NFO;
					HttpClient::GET(ZT_DEFAULTS.updateLatestNfoURL,HttpClient::NO_HEADERS,ZT_UPDATE_HTTP_TIMEOUT,&_cbHandleGetLatestVersionInfo,this);
				}
			}
		}
	}

	/**
	 * Check for updates now regardless of last check time or version
	 *
	 * This only starts a check if one is not in progress. Otherwise it does
	 * nothing.
	 */
	inline void checkNow()
	{
		Mutex::Lock _l(_lock);
		if (_status == UPDATE_STATUS_IDLE) {
			_lastUpdateAttempt = Utils::now();
			_status = UPDATE_STATUS_GETTING_NFO;
			HttpClient::GET(ZT_DEFAULTS.updateLatestNfoURL,HttpClient::NO_HEADERS,ZT_UPDATE_HTTP_TIMEOUT,&_cbHandleGetLatestVersionInfo,this);
		}
	}

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

private:
	static void _cbHandleGetLatestVersionInfo(void *arg,int code,const std::string &url,bool onDisk,const std::string &body);
	static void _cbHandleGetLatestVersionBinary(void *arg,int code,const std::string &url,bool onDisk,const std::string &body);

	const RuntimeEnvironment *_r;
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
