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

#ifndef ZT_REDISNETWORKCONFIGMASTER_HPP
#define ZT_REDISNETWORKCONFIGMASTER_HPP

#include <stdint.h>
#include <string>
#include <map>
#include <vector>

#include "../node/Constants.hpp"
#include "../node/NetworkConfigMaster.hpp"
#include "../node/Mutex.hpp"

#include <hiredis/hiredis.h>

// Redis timeout in seconds
#define ZT_NETCONF_REDIS_TIMEOUT 10

namespace ZeroTier {

class RedisNetworkConfigMaster : public NetworkConfigMaster
{
public:
	RedisNetworkConfigMaster(
		const Identity &signingId,
		const char *redisHost,
		unsigned int redisPort,
		const char *redisPassword,
		unsigned int redisDatabaseNumber);

	virtual ~RedisNetworkConfigMaster();

	virtual NetworkConfigMaster::ResultCode doNetworkConfigRequest(
		const InetAddress &fromAddr,
		uint64_t packetId,
		const Identity &member,
		uint64_t nwid,
		const Dictionary &metaData,
		uint64_t haveTimestamp,
		Dictionary &netconf);

private:
	// These assume _lock is locked
	bool _reconnect();
	bool _hgetall(const char *key,Dictionary &hdata);
	bool _hmset(const char *key,const Dictionary &hdata);
	bool _hget(const char *key,const char *hashKey,std::string &value);
	bool _hset(const char *key,const char *hashKey,const char *value);
	bool _get(const char *key,std::string &value);
	bool _sadd(const char *key,const char *value);
	bool _smembers(const char *key,std::vector<std::string> &sdata);

	bool _initNewMember(uint64_t nwid,const Identity &member,const Dictionary &metaData,Dictionary &memberRecord);
	bool _generateNetconf(uint64_t nwid,const Identity &member,const Dictionary &metaData,Dictionary &netconf,uint64_t &ts,std::string &errorMessage);

	Mutex _lock;

	Identity _signingId;

	std::string _redisHost;
	std::string _redisPassword;
	unsigned int _redisPort;
	unsigned int _redisDatabaseNumber;

	redisContext *_rc;
};

} // namespace ZeroTier

#endif
