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
#include "NetworkConfigMaster.hpp"

#ifdef ZT_ENABLE_NETCONF_MASTER

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>

#include "RuntimeEnvironment.hpp"
#include "Switch.hpp"
#include "Packet.hpp"
#include "NetworkConfig.hpp"
#include "Utils.hpp"
#include "Node.hpp"
#include "Logger.hpp"
#include "Topology.hpp"
#include "Peer.hpp"
#include "CertificateOfMembership.hpp"

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

void NetworkConfigMaster::doNetworkConfigRequest(const InetAddress &fromAddr,uint64_t packetId,const Address &member,uint64_t nwid,const Dictionary &metaData,uint64_t haveTimestamp)
{
	char memberKey[256],nwids[24],addrs[16],nwKey[256];
	Dictionary memberRecord;
	std::string revision,tmps2;

	Mutex::Lock _l(_lock);

	Utils::snprintf(nwids,sizeof(nwids),"%.16llx",(unsigned long long)nwid);
	Utils::snprintf(addrs,sizeof(addrs),"%.10llx",(unsigned long long)member.toInt());
	Utils::snprintf(memberKey,sizeof(memberKey),"zt1:network:%s:member:%s:~",nwids,addrs);
	Utils::snprintf(nwKey,sizeof(nwKey),"zt1:network:%s:~",nwids);

	TRACE("netconf: request from %s for %s (if newer than %llu)",addrs,nwids,(unsigned long long)haveTimestamp);

	if (!_hget(nwKey,"id",tmps2)) {
		LOG("netconf: Redis error retrieving %s/id",nwKey);
		return;
	}
	if (tmps2 != nwids) {
		TRACE("netconf: network %s not found",nwids);
		Packet outp(member,RR->identity.address(),Packet::VERB_ERROR);
		outp.append((unsigned char)Packet::VERB_NETWORK_CONFIG_REQUEST);
		outp.append(packetId);
		outp.append((unsigned char)Packet::ERROR_OBJ_NOT_FOUND);
		outp.append(nwid);
		RR->sw->send(outp,true);
		return;
	}

	if (!_hget(nwKey,"revision",revision)) {
		LOG("netconf: Redis error retrieving %s/revision",nwKey);
		return;
	}
	if (!revision.length())
		revision = "0";

	if (!_hgetall(memberKey,memberRecord)) {
		LOG("netconf: Redis error retrieving %s",memberKey);
		return;
	}

	if ((memberRecord.size() == 0)||(memberRecord.get("id","") != addrs)||(memberRecord.get("nwid","") != nwids)) {
		if (!_initNewMember(nwid,member,metaData,memberRecord))
			return;
	}

	if (memberRecord.getBoolean("authorized")) {
		uint64_t ts = memberRecord.getHexUInt("netconfTimestamp",0);
		std::string netconf(memberRecord.get("netconf",""));

		Dictionary upd;
		upd.setHex("netconfClientTimestamp",haveTimestamp);
		if (fromAddr)
			upd.set("lastAt",fromAddr.toString());
		upd.setHex("lastSeen",Utils::now());
		_hmset(memberKey,upd);

		if (((ts == 0)||(netconf.length() == 0))||(memberRecord.get("netconfRevision","") != revision)) {
			if (!_generateNetconf(nwid,member,metaData,netconf,ts))
				return;
		}

		if (ts > haveTimestamp) {
			TRACE("netconf: sending %u bytes of netconf data to %s",netconf.length(),addrs);
			Packet outp(member,RR->identity.address(),Packet::VERB_OK);
			outp.append((unsigned char)Packet::VERB_NETWORK_CONFIG_REQUEST);
			outp.append(packetId);
			outp.append(nwid);
			outp.append((uint16_t)netconf.length());
			outp.append(netconf.data(),netconf.length());
			outp.compress();
			if (outp.size() > ZT_PROTO_MAX_PACKET_LENGTH) { // sanity check -- this would be weird
				TRACE("netconf: compressed packet exceeds ZT_PROTO_MAX_PACKET_LENGTH!");
				return;
			}
			RR->sw->send(outp,true);
		}
	} else {
		TRACE("netconf: access denied for %s on %s",addrs,nwids);
		Packet outp(member,RR->identity.address(),Packet::VERB_ERROR);
		outp.append((unsigned char)Packet::VERB_NETWORK_CONFIG_REQUEST);
		outp.append(packetId);
		outp.append((unsigned char)Packet::ERROR_NETWORK_ACCESS_DENIED_);
		outp.append(nwid);
		RR->sw->send(outp,true);
	}
}

bool NetworkConfigMaster::_reconnect()
{
	struct timeval tv;

	if (_rc)
		redisFree(_rc);

	tv.tv_sec = ZT_NETCONF_REDIS_TIMEOUT;
	tv.tv_usec = 0;
	_rc = redisConnectWithTimeout(_redisHost.c_str(),_redisPort,tv);
	if (!_rc)
		return false;
	if (_rc->err) {
		redisFree(_rc);
		_rc = (redisContext *)0;
		return false;
	}
	redisSetTimeout(_rc,tv); // necessary???

	// TODO: support AUTH and SELECT !!!

	return true;
}

bool NetworkConfigMaster::_hgetall(const char *key,Dictionary &hdata)
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
				const char *k = reply->element[i]->str;
				if (++i >= reply->elements)
					break;
				if ((k)&&(reply->element[i]->str))
					hdata[k] = reply->element[i]->str;
				++i;
			} catch ( ... ) {
				break; // memory safety
			}
		}
	}

	freeReplyObject(reply);

	return true;
}

bool NetworkConfigMaster::_hmset(const char *key,const Dictionary &hdata)
{
	const char *hargv[1024];

	if (!hdata.size())
		return true;

	if (!_rc) {
		if (!_reconnect())
			return false;
	}

	hargv[0] = "HMSET";
	hargv[1] = key;
	int hargc = 2;
	for(Dictionary::const_iterator i(hdata.begin());i!=hdata.end();++i) {
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

bool NetworkConfigMaster::_initNewMember(uint64_t nwid,const Address &member,const Dictionary &metaData,Dictionary &memberRecord)
{
	char memberKey[256],nwids[24],addrs[16],nwKey[256];
	Dictionary networkRecord;

	Utils::snprintf(nwids,sizeof(nwids),"%.16llx",(unsigned long long)nwid);
	Utils::snprintf(addrs,sizeof(addrs),"%.10llx",(unsigned long long)member.toInt());
	Utils::snprintf(memberKey,sizeof(memberKey),"zt1:network:%s:member:%s:~",nwids,addrs);
	Utils::snprintf(nwKey,sizeof(nwKey),"zt1:network:%s:~",nwids);

	if (!_hgetall(nwKey,networkRecord)) {
		LOG("netconf: Redis error retrieving %s",nwKey);
		return false;
	}
	if (networkRecord.get("id","") != nwids) {
		TRACE("netconf: network %s not found (initNewMember)",nwids);
		return false;
	}

	memberRecord.clear();
	memberRecord["id"] = addrs;
	memberRecord["nwid"] = nwids;
	memberRecord["authorized"] = (networkRecord.getBoolean("private",true) ? "0" : "1"); // auto-authorize on public networks
	memberRecord.setHex("firstSeen",Utils::now());
	{
		SharedPtr<Peer> peer(RR->topology->getPeer(member));
		if (peer)
			memberRecord["identity"] = peer->identity().toString(false);
	}

	if (!_hmset(memberKey,memberRecord)) {
		LOG("netconf: Redis error storing %s for new member %s",memberKey,addrs);
		return false;
	}

	return true;
}

bool NetworkConfigMaster::_generateNetconf(uint64_t nwid,const Address &member,const Dictionary &metaData,std::string &netconf,uint64_t &ts)
{
	char memberKey[256],nwids[24],addrs[16],tss[24],nwKey[256];
	Dictionary networkRecord,memberRecord,nc;

	Utils::snprintf(nwids,sizeof(nwids),"%.16llx",(unsigned long long)nwid);
	Utils::snprintf(addrs,sizeof(addrs),"%.10llx",(unsigned long long)member.toInt());
	Utils::snprintf(memberKey,sizeof(memberKey),"zt1:network:%s:member:%s:~",nwids,addrs);
	Utils::snprintf(nwKey,sizeof(nwKey),"zt1:network:%s:~",nwids);

	if (!_hgetall(nwKey,networkRecord)) {
		LOG("netconf: Redis error retrieving %s",nwKey);
		return false;
	}
	if (networkRecord.get("id","") != nwids) {
		TRACE("netconf: network %s not found (generateNetconf)",nwids);
		return false;
	}

	if (!_hgetall(memberKey,memberRecord)) {
		LOG("netconf: Redis error retrieving %s",memberKey);
		return false;
	}

	uint64_t revision = networkRecord.getHexUInt("revision",0);
	bool isPrivate = networkRecord.getBoolean("private",true);
	ts = Utils::now();
	Utils::snprintf(tss,sizeof(tss),"%llx",ts);

	nc[ZT_NETWORKCONFIG_DICT_KEY_TIMESTAMP] = tss;
	nc[ZT_NETWORKCONFIG_DICT_KEY_NETWORK_ID] = nwids;
	nc[ZT_NETWORKCONFIG_DICT_KEY_ISSUED_TO] = addrs;
	nc[ZT_NETWORKCONFIG_DICT_KEY_PRIVATE] = isPrivate ? "1" : "0";
	nc[ZT_NETWORKCONFIG_DICT_KEY_NAME] = networkRecord.get("name",nwids);
	nc[ZT_NETWORKCONFIG_DICT_KEY_DESC] = networkRecord.get("desc","");
	nc[ZT_NETWORKCONFIG_DICT_KEY_ENABLE_BROADCAST] = networkRecord.getBoolean("enableBroadcast",true) ? "1" : "0";
	nc[ZT_NETWORKCONFIG_DICT_KEY_ALLOW_PASSIVE_BRIDGING] = networkRecord.getBoolean("allowPassiveBridging",false) ? "1" : "0";
	nc[ZT_NETWORKCONFIG_DICT_KEY_ALLOWED_ETHERNET_TYPES] = networkRecord.get("etherTypes","");
	nc[ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_RATES] = networkRecord.get("multicastRates","");

	uint64_t ml = networkRecord.getHexUInt("multicastLimit",0);
	if (ml > 0)
		nc.setHex(ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_LIMIT,ml);

	std::string activeBridgeList;
	if (activeBridgeList.length() > 0)
		nc[ZT_NETWORKCONFIG_DICT_KEY_ACTIVE_BRIDGES] = activeBridgeList;

	std::string v4s,v6s;
	if (v4s.length())
		nc[ZT_NETWORKCONFIG_DICT_KEY_IPV4_STATIC] = v4s;
	if (v6s.length())
		nc[ZT_NETWORKCONFIG_DICT_KEY_IPV6_STATIC] = v6s;

	if (isPrivate) {
		CertificateOfMembership com(revision,2,nwid,member);
		if (com.sign(RR->identity))
			nc[ZT_NETWORKCONFIG_DICT_KEY_CERTIFICATE_OF_MEMBERSHIP] = com.toString();
	}

	netconf = nc.toString();

	_hset(memberKey,"netconf",netconf.c_str());
	_hset(memberKey,"netconfTimestamp",tss);
	_hset(memberKey,"netconfRevision",networkRecord.get("revision","0").c_str());

	return true;
}

} // namespace ZeroTier

#endif // ZT_ENABLE_NETCONF_MASTER
