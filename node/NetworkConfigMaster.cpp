/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2015  ZeroTier Networks
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

#include "Constants.hpp"

#ifdef ZT_ENABLE_NETCONF_MASTER

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>

#include "NetworkConfigMaster.hpp"
#include "RuntimeEnvironment.hpp"
#include "Switch.hpp"
#include "Packet.hpp"
#include "NetworkConfig.hpp"
#include "Utils.hpp"
#include "Node.hpp"
#include "Logger.hpp"

// Redis timeout in seconds
#define ZT_NETCONF_REDIS_TIMEOUT 10

namespace ZeroTier {

NetworkConfigMaster::NetworkConfigMaster(
	const RuntimeEnvironment *renv,
	const char *redisHost,
	unsigned int redisPort,
	const char *redisPassword,
	unsigned int redisDatabaseNumber) :
	_lock(),
	_redisHost(redisHost),
	_redisPassword((redisPassword) ? redisPassword : ""),
	_redisPort(redisPort),
	_redisDatabaseNumber(redisDatabaseNumber),
	RR(renv),
	_rc((redisContext *)0)
{
}

NetworkConfigMaster::~NetworkConfigMaster()
{
	Mutex::Lock _l(_lock);
	if (_rc)
		redisFree(_rc);
}

void NetworkConfigMaster::doNetworkConfigRequest(
	uint64_t packetId,
	const Address &from,
	uint64_t nwid,
	const Dictionary &metaData,
	uint64_t haveTimestamp)
{
}

bool NetworkConfigMaster::_reconnect()
{
	struct timeval tv;

	if (_rc)
		redisFree(_rc);

	tv.tv_sec = ZT_NETCONF_REDIS_TIMEOUT;
	tv.tv_usec = 0;
	_rc = redisConnectWithTimeout(_redisHost.c_str(),_redisPort,&tv);
	if (!_rc)
		return false;
	if (_rc->err) {
		redisFree(_rc);
		_rc = (redisContext *)0;
		return false;
	}
	redisSetTimeout(_rc,&tv); // necessary???

	// TODO: support AUTH and SELECT !!!

	return true;
}

bool NetworkConfigMaster::_hgetall(const char *key,std::map<std::string,std::string> &hdata)
{
	if (!_rc) {
		if (!_reconnect())
			return false;
	}

	redisReply *reply = (redisReply *)redisCommand(_rc,"HGETALL %s",key);
	if (!reply) {
		if (_reconnect())
			return _hgetall(key,hdata);
		return false;
	}

	hdata.clear();
	if (reply->type == REDIS_REPLY_ARRAY) {
		for(long i=0;i<reply->elements;) {
			try {
				const char *k = reply->elements[i]->str;
				if (++i >= reply->elements)
					break;
				if ((k)&&(reply->elements[i]->str))
					hdata[k] = reply->elements[i]->str;
				++i;
			} catch ( ... ) {
				break; // memory safety
			}
		}
	}

	freeReplyObject(reply);

	return true;
}

bool NetworkConfigMaster::_hmset(const char *key,const std::map<std::string,std::string> &hdata)
{
	const const char *hargv[1024];

	if (!hdata.size())
		return true;

	if (!_rc) {
		if (!_reconnect())
			return false;
	}

	hargv[0] = "HMSET";
	hargv[1] = key;
	int hargc = 2;
	for(std::map<std::string,std::string>::const_iterator i(hdata.begin());i!=hdata.end();++i) {
		if (hargc >= 1024)
			break;
		hargv[hargc++] = i->first.c_str();
		hargv[hargc++] = i->second.c_str();
	}

	redisReply *reply = (redisReply *)redisCommandArgv(_rc,hargc,hargv,(const size_t *)0);
	if (!reply) {
		if (_reconnect())
			return _hmset(key,hdata);
		return false;
	}

	if (reply->type == REDIS_REPLY_ERROR) {
		freeReplyObject(reply);
		return false;
	}

	freeReplyObject(reply);

	return true;
}

bool NetworkConfigMaster::_hget(const char *key,const char *hashKey,std::string &value)
{
	if (!_rc) {
		if (!_reconnect())
			return false;
	}

	redisReply *reply = (redisReply *)redisCommand(_rc,"HGET %s %s",key,hashKey);
	if (!reply) {
		if (_reconnect())
			return _hget(key,hashKey,value);
		return false;
	}

	if (reply->type == REDIS_REPLY_STRING)
		value = reply->str;
	else value = "";

	freeReplyObject(reply);

	return true;
}

bool NetworkConfigMaster::_hset(const char *key,const char *hashKey,const char *value)
{
	if (!_rc) {
		if (!_reconnect())
			return false;
	}

	redisReply *reply = (redisReply *)redisCommand(_rc,"HSET %s %s %s",key,hashKey,value);
	if (!reply) {
		if (_reconnect())
			return _hset(key,hashKey,value);
		return false;
	}

	if (reply->type == REDIS_REPLY_ERROR) {
		freeReplyObject(reply);
		return false;
	}

	freeReplyObject(reply);

	return true;
}

} // namespace ZeroTier

#endif // ZT_ENABLE_NETCONF_MASTER
