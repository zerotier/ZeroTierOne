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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>

#include <algorithm>
#include <utility>
#include <stdexcept>

#include "SqliteNetworkConfigMaster.hpp"
#include "../node/Utils.hpp"
#include "../node/CertificateOfMembership.hpp"
#include "../node/NetworkConfig.hpp"

// Include ZT_NETCONF_SCHEMA_SQL constant to init database
#include "netconf-schema.sql.c"

// Stored in database as schemaVersion key in Config.
// If not present, database is assumed to be empty and at the current schema version
// and this key/value is added automatically.
#define ZT_NETCONF_SQLITE_SCHEMA_VERSION 1
#define ZT_NETCONF_SQLITE_SCHEMA_VERSION_STR "1"

namespace ZeroTier {

SqliteNetworkConfigMaster::SqliteNetworkConfigMaster(const Identity &signingId,const char *dbPath) :
	_signingId(signingId),
	_dbPath(dbPath),
	_db((sqlite3 *)0),
	_lock()
{
	if (!_signingId.hasPrivate())
		throw std::runtime_error("SqliteNetworkConfigMaster signing identity must have a private key");

	if (sqlite3_open_v2(dbPath,&_db,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE,(const char *)0) != SQLITE_OK)
		throw std::runtime_error("SqliteNetworkConfigMaster cannot open database file");
	sqlite3_busy_timeout(_db,10000);

	sqlite3_stmt *s;
	for(int k=0;k<2;++k) {
		s = (sqlite3_stmt *)0;
		if ((sqlite3_prepare_v2(_db,"SELECT 'v' FROM Config WHERE 'k' = 'schemaVersion';",-1,&s,(const char **)0) != SQLITE_OK)||(!s)) {
			if (sqlite3_exec(_db,ZT_NETCONF_SCHEMA_SQL"INSERT INTO Config (k,v) VALUES ('schemaVersion',"ZT_NETCONF_SQLITE_SCHEMA_VERSION_STR");",0,0,0) != SQLITE_OK) {
				sqlite3_close(_db);
				throw std::runtime_error("SqliteNetworkConfigMaster cannot initialize database and/or insert schemaVersion into Config table");
			} else {
				// Initialized database and set schema version, so we are done.
				return;
			}
		} else break;
	}
	if (!s) {
		sqlite3_close(_db);
		throw std::runtime_error("SqliteNetworkConfigMaster unable to create prepared statement or initialize database");
	}

	// If we made it here, database was opened and prepared statement was created
	// to check schema version. Check and upgrade if needed.

	int schemaVersion = -1234;
	if (sqlite3_step(s) == SQLITE_ROW)
		schemaVersion = sqlite3_column_int(s,0);

	sqlite3_finalize(s);

	if (schemaVersion == -1234) {
		sqlite3_close(_db);
		throw std::runtime_error("SqliteNetworkConfigMaster schemaVersion not found in Config table (init failure?)");
	} else if (schemaVersion != ZT_NETCONF_SQLITE_SCHEMA_VERSION) {
		// Note -- this will eventually run auto-upgrades so this isn't how it'll work going forward
		sqlite3_close(_db);
		throw std::runtime_error("SqliteNetworkConfigMaster database schema version mismatch");
	}
}

SqliteNetworkConfigMaster::~SqliteNetworkConfigMaster()
{
	Mutex::Lock _l(_lock);
	if (_db)
		sqlite3_close(_db);
}

NetworkConfigMaster::ResultCode SqliteNetworkConfigMaster::doNetworkConfigRequest(const InetAddress &fromAddr,uint64_t packetId,const Identity &member,uint64_t nwid,const Dictionary &metaData,uint64_t haveTimestamp,Dictionary &netconf)
{
#if 0
	char memberKey[128],nwids[24],addrs[16],nwKey[128],revKey[128];
	Dictionary memberRecord;
	std::string revision,tmps2;

	Mutex::Lock _l(_lock);

	Utils::snprintf(nwids,sizeof(nwids),"%.16llx",(unsigned long long)nwid);
	Utils::snprintf(addrs,sizeof(addrs),"%.10llx",(unsigned long long)member.address().toInt());
	Utils::snprintf(memberKey,sizeof(memberKey),"zt1:network:%s:member:%s:~",nwids,addrs);
	Utils::snprintf(nwKey,sizeof(nwKey),"zt1:network:%s:~",nwids);
	Utils::snprintf(revKey,sizeof(revKey),"zt1:network:%s:revision",nwids);

	//TRACE("netconf: %s : %s if > %llu",nwids,addrs,(unsigned long long)haveTimestamp);

	// Check to make sure network itself exists and is valid
	if (!_hget(nwKey,"id",tmps2)) {
		netconf["error"] = "Sqlite error retrieving network record ID field";
		return NetworkConfigMaster::NETCONF_QUERY_INTERNAL_SERVER_ERROR;
	}
	if (tmps2 != nwids)
		return NetworkConfigMaster::NETCONF_QUERY_OBJECT_NOT_FOUND;

	// Get network revision
	if (!_get(revKey,revision)) {
		netconf["error"] = "Sqlite error retrieving network revision";
		return NetworkConfigMaster::NETCONF_QUERY_INTERNAL_SERVER_ERROR;
	}
	if (!revision.length())
		revision = "0";

	// Get network member record for this peer
	if (!_hgetall(memberKey,memberRecord)) {
		netconf["error"] = "Sqlite error retrieving member record";
		return NetworkConfigMaster::NETCONF_QUERY_INTERNAL_SERVER_ERROR;
	}

	// If there is no member record, init a new one -- for public networks this
	// auto-authorizes, and for private nets it makes the peer show up in the UI
	// so the admin can authorize or delete/hide it.
	if ((memberRecord.size() == 0)||(memberRecord.get("id","") != addrs)||(memberRecord.get("nwid","") != nwids)) {
		if (!_initNewMember(nwid,member,metaData,memberRecord)) {
			netconf["error"] = "_initNewMember() failed";
			return NetworkConfigMaster::NETCONF_QUERY_INTERNAL_SERVER_ERROR;
		}
	}

	if (memberRecord.getBoolean("authorized")) {
		// Get current netconf and netconf timestamp
		uint64_t ts = memberRecord.getUInt("netconfTimestamp",0);
		std::string netconfStr(memberRecord.get("netconf",""));
		netconf.fromString(netconfStr);

		// Update statistics for this node
		Dictionary upd;
		upd.set("netconfClientTimestamp",haveTimestamp);
		if (fromAddr)
			upd.set("lastAt",fromAddr.toString());
		upd.set("lastSeen",Utils::now());
		_hmset(memberKey,upd);

		// Attempt to generate netconf for this node if there isn't
		// one or it's not in step with the network's revision.
		if (((ts == 0)||(netconfStr.length() == 0))||(memberRecord.get("netconfRevision","") != revision)) {
			std::string errorMessage;
			if (!_generateNetconf(nwid,member,metaData,netconf,ts,errorMessage)) {
				netconf["error"] = errorMessage;
				return NetworkConfigMaster::NETCONF_QUERY_INTERNAL_SERVER_ERROR;
			}
		}

		if (ts > haveTimestamp)
			return NetworkConfigMaster::NETCONF_QUERY_OK;
		else return NetworkConfigMaster::NETCONF_QUERY_OK_BUT_NOT_NEWER;
	} else {
		return NetworkConfigMaster::NETCONF_QUERY_ACCESS_DENIED;
	}
#endif
}

bool SqliteNetworkConfigMaster::_initNewMember(uint64_t nwid,const Identity &member,const Dictionary &metaData,Dictionary &memberRecord)
{
#if 0
	char memberKey[128],nwids[24],addrs[16],nwKey[128],membersKey[128];
	Dictionary networkRecord;

	Utils::snprintf(nwids,sizeof(nwids),"%.16llx",(unsigned long long)nwid);
	Utils::snprintf(addrs,sizeof(addrs),"%.10llx",(unsigned long long)member.address().toInt());
	Utils::snprintf(memberKey,sizeof(memberKey),"zt1:network:%s:member:%s:~",nwids,addrs);
	Utils::snprintf(nwKey,sizeof(nwKey),"zt1:network:%s:~",nwids);
	Utils::snprintf(membersKey,sizeof(membersKey),"zt1:network:%s:members",nwids);

	if (!_hgetall(nwKey,networkRecord)) {
		//LOG("netconf: Sqlite error retrieving %s",nwKey);
		return false;
	}
	if (networkRecord.get("id","") != nwids) {
		//TRACE("netconf: network %s not found (initNewMember)",nwids);
		return false;
	}

	memberRecord.clear();
	memberRecord["id"] = addrs;
	memberRecord["nwid"] = nwids;
	memberRecord["authorized"] = ((networkRecord.get("private","1") == "0") ? "1" : "0"); // auto-authorize on public networks
	memberRecord.set("firstSeen",Utils::now());
	memberRecord["identity"] = member.toString(false);

	if (!_hmset(memberKey,memberRecord))
		return false;
	if (!_sadd(membersKey,addrs))
		return false;

	return true;
#endif
}

bool SqliteNetworkConfigMaster::_generateNetconf(uint64_t nwid,const Identity &member,const Dictionary &metaData,Dictionary &netconf,uint64_t &ts,std::string &errorMessage)
{
#if 0
	char memberKey[256],nwids[24],addrs[16],tss[24],nwKey[256],revKey[128],abKey[128],ipaKey[128];
	Dictionary networkRecord,memberRecord;
	std::string revision;

	Utils::snprintf(memberKey,sizeof(memberKey),"zt1:network:%s:member:%s:~",nwids,addrs);
	Utils::snprintf(nwids,sizeof(nwids),"%.16llx",(unsigned long long)nwid);
	Utils::snprintf(addrs,sizeof(addrs),"%.10llx",(unsigned long long)member.address().toInt());
	Utils::snprintf(nwKey,sizeof(nwKey),"zt1:network:%s:~",nwids);
	Utils::snprintf(revKey,sizeof(revKey),"zt1:network:%s:revision",nwids);
	Utils::snprintf(abKey,sizeof(revKey),"zt1:network:%s:activeBridges",nwids);
	Utils::snprintf(ipaKey,sizeof(revKey),"zt1:network:%s:ipAssignments",nwids);

	if (!_hgetall(nwKey,networkRecord)) {
		errorMessage = "Sqlite error retrieving network record";
		return false;
	}
	if (networkRecord.get("id","") != nwids) {
		errorMessage = "network IDs do not match in database";
		return false;
	}

	if (!_hgetall(memberKey,memberRecord)) {
		errorMessage = "Sqlite error retrieving member record";
		return false;
	}

	if (!_get(revKey,revision)) {
		errorMessage = "Sqlite error retrieving network revision";
		return false;
	}
	if (!revision.length())
		revision = "0";

	bool isPrivate = networkRecord.getBoolean("private",true);
	ts = Utils::now();
	Utils::snprintf(tss,sizeof(tss),"%llx",ts);

	// Core configuration
	netconf[ZT_NETWORKCONFIG_DICT_KEY_TIMESTAMP] = tss;
	netconf[ZT_NETWORKCONFIG_DICT_KEY_NETWORK_ID] = nwids;
	netconf[ZT_NETWORKCONFIG_DICT_KEY_ISSUED_TO] = addrs;
	netconf[ZT_NETWORKCONFIG_DICT_KEY_PRIVATE] = isPrivate ? "1" : "0";
	netconf[ZT_NETWORKCONFIG_DICT_KEY_NAME] = networkRecord.get("name",nwids);
	netconf[ZT_NETWORKCONFIG_DICT_KEY_DESC] = networkRecord.get("desc","");
	netconf[ZT_NETWORKCONFIG_DICT_KEY_ENABLE_BROADCAST] = networkRecord.getBoolean("enableBroadcast",true) ? "1" : "0";
	netconf[ZT_NETWORKCONFIG_DICT_KEY_ALLOW_PASSIVE_BRIDGING] = networkRecord.getBoolean("allowPassiveBridging",false) ? "1" : "0";
	netconf[ZT_NETWORKCONFIG_DICT_KEY_ALLOWED_ETHERNET_TYPES] = networkRecord.get("etherTypes",""); // these are stored as hex comma-delimited list

	// Multicast options
	netconf[ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_RATES] = networkRecord.get("multicastRates","");
	uint64_t ml = networkRecord.getHexUInt("multicastLimit",0);
	if (ml > 0)
		netconf.setHex(ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_LIMIT,ml);

	// Active bridge configuration
	{
		std::string activeBridgeList;
		std::vector<std::string> activeBridgeSet;
		if (!_smembers(abKey,activeBridgeSet)) {
			errorMessage = "Sqlite error retrieving active bridge set";
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
			netconf[ZT_NETWORKCONFIG_DICT_KEY_ACTIVE_BRIDGES] = activeBridgeList;
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
					uint32_t first = (((uint32_t)(member.address().toInt() & 0xffffffffULL)) & invmask) | (pnet & pmask);
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
							errorMessage = "Sqlite error while checking IP allocation";
							return false;
						}
						if ((assignment.length() != 10)||(assignment == member.address().toString())) {
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
						_hset(ipaKey,ip.toString().c_str(),member.address().toString().c_str());
						if (ipAssignments.length() > 0)
							ipAssignments.push_back(',');
						ipAssignments.append(ip.toString());
						_hset(memberKey,"ipAssignments",ipAssignments.c_str());
					} else {
						char tmp[1024];
						Utils::snprintf(tmp,sizeof(tmp),"failed to allocate IP in %s for %s in network %s, need a larger pool!",v4AssignPool.toString().c_str(),addrs,nwids);
						errorMessage = tmp;
						return false;
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
				netconf[ZT_NETWORKCONFIG_DICT_KEY_IPV4_STATIC] = v4s;
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
				netconf[ZT_NETWORKCONFIG_DICT_KEY_IPV6_STATIC] = v6s;
		}
	}

	// If this is a private network, generate a signed certificate of membership
	if (isPrivate) {
		CertificateOfMembership com(Utils::strToU64(revision.c_str()),1,nwid,member.address());
		if (com.sign(_signingId)) // basically can't fail unless our identity is invalid
			netconf[ZT_NETWORKCONFIG_DICT_KEY_CERTIFICATE_OF_MEMBERSHIP] = com.toString();
		else {
			errorMessage = "unable to sign COM";
			return false;
		}
	}

	// Sign netconf dictionary itself
	if (!netconf.sign(_signingId)) {
		errorMessage = "unable to sign netconf dictionary";
		return false;
	}

	// Record new netconf in database for re-use on subsequent repeat queries
	{
		Dictionary upd;
		upd["netconf"] = netconf.toString();
		upd.set("netconfTimestamp",ts);
		upd["netconfRevision"] = revision;
		if (!_hmset(memberKey,upd)) {
			errorMessage = "Sqlite error updating network record with new netconf dictionary";
			return false;
		}
	}

	return true;
#endif
}

} // namespace ZeroTier
