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

#include <algorithm>
#include <utility>

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
	char memberKey[128],nwids[24],addrs[16],nwKey[128],revKey[128];
	Dictionary memberRecord;
	std::string revision,tmps2;

	Mutex::Lock _l(_lock);

	Utils::snprintf(nwids,sizeof(nwids),"%.16llx",(unsigned long long)nwid);
	Utils::snprintf(addrs,sizeof(addrs),"%.10llx",(unsigned long long)member.toInt());
	Utils::snprintf(memberKey,sizeof(memberKey),"zt1:network:%s:member:%s:~",nwids,addrs);
	Utils::snprintf(nwKey,sizeof(nwKey),"zt1:network:%s:~",nwids);
	Utils::snprintf(revKey,sizeof(revKey),"zt1:network:%s:revision",nwids);

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

	if (!_get(revKey,revision)) {
		LOG("netconf: Redis error retrieving %s",revKey);
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

bool NetworkConfigMaster::_get(const char *key,std::string &value)
{
	if (!_rc) {
		if (!_reconnect())
			return false;
	}

	redisReply *reply = (redisReply *)redisCommand(_rc,"GET %s",key);
	if (!reply) {
		if (_reconnect())
			return _get(key,value);
		return false;
	}

	if ((reply->type == REDIS_REPLY_STRING)&&(reply->str))
		value = reply->str;
	else value = "";

	freeReplyObject(reply);

	return true;
}

bool NetworkConfigMaster::_smembers(const char *key,std::vector<std::string> &sdata)
{
	if (!_rc) {
		if (!_reconnect())
			return false;
	}

	redisReply *reply = (redisReply *)redisCommand(_rc,"SMEMBERS %s",key);
	if (!reply) {
		if (_reconnect())
			return _smembers(key,sdata);
		return false;
	}

	sdata.clear();
	if (reply->type == REDIS_REPLY_ARRAY) {
		for(long i=0;i<reply->elements;++i) {
			if (reply->element[i]->str)
				sdata.push_back(reply->element[i]->str);
		}
	}

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
	char memberKey[256],nwids[24],addrs[16],tss[24],nwKey[256],revKey[128],abKey[128],ipaKey[128];
	Dictionary networkRecord,memberRecord,nc;
	std::string revision;

	Utils::snprintf(memberKey,sizeof(memberKey),"zt1:network:%s:member:%s:~",nwids,addrs);
	Utils::snprintf(nwids,sizeof(nwids),"%.16llx",(unsigned long long)nwid);
	Utils::snprintf(addrs,sizeof(addrs),"%.10llx",(unsigned long long)member.toInt());
	Utils::snprintf(nwKey,sizeof(nwKey),"zt1:network:%s:~",nwids);
	Utils::snprintf(revKey,sizeof(revKey),"zt1:network:%s:revision",nwids);
	Utils::snprintf(abKey,sizeof(revKey),"zt1:network:%s:activeBridges",nwids);
	Utils::snprintf(ipaKey,sizeof(revKey),"zt1:network:%s:ipAssignments",nwids);

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

	if (!_get(revKey,revision)) {
		LOG("netconf: Redis error retrieving %s",revKey);
		return false;
	}
	if (!revision.length())
		revision = "0";

	bool isPrivate = networkRecord.getBoolean("private",true);
	ts = Utils::now();
	Utils::snprintf(tss,sizeof(tss),"%llx",ts);

	// Core configuration
	nc[ZT_NETWORKCONFIG_DICT_KEY_TIMESTAMP] = tss;
	nc[ZT_NETWORKCONFIG_DICT_KEY_NETWORK_ID] = nwids;
	nc[ZT_NETWORKCONFIG_DICT_KEY_ISSUED_TO] = addrs;
	nc[ZT_NETWORKCONFIG_DICT_KEY_PRIVATE] = isPrivate ? "1" : "0";
	nc[ZT_NETWORKCONFIG_DICT_KEY_NAME] = networkRecord.get("name",nwids);
	nc[ZT_NETWORKCONFIG_DICT_KEY_DESC] = networkRecord.get("desc","");
	nc[ZT_NETWORKCONFIG_DICT_KEY_ENABLE_BROADCAST] = networkRecord.getBoolean("enableBroadcast",true) ? "1" : "0";
	nc[ZT_NETWORKCONFIG_DICT_KEY_ALLOW_PASSIVE_BRIDGING] = networkRecord.getBoolean("allowPassiveBridging",false) ? "1" : "0";
	nc[ZT_NETWORKCONFIG_DICT_KEY_ALLOWED_ETHERNET_TYPES] = networkRecord.get("etherTypes","");

	// Multicast options
	nc[ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_RATES] = networkRecord.get("multicastRates","");
	uint64_t ml = networkRecord.getHexUInt("multicastLimit",0);
	if (ml > 0)
		nc.setHex(ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_LIMIT,ml);

	// Active bridge configuration
	{
		std::string activeBridgeList;
		std::vector<std::string> activeBridgeSet;
		if (!_smembers(abKey,activeBridgeSet)) {
			LOG("netconf: Redis error retrieving set %s",abKey);
			return false;
		}
		std::sort(activeBridgeSet.begin(),activeBridgeSet.end());
		for(std::vector<std::string>::const_iterator i(activeBridgeSet.begin());i!=activeBridgeSet.end();++i) {
			if (i->length() == 10) {
				if (activeBridgeList.length() > 0)
					activeBridgeList.push_back(',');
				activeBridgeList.append(*i);
			}
		}
		if (activeBridgeList.length() > 0)
			nc[ZT_NETWORKCONFIG_DICT_KEY_ACTIVE_BRIDGES] = activeBridgeList;
	}

	// IP address assignment and auto-assign using the ZeroTier-internal mechanism (not DHCP, etc.)
	{
		std::string ipAssignments(memberRecord.get("ipAssignments",""));

		// Get sorted, separated lists of IPv4 and IPv6 IP address assignments already present
		std::vector<InetAddress> ip4s,ip6s;
		{
			std::vector<std::string> ips(Utils::split(ipAssignments.c_str(),",","",""));
			for(std::vector<std::string>::iterator i(ips.begin());i!=ips.end();++i) {
				InetAddress a(*i);
				if (a.isV4())
					ip4s.push_back(a);
				else if (a.isV6())
					ip6s.push_back(a);
			}
		}
		std::sort(ip4s.begin(),ip4s.end());
		std::unique(ip4s.begin(),ip4s.end());
		std::sort(ip6s.begin(),ip6s.end());
		std::unique(ip6s.begin(),ip6s.end());

		// If IPv4 assignment mode is 'zt', send them to the client
		if (networkRecord.get("v4AssignMode","") == "zt") {
			// If we have no IPv4 addresses and we have an assignment pool, auto-assign
			if (ip4s.empty()) {
				InetAddress v4AssignPool(networkRecord.get("v4AssignPool",""));
				uint32_t pnet = Utils::ntoh(*((const uint32_t *)v4AssignPool.rawIpData()));
				unsigned int pbits = v4AssignPool.netmaskBits();

				if ((v4AssignPool.isV4())&&(pbits > 0)&&(pbits < 32)&&(pnet != 0)) {
					uint32_t pmask = 0xffffffff << (32 - pbits); // netmask over network part
					uint32_t invmask = ~pmask; // netmask over "random" part

					// Begin exploring the IP space by generating an IP from the ZeroTier address
					uint32_t first = (((uint32_t)(member.toInt() & 0xffffffffULL)) & invmask) | (pnet & pmask);
					if ((first & 0xff) == 0)
						first |= 1;
					else if ((first & 0xff) == 0xff)
						first &= 0xfe;

					// Start by trying this first IP
					uint32_t abcd = first;

					InetAddress ip;
					bool gotone = false;
					unsigned long sanityCounter = 0;
					do {
						// Convert to IPv4 InetAddress
						uint32_t abcdNetworkByteOrder = Utils::hton(abcd);
						ip.set(&abcdNetworkByteOrder,4,pbits);

						// Is 'ip' already assigned to another node?
						std::string assignment;
						if (!_hget(ipaKey,ip.toString().c_str(),assignment)) {
							LOG("netconf: Redis error checking IP allocation in %s",ipaKey);
							return false;
						}
						if ((assignment.length() != 10)||(assignment == member.toString())) {
							gotone = true;
							break; // not taken!
						}

						// If we made it here, the IP was taken so increment and mask and try again
						++abcd;
						abcd &= invmask;
						abcd |= (pnet & pmask);
						if ((abcd & 0xff) == 0)
							abcd |= 1;
						else if ((abcd & 0xff) == 0xff)
							abcd &= 0xfe;

						// Don't spend insane amounts of time here -- if we have to try this hard, the user
						// needs to allocate a larger IP block.
						if (++sanityCounter >= 65535)
							break;
					} while (abcd != first); // keep going until we loop back around to 'first'

					// If we got one, add to IP list and claim in database
					if (gotone) {
						ip4s.push_back(ip);
						_hset(ipaKey,ip.toString().c_str(),member.toString().c_str());
						if (ipAssignments.length() > 0)
							ipAssignments.push_back(',');
						ipAssignments.append(ip.toString());
						_hset(memberKey,"ipAssignments",ipAssignments.c_str());
					} else {
						LOG("netconf: failed to allocate IP in %s for %s in network %s, need a larger pool!",v4AssignPool.toString().c_str(),addrs,nwids);
					}
				}
			}

			// Create comma-delimited list to send to client
			std::string v4s;
			for(std::vector<InetAddress>::iterator i(ip4s.begin());i!=ip4s.end();++i) {
				if (v4s.length() > 0)
					v4s.push_back(',');
				v4s.append(i->toString());
			}
			if (v4s.length())
				nc[ZT_NETWORKCONFIG_DICT_KEY_IPV4_STATIC] = v4s;
		}

		if (networkRecord.get("v6AssignMode","") == "zt") {
			// TODO: IPv6 auto-assign ... not quite baked yet. :)

			std::string v6s;
			for(std::vector<InetAddress>::iterator i(ip6s.begin());i!=ip6s.end();++i) {
				if (v6s.length() > 0)
					v6s.push_back(',');
				v6s.append(i->toString());
			}
			if (v6s.length())
				nc[ZT_NETWORKCONFIG_DICT_KEY_IPV6_STATIC] = v6s;
		}
	}

	// If this is a private network, generate a signed certificate of membership
	if (isPrivate) {
		CertificateOfMembership com(Utils::strToU64(revision.c_str()),1,nwid,member);
		if (com.sign(RR->identity)) // basically can't fail unless our identity is invalid
			nc[ZT_NETWORKCONFIG_DICT_KEY_CERTIFICATE_OF_MEMBERSHIP] = com.toString();
		else {
			LOG("netconf: failure signing certificate (identity problem?)");
			return false;
		}
	}

	// Sign netconf dictionary itself
	if (!nc.sign(RR->identity)) {
		LOG("netconf: failure signing dictionary (identity problem?)");
		return false;
	}

	// Convert to string-serialized form into result paramter
	netconf = nc.toString();

	// Record new netconf in database for re-use on subsequent repeat queries
	{
		Dictionary upd;
		upd["netconf"] = netconf;
		upd["netconfTimestamp"] = tss;
		upd["netconfRevision"] = revision;
		if (!_hmset(memberKey,upd)) {
			LOG("netconf: Redis error writing to key %s",memberKey);
			return false;
		}
	}

	return true;
}

} // namespace ZeroTier

#endif // ZT_ENABLE_NETCONF_MASTER
