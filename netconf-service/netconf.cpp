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

/*
 * This is the netconf service. It's currently used only by netconf nodes that
 * are run by ZeroTier itself. There is nothing to prevent you from running
 * your own if you wanted to create your own networks outside our system.
 *
 * That being said, we'd like to charge for private networks to support
 * ZeroTier One and future development efforts. So while this software is
 * open source and we're not going to stop you from sidestepping this, we
 * do ask -- honor system here -- that you pay for private networks if you
 * are going to use them for any commercial purpose such as a business VPN
 * alternative.
 *
 * This will at the moment only build on Linux and requires the mysql++
 * library, which is available here:
 *
 * http://tangentsoft.net/mysql++/
 *
 * (Packages are available for CentOS via EPEL and for any Debian distro.)
 *
 * This program must be built and installed in the services.d subfolder of
 * the ZeroTier One home folder of the node designated to act as a master
 * for networks. Doing so will enable the NETWORK_CONFIG_REQUEST protocol
 * verb.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include <iostream>
#include <string>
#include <map>
#include <list>
#include <vector>
#include <algorithm>

#include <mysql++/mysql++.h>

#include "../node/Dictionary.hpp"
#include "../node/Identity.hpp"
#include "../node/Utils.hpp"
#include "../node/Mutex.hpp"

using namespace ZeroTier;
using namespace mysqlpp;

static Mutex stdoutWriteLock;
static Connection *dbCon = (Connection *)0;
static char mysqlHost[64],mysqlPort[64],mysqlDatabase[64],mysqlUser[64],mysqlPassword[64];

int main(int argc,char **argv)
{
	{
		char *ee = getenv("ZT_NETCONF_MYSQL_HOST");
		if (!ee) {
			fprintf(stderr,"missing environment variable: ZT_NETCONF_MYSQL_HOST\n");
			return -1;
		}
		strcpy(mysqlHost,ee);
		ee = getenv("ZT_NETCONF_MYSQL_PORT");
		if (!ee)
			strcpy(mysqlPort,"3306");
		else strcpy(mysqlPort,ee);
		ee = getenv("ZT_NETCONF_MYSQL_DATABASE");
		if (!ee) {
			fprintf(stderr,"missing environment variable: ZT_NETCONF_MYSQL_DATABASE\n");
			return -1;
		}
		strcpy(mysqlDatabase,ee);
		ee = getenv("ZT_NETCONF_MYSQL_USER");
		if (!ee) {
			fprintf(stderr,"missing environment variable: ZT_NETCONF_MYSQL_USER\n");
			return -1;
		}
		strcpy(mysqlUser,ee);
		ee = getenv("ZT_NETCONF_MYSQL_PASSWORD");
		if (!ee) {
			fprintf(stderr,"missing environment variable: ZT_NETCONF_MYSQL_PASSWORD\n");
			return -1;
		}
		strcpy(mysqlPassword,ee);
	}

	char buf[131072],buf2[131072];
	std::string dictBuf;

	try {
		dbCon = new Connection(mysqlDatabase,mysqlHost,mysqlUser,mysqlPassword,(unsigned int)strtol(mysqlPort,(char **)0,10));
		if (dbCon->connected()) {
			fprintf(stderr,"connected to mysql server successfully\n");
		} else {
			fprintf(stderr,"unable to connect to database server\n");
			return -1;
		}
	} catch (std::exception &exc) {
		fprintf(stderr,"unable to connect to database server: %s\n",exc.what());
		return -1;
	}

	for(;;) {
		for(int l=0;l<4;) {
			int n = (int)read(STDIN_FILENO,buf + l,4 - l);
			if (n < 0) {
				fprintf(stderr,"error reading frame size from stdin: %s\n",strerror(errno));
				return -1;
			}
			l += n;
		}
		unsigned int fsize = (unsigned int)ntohl(*((const uint32_t *)buf));

		while (dictBuf.length() < fsize) {
			int n = (int)read(STDIN_FILENO,buf,std::min((int)sizeof(buf),(int)(fsize - dictBuf.length())));
			if (n < 0) {
				fprintf(stderr,"error reading frame from stdin: %s\n",strerror(errno));
				return -1;
			}
			for(int i=0;i<n;++i)
				dictBuf.push_back(buf[i]);
		}
		Dictionary request(dictBuf);
		dictBuf = "";

		if (!dbCon->connected()) {
			fprintf(stderr,"connection to database server lost\n");
			return -1;
		}

		// Check QNetworkConfigRefresh (MEMORY table) and push network
		// config refreshes to queued peer/network pairs.
		try {
			Dictionary to;
			{
				Query q = dbCon->query();
				q << "SELECT LOWER(HEX(Node_id)) AS Node_id,LOWER(HEX(Network_id)) AS Network_id FROM QNetworkConfigRefresh";
				StoreQueryResult rs = q.store();
				for(unsigned long i=0;i<rs.num_rows();++i) {
					std::string &nwids = to[rs[i]["Node_id"]];
					if (nwids.length())
						nwids.push_back(',');
					nwids.append(rs[i]["Network_id"]);
				}
			}

			{
				Query q = dbCon->query();
				q << "DELETE FROM QNetworkConfigRefresh";
				q.exec();
			}

			Dictionary response;
			response["type"] = "netconf-push";
			response["to"] = to.toString();
			std::string respm = response.toString();
			uint32_t respml = (uint32_t)htonl((uint32_t)respm.length());

			stdoutWriteLock.lock();
			write(STDOUT_FILENO,&respml,4);
			write(STDOUT_FILENO,respm.data(),respm.length());
			stdoutWriteLock.unlock();
		} catch ( ... ) {}

		try {
			const std::string &reqType = request.get("type");
			if (reqType == "netconf-request") { // NETWORK_CONFIG_REQUEST packet
				// Deserialize querying peer identity and network ID
				Identity peerIdentity(request.get("peerId"));
				uint64_t nwid = strtoull(request.get("nwid").c_str(),(char **)0,16);
				std::string fromAddr(request.get("from"));

				// Meta-information from node, such as (future) geo-location stuff
				Dictionary meta;
				if (request.contains("meta"))
					meta.fromString(request.get("meta"));

				// Check validity of node's identity, ignore request on failure
				if (!peerIdentity.locallyValidate()) {
					fprintf(stderr,"identity failed validity check: %s\n",peerIdentity.toString(false).c_str());
					continue;
				}

				// Save node's identity if unknown
				{
					Query q = dbCon->query();
					q << "SELECT identity FROM Node WHERE id = " << peerIdentity.address().toInt();
					StoreQueryResult rs = q.store();
					if (rs.num_rows() > 0) {
						if (rs[0]["identity"] != peerIdentity.toString(false)) {
							// TODO: handle collisions...
							continue;
						} else if ((int)rs[0]["identityValidated"] == 0) {
							// TODO: launch background validation
						}
					} else {
						q = dbCon->query();
						q << "INSERT INTO Node (id,creationTime,lastSeen,identity) VALUES (" << peerIdentity.address().toInt() << "," << Utils::now() << ",0," << quote << peerIdentity.toString(false) << ")";
						if (!q.exec()) {
							fprintf(stderr,"error inserting Node row for peer %s, aborting netconf request\n",peerIdentity.address().toString().c_str());
							continue;
						}
						// TODO: launch background validation
					}
				}

				// Update lastSeen for Node, which is always updated on a netconf request
				{
					Query q = dbCon->query();
					q << "UPDATE Node SET lastSeen = " << Utils::now() << " WHERE id = " << peerIdentity.address().toInt();
					q.exec();
				}

				// Look up core network information
				bool isOpen = false;
				unsigned int multicastPrefixBits = 0;
				unsigned int multicastDepth = 0;
				bool emulateArp = false;
				bool emulateNdp = false;
				std::string name;
				std::string desc;
				{
					Query q = dbCon->query();
					q << "SELECT name,`desc`,isOpen,multicastPrefixBits,multicastDepth FROM Network WHERE id = " << nwid;
					StoreQueryResult rs = q.store();
					if (rs.num_rows() > 0) {
						name = rs[0]["name"].c_str();
						desc = rs[0]["desc"].c_str();
						isOpen = ((int)rs[0]["isOpen"] > 0);
						emulateArp = ((int)rs[0]["emulateArp"] > 0);
						emulateNdp = ((int)rs[0]["emulateNdp"] > 0);
						multicastPrefixBits = (unsigned int)rs[0]["multicastPrefixBits"];
						multicastDepth = (unsigned int)rs[0]["multicastDepth"];
					} else {
						Dictionary response;
						response["peer"] = peerIdentity.address().toString();
						response["nwid"] = request.get("nwid");
						response["type"] = "netconf-response";
						response["requestId"] = request.get("requestId");
						response["error"] = "OBJ_NOT_FOUND";
						std::string respm = response.toString();
						uint32_t respml = (uint32_t)htonl((uint32_t)respm.length());

						stdoutWriteLock.lock();
						write(STDOUT_FILENO,&respml,4);
						write(STDOUT_FILENO,respm.data(),respm.length());
						stdoutWriteLock.unlock();
						continue; // ABORT, wait for next request
					}
				}

				// Check membership if this is a closed network
				if (!isOpen) {
					Query q = dbCon->query();
					q << "SELECT Node_id FROM NetworkNodes WHERE Network_id = " << nwid << " AND Node_id = " << peerIdentity.address().toInt();
					StoreQueryResult rs = q.store();
					if (!rs.num_rows()) {
						Dictionary response;
						response["peer"] = peerIdentity.address().toString();
						response["nwid"] = request.get("nwid");
						response["type"] = "netconf-response";
						response["requestId"] = request.get("requestId");
						response["error"] = "ACCESS_DENIED";
						std::string respm = response.toString();
						uint32_t respml = (uint32_t)htonl((uint32_t)respm.length());

						stdoutWriteLock.lock();
						write(STDOUT_FILENO,&respml,4);
						write(STDOUT_FILENO,respm.data(),respm.length());
						stdoutWriteLock.unlock();
						continue; // ABORT, wait for next request
					}
				}

				// Get list of etherTypes in comma-delimited hex format
				std::string etherTypeWhitelist;
				{
					Query q = dbCon->query();
					q << "SELECT DISTINCT LOWER(HEX(etherType)) AS etherType FROM NetworkEthertypes WHERE Network_id = " << nwid;
					StoreQueryResult rs = q.store();
					for(unsigned long i=0;i<rs.num_rows();++i) {
						if (etherTypeWhitelist.length() > 0)
							etherTypeWhitelist.push_back(',');
						etherTypeWhitelist.append(rs[i]["etherType"].c_str());
					}
				}

				// Get multicast group rates in dictionary format
				Dictionary multicastRates;
				{
					Query q = dbCon->query();
					q << "SELECT DISTINCT multicastGroupMac,multicastGroupAdi,preload,maxBalance,accrual FROM NetworkMulticastRates WHERE Network_id = " << nwid;
					StoreQueryResult rs = q.store();
					for(unsigned long i=0;i<rs.num_rows();++i) {
						long preload = (long)rs[i]["preload"];
						long maxBalance = (long)rs[i]["maxBalance"];
						long accrual = (long)rs[i]["accrual"];
						sprintf(buf2,"%s%lx,%s%lx,%s%lx",
							((preload < 0) ? "-" : ""),
							preload,
							((maxBalance < 0) ? "-" : ""),
							maxBalance,
							((accrual < 0) ? "-" : ""),
							accrual);
						unsigned long long mac = (unsigned long long)rs[i]["multicastGroupMac"];
						if (mac) {
							sprintf(buf,"%.12llx/%lx",(mac & 0xffffffffffffULL),(unsigned long)rs[i]["multicastGroupAdi"]);
							multicastRates[buf] = buf2;
						} else { // zero MAC indicates default for unmatching multicast groups
							multicastRates["*"] = buf2;
						}
					}
				}

				// Check for (or assign?) static IP address assignments
				std::string ipv4Static;
				std::string ipv6Static;
				{
					Query q = dbCon->query();
					q << "SELECT INET_NTOA(ip) AS ip,netmaskBits FROM IPv4Static WHERE Node_id = " << peerIdentity.address().toInt() << " AND Network_id = " << nwid;
					StoreQueryResult rs = q.store();
					if (rs.num_rows() > 0) {
						for(int i=0;i<rs.num_rows();++i) {
							if (ipv4Static.length())
								ipv4Static.push_back(',');
							ipv4Static.append(rs[i]["ip"].c_str());
							ipv4Static.push_back('/');
							ipv4Static.append(rs[i]["netmaskBits"].c_str());
						}
					}

					// Try to auto-assign if there's any auto-assign networks with space
					// available.
					if (!ipv4Static.length()) {
						unsigned char addressBytes[5];
						peerIdentity.address().copyTo(addressBytes,5);

						q = dbCon->query();
						q << "SELECT ipNet,netmaskBits FROM IPv4AutoAssign WHERE Network_id = " << nwid;
						rs = q.store();
						if (rs.num_rows() > 0) {
							for(int aaRow=0;aaRow<rs.num_rows();++aaRow) {
								uint32_t ipNet = (uint32_t)((unsigned long)rs[aaRow]["ipNet"]);
								unsigned int netmaskBits = (unsigned int)rs[aaRow]["netmaskBits"];

								uint32_t tryIp = (((uint32_t)addressBytes[1]) << 24) |
								                 (((uint32_t)addressBytes[2]) << 16) |
								                 (((uint32_t)addressBytes[3]) << 8) |
								                 ((((uint32_t)addressBytes[4]) % 254) + 1);
								tryIp &= (0xffffffff >> netmaskBits);
								tryIp |= ipNet;

								for(int k=0;k<100000;++k) {
									Query q2 = dbCon->query();
									q2 << "INSERT INTO IPv4Static (Network_id,Node_id,ip,netmaskBits) VALUES (" << nwid << "," << peerIdentity.address().toInt() << "," << tryIp << "," << netmaskBits << ")";
									if (q2.exec()) {
										sprintf(buf,"%u.%u.%u.%u",(unsigned int)((tryIp >> 24) & 0xff),(unsigned int)((tryIp >> 16) & 0xff),(unsigned int)((tryIp >> 8) & 0xff),(unsigned int)(tryIp & 0xff));
										if (ipv4Static.length())
											ipv4Static.push_back(',');
										ipv4Static.append(buf);
										ipv4Static.push_back('/');
										sprintf(buf,"%u",netmaskBits);
										ipv4Static.append(buf);
										break;
									} else { // insert will fail if IP is in use due to uniqueness constraints in DB
										++tryIp;
										if ((tryIp & 0xff) == 0)
											tryIp |= 1;
										tryIp &= (0xffffffff >> netmaskBits);
										tryIp |= ipNet;
									}
								}

								if (ipv4Static.length())
									break;
							}
						}
					}
				}

				// Update activity table for this network to indicate peer's participation
				{
					Query q = dbCon->query();
					q << "INSERT INTO NetworkActivity (Network_id,Node_id,lastActivityTime,lastActivityFrom) VALUES (" << nwid << "," << peerIdentity.address().toInt() << "," << Utils::now() << "," << fromAddr << ") ON DUPLICATE KEY UPDATE lastActivityTime = VALUES(lastActivityTime),lastActivityFrom = VALUES(lastActivityFrom)";
					q.exec();
				}

				// Assemble response dictionary to send to peer
				Dictionary netconf;
				sprintf(buf,"%.16llx",(unsigned long long)nwid);
				netconf["nwid"] = buf;
				netconf["peer"] = peerIdentity.address().toString();
				netconf["name"] = name;
				netconf["desc"] = desc;
				netconf["o"] = (isOpen ? "1" : "0");
				netconf["et"] = etherTypeWhitelist;
				netconf["mr"] = multicastRates.toString();
				sprintf(buf,"%llx",(unsigned long long)Utils::now());
				netconf["ts"] = buf;
				netconf["eARP"] = (emulateArp ? "1" : "0");
				netconf["eNDP"] = (emulateNdp ? "1" : "0");
				if (multicastPrefixBits) {
					sprintf(buf,"%x",multicastPrefixBits);
					netconf["mpb"] = buf;
				}
				if (multicastDepth) {
					sprintf(buf,"%x",multicastDepth);
					netconf["md"] = buf;
				}
				if (ipv4Static.length())
					netconf["v4s"] = ipv4Static;
				if (ipv6Static.length())
					netconf["v6s"] = ipv6Static;

				// Send netconf as service bus response
				{
					Dictionary response;
					response["peer"] = peerIdentity.address().toString();
					response["nwid"] = request.get("nwid");
					response["type"] = "netconf-response";
					response["requestId"] = request.get("requestId");
					response["netconf"] = netconf.toString();
					std::string respm = response.toString();
					uint32_t respml = (uint32_t)htonl((uint32_t)respm.length());

					stdoutWriteLock.lock();
					write(STDOUT_FILENO,&respml,4);
					write(STDOUT_FILENO,respm.data(),respm.length());
					stdoutWriteLock.unlock();

					// LOOP, wait for next request
				}
			}
		} catch (std::exception &exc) {
			fprintf(stderr,"unexpected exception handling message: %s\n",exc.what());
		} catch ( ... ) {
			fprintf(stderr,"unexpected exception handling message: unknown exception\n");
		}
	}
}
