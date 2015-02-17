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

#include "Constants.hpp"

#define ZT_LOCAL_CONFIG_NETCONF_REDIS_HOST "netconf.redisHost"
#define ZT_LOCAL_CONFIG_NETCONF_REDIS_PORT "netconf.redisPort"
#define ZT_LOCAL_CONFIG_NETCONF_REDIS_PORT_DEFAULT 6379
#define ZT_LOCAL_CONFIG_NETCONF_REDIS_AUTH "netconf.redisAuth"
#define ZT_LOCAL_CONFIG_NETCONF_REDIS_DBNUM "netconf.redisDatabaseNumber"
#define ZT_LOCAL_CONFIG_NETCONF_REDIS_DBNUM_DEFAULT 0

#ifdef ZT_ENABLE_NETCONF_MASTER

#include <stdint.h>
#include <string>
#include <map>
#include <vector>

#include "Address.hpp"
#include "Dictionary.hpp"
#include "Mutex.hpp"
#include "InetAddress.hpp"

#include <hiredis/hiredis.h>

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Network configuration master -- responds to NETCONF requests
 *
 * This requires the 'hiredis' C library to build.
 */
class NetworkConfigMaster
{
public:
	/**
	 * Create netconf master
	 *
	 * This doesn't connect to Redis until the first request is received.
	 *
	 * @param renv Runtime environment
	 * @param redisHost Hostname or IP of Redis server
	 * @param redisPort Redis IP port number
	 * @param redisPassword Redis AUTH password or NULL if none
	 * @param redisDatabaseNumber Redis database number (usually 0)
	 */
	NetworkConfigMaster(
		const RuntimeEnvironment *renv,
		const char *redisHost,
		unsigned int redisPort,
		const char *redisPassword,
		unsigned int redisDatabaseNumber);

	~NetworkConfigMaster();

	/**
	 * Handle a network config request, sending replies if necessary
	 *
	 * This is a blocking call, so rate is limited by Redis. It will fail
	 * and log its failure if the Redis server is not available or times out.
	 *
	 * @param fromAddr Originating IP address
	 * @param packetId 64-bit packet ID
	 * @param member Originating peer ZeroTier address
	 * @param nwid 64-bit network ID
	 * @param metaData Meta-data bundled with request (empty if none)
	 * @param haveTimestamp Timestamp requesting peer has or 0 if none or not included
	 */
	void doNetworkConfigRequest(
		const InetAddress &fromAddr,
		uint64_t packetId,
		const Address &member,
		uint64_t nwid,
		const Dictionary &metaData,
		uint64_t haveTimestamp);

private:
	// These assume _lock is locked
	bool _reconnect();
	bool _hgetall(const char *key,Dictionary &hdata);
	bool _hmset(const char *key,const Dictionary &hdata);
	bool _hget(const char *key,const char *hashKey,std::string &value);
	bool _hset(const char *key,const char *hashKey,const char *value);
	bool _get(const char *key,std::string &value);
	bool _smembers(const char *key,std::vector<std::string> &sdata);

	bool _initNewMember(uint64_t nwid,const Address &member,const Dictionary &metaData,Dictionary &memberRecord);
	bool _generateNetconf(uint64_t nwid,const Address &member,const Dictionary &metaData,std::string &netconf,uint64_t &ts);

	Mutex _lock;

	std::string _redisHost;
	std::string _redisPassword;
	unsigned int _redisPort;
	unsigned int _redisDatabaseNumber;

	const RuntimeEnvironment *RR;
	redisContext *_rc;
};

} // namespace ZeroTier

#endif // ZT_ENABLE_NETCONF_MASTER

#endif
