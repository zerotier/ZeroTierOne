/*
 * Copyright (c)2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>

#include "node/Constants.hpp"

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <Windows.h>
#include <tchar.h>
#include <wchar.h>
#include <lmcons.h>
#include <newdev.h>
#include <atlbase.h>
#include <iphlpapi.h>
#include <iomanip>
#include "osdep/WindowsEthernetTap.hpp"
#include "windows/ZeroTierOne/ServiceInstaller.h"
#include "windows/ZeroTierOne/ServiceBase.h"
#include "windows/ZeroTierOne/ZeroTierOneService.h"
#else
#include <unistd.h>
#include <pwd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <dirent.h>
#include <signal.h>
#ifdef __LINUX__
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#ifndef ZT_NO_CAPABILITIES
#include <linux/capability.h>
#include <linux/securebits.h>
#endif
#endif
#endif

#include <string>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <algorithm>

#include "version.h"
#include "include/ZeroTierOne.h"

#include "node/Identity.hpp"
#include "node/CertificateOfMembership.hpp"
#include "node/Utils.hpp"
#include "node/NetworkController.hpp"
#include "node/Buffer.hpp"
#include "node/World.hpp"

#include "osdep/OSUtils.hpp"
#include "osdep/Http.hpp"
#include "osdep/Thread.hpp"

#include "node/BondController.hpp"

#include "service/OneService.hpp"

#include "ext/json/json.hpp"

#ifdef __APPLE__
#include <SystemConfiguration/SystemConfiguration.h>
#include <CoreServices/CoreServices.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#endif

#define ZT_PID_PATH "zerotier-one.pid"

using namespace ZeroTier;

static OneService *volatile zt1Service = (OneService *)0;

#define PROGRAM_NAME "ZeroTier One"
#define COPYRIGHT_NOTICE "Copyright (c) 2020 ZeroTier, Inc."
#define LICENSE_GRANT "Licensed under the ZeroTier BSL 1.1 (see LICENSE.txt)"

/****************************************************************************/
/* zerotier-cli personality                                                 */
/****************************************************************************/

// This is getting deprecated soon in favor of the stuff in cli/

static void cliPrintHelp(const char *pn,FILE *out)
{
	fprintf(out,
		"%s version %d.%d.%d build %d (platform %d arch %d)" ZT_EOL_S,
		PROGRAM_NAME,
		ZEROTIER_ONE_VERSION_MAJOR, ZEROTIER_ONE_VERSION_MINOR, ZEROTIER_ONE_VERSION_REVISION, ZEROTIER_ONE_VERSION_BUILD,
		ZT_BUILD_PLATFORM, ZT_BUILD_ARCHITECTURE);
	fprintf(out,
		COPYRIGHT_NOTICE ZT_EOL_S
		LICENSE_GRANT ZT_EOL_S);
	fprintf(out,"Usage: %s [-switches] <command/path> [<args>]" ZT_EOL_S"" ZT_EOL_S,pn);
	fprintf(out,"Available switches:" ZT_EOL_S);
	fprintf(out,"  -h                      - Display this help" ZT_EOL_S);
	fprintf(out,"  -v                      - Show version" ZT_EOL_S);
	fprintf(out,"  -j                      - Display full raw JSON output" ZT_EOL_S);
	fprintf(out,"  -D<path>                - ZeroTier home path for parameter auto-detect" ZT_EOL_S);
	fprintf(out,"  -p<port>                - HTTP port (default: auto)" ZT_EOL_S);
	fprintf(out,"  -T<token>               - Authentication token (default: auto)" ZT_EOL_S);
	fprintf(out,ZT_EOL_S"Available commands:" ZT_EOL_S);
	fprintf(out,"  info                    - Display status info" ZT_EOL_S);
	fprintf(out,"  listpeers               - List all peers" ZT_EOL_S);
	fprintf(out,"  peers                   - List all peers (prettier)" ZT_EOL_S);
	fprintf(out,"  listnetworks            - List all networks" ZT_EOL_S);
	fprintf(out,"  join <network ID>          - Join a network" ZT_EOL_S);
	fprintf(out,"  leave <network ID>         - Leave a network" ZT_EOL_S);
	fprintf(out,"  set <network ID> <setting> - Set a network setting" ZT_EOL_S);
	fprintf(out,"  get <network ID> <setting> - Get a network setting" ZT_EOL_S);
	fprintf(out,"  listmoons               - List moons (federated root sets)" ZT_EOL_S);
	fprintf(out,"  orbit <world ID> <seed> - Join a moon via any member root" ZT_EOL_S);
	fprintf(out,"  deorbit <world ID>      - Leave a moon" ZT_EOL_S);
	fprintf(out,ZT_EOL_S"Available settings:" ZT_EOL_S);
	fprintf(out,"  Settings to use with [get/set] may include property names from " ZT_EOL_S);
	fprintf(out,"  the JSON output of \"zerotier-cli -j listnetworks\". Additionally, " ZT_EOL_S);
	fprintf(out,"  (ip, ip4, ip6, ip6plane, and ip6prefix can be used). For instance:" ZT_EOL_S);
	fprintf(out,"  zerotier-cli get <network ID> ip6plane will return the 6PLANE address" ZT_EOL_S);
	fprintf(out,"  assigned to this node." ZT_EOL_S);
}

static std::string cliFixJsonCRs(const std::string &s)
{
	std::string r;
	for(std::string::const_iterator c(s.begin());c!=s.end();++c) {
		if (*c == '\n')
			r.append(ZT_EOL_S);
		else r.push_back(*c);
	}
	return r;
}

#ifdef __WINDOWS__
static int cli(int argc, _TCHAR* argv[])
#else
static int cli(int argc,char **argv)
#endif
{
	unsigned int port = 0;
	std::string homeDir,command,arg1,arg2,authToken;
	std::string ip("127.0.0.1");
	bool json = false;
	for(int i=1;i<argc;++i) {
		if (argv[i][0] == '-') {
			switch(argv[i][1]) {

				case 'q': // ignore -q used to invoke this personality
					if (argv[i][2]) {
						cliPrintHelp(argv[0],stdout);
						return 1;
					}
					break;

				case 'j':
					if (argv[i][2]) {
						cliPrintHelp(argv[0],stdout);
						return 1;
					}
					json = true;
					break;

				case 'p':
					port = Utils::strToUInt(argv[i] + 2);
					if ((port > 0xffff)||(port == 0)) {
						cliPrintHelp(argv[0],stdout);
						return 1;
					}
					break;

				case 'D':
					if (argv[i][2]) {
						homeDir = argv[i] + 2;
					} else {
						cliPrintHelp(argv[0],stdout);
						return 1;
					}
					break;

				case 'H':
					if (argv[i][2]) {
						ip = argv[i] + 2;
					} else {
						cliPrintHelp(argv[0],stdout);
						return 1;
					}
					break;

				case 'T':
					if (argv[i][2]) {
						authToken = argv[i] + 2;
					} else {
						cliPrintHelp(argv[0],stdout);
						return 1;
					}
					break;

				case 'v':
					if (argv[i][2]) {
						cliPrintHelp(argv[0],stdout);
						return 1;
					}
					printf("%d.%d.%d" ZT_EOL_S,ZEROTIER_ONE_VERSION_MAJOR,ZEROTIER_ONE_VERSION_MINOR,ZEROTIER_ONE_VERSION_REVISION);
					return 0;

				case 'h':
				case '?':
				default:
					cliPrintHelp(argv[0],stdout);
					return 0;
			}
		} else {
			if (arg1.length())
				arg2 = argv[i];
			else if (command.length())
				arg1 = argv[i];
			else command = argv[i];
		}
	}
	if (!homeDir.length())
		homeDir = OneService::platformDefaultHomePath();

	// TODO: cleanup this logic
	// A lot of generic CLI errors land here; missing admin rights cause a bit of this.
	if ((!port)||(!authToken.length())) {
		if (!homeDir.length()) {
			fprintf(stderr,"%s: missing port or authentication token and no home directory specified to auto-detect" ZT_EOL_S,argv[0]);
			fprintf(stderr, "If you did not, please run this command as an Administrator / sudo / Root user. Thanks!");
			return 2;
		}

		if (!port) {
			std::string portStr;
			OSUtils::readFile((homeDir + ZT_PATH_SEPARATOR_S + "zerotier-one.port").c_str(),portStr);
			port = Utils::strToUInt(portStr.c_str());
			if ((port == 0)||(port > 0xffff)) {
				fprintf(stderr,"%s: missing port and zerotier-one.port not found in %s" ZT_EOL_S,argv[0],homeDir.c_str());
				fprintf(stderr, "If you did not, please run this command as an Administrator / sudo / Root user. Thanks!");
				return 2;
			}
		}

		if (!authToken.length()) {
			OSUtils::readFile((homeDir + ZT_PATH_SEPARATOR_S + "authtoken.secret").c_str(),authToken);
#ifdef __UNIX_LIKE__
			if (!authToken.length()) {
				const char *hd = getenv("HOME");
				if (hd) {
					char p[4096];
#ifdef __APPLE__
					OSUtils::ztsnprintf(p,sizeof(p),"%s/Library/Application Support/ZeroTier/One/authtoken.secret",hd);
#else
					OSUtils::ztsnprintf(p,sizeof(p),"%s/.zeroTierOneAuthToken",hd);
#endif
					OSUtils::readFile(p,authToken);
				}
			}
#endif
			if (!authToken.length()) {
				fprintf(stderr,"%s: missing authentication token and authtoken.secret not found (or readable) in %s" ZT_EOL_S,argv[0],homeDir.c_str());
				fprintf(stderr, "If you did not, please run this command as an Administrator / sudo / Root user. Thanks!");
				return 2;
			}
		}
	}

	InetAddress addr;
	{
		char addrtmp[256];
		OSUtils::ztsnprintf(addrtmp,sizeof(addrtmp),"%s/%u",ip.c_str(),port);
		addr = InetAddress(addrtmp);
	}

	std::map<std::string,std::string> requestHeaders;
	std::map<std::string,std::string> responseHeaders;
	std::string responseBody;

	requestHeaders["X-ZT1-Auth"] = authToken;

	if ((command.length() > 0)&&(command[0] == '/')) {
		unsigned int scode = Http::GET(
			1024 * 1024 * 16,
			60000,
			(const struct sockaddr *)&addr,
			command.c_str(),
			requestHeaders,
			responseHeaders,
			responseBody);
		if (scode == 200) {
			printf("%s", cliFixJsonCRs(responseBody).c_str());
			return 0;
		} else {
			printf("%u %s %s" ZT_EOL_S,scode,command.c_str(),responseBody.c_str());
			return 1;
		}
	} else if ((command == "info")||(command == "status")) {
		const unsigned int scode = Http::GET(1024 * 1024 * 16,60000,(const struct sockaddr *)&addr,"/status",requestHeaders,responseHeaders,responseBody);

		if (scode == 0) {
			printf("Error connecting to the ZeroTier service: %s\n\nPlease check that the service is running and that TCP port 9993 can be contacted via 127.0.0.1." ZT_EOL_S, responseBody.c_str());
			return 1;
		}

		nlohmann::json j;
		try {
			j = OSUtils::jsonParse(responseBody);
		} catch (std::exception &exc) {
			printf("%u %s invalid JSON response (%s)" ZT_EOL_S,scode,command.c_str(),exc.what());
			return 1;
		} catch ( ... ) {
			printf("%u %s invalid JSON response (unknown exception)" ZT_EOL_S,scode,command.c_str());
			return 1;
		}

		if (scode == 200) {
			if (json) {
				printf("%s" ZT_EOL_S,OSUtils::jsonDump(j).c_str());
			} else {
				if (j.is_object()) {
					printf("200 info %s %s %s" ZT_EOL_S,
						OSUtils::jsonString(j["address"],"-").c_str(),
						OSUtils::jsonString(j["version"],"-").c_str(),
						((j["tcpFallbackActive"]) ? "TUNNELED" : ((j["online"]) ? "ONLINE" : "OFFLINE")));
				}
			}
			return 0;
		} else {
			printf("%u %s %s" ZT_EOL_S,scode,command.c_str(),responseBody.c_str());
			return 1;
		}
	} else if (command == "listpeers") {
		const unsigned int scode = Http::GET(1024 * 1024 * 16,60000,(const struct sockaddr *)&addr,"/peer",requestHeaders,responseHeaders,responseBody);

		if (scode == 0) {
			printf("Error connecting to the ZeroTier service: %s\n\nPlease check that the service is running and that TCP port 9993 can be contacted via 127.0.0.1." ZT_EOL_S, responseBody.c_str());
			return 1;
		}

		nlohmann::json j;
		try {
			j = OSUtils::jsonParse(responseBody);
		} catch (std::exception &exc) {
			printf("%u %s invalid JSON response (%s)" ZT_EOL_S,scode,command.c_str(),exc.what());
			return 1;
		} catch ( ... ) {
			printf("%u %s invalid JSON response (unknown exception)" ZT_EOL_S,scode,command.c_str());
			return 1;
		}

		if (scode == 200) {
			if (json) {
				printf("%s" ZT_EOL_S,OSUtils::jsonDump(j).c_str());
			} else {
				printf("200 listpeers <ztaddr> <path> <latency> <version> <role>" ZT_EOL_S);
				if (j.is_array()) {
					for(unsigned long k=0;k<j.size();++k) {
						nlohmann::json &p = j[k];
						std::string bestPath;
						nlohmann::json &paths = p["paths"];
						if (paths.is_array()) {
							for(unsigned long i=0;i<paths.size();++i) {
								nlohmann::json &path = paths[i];
								if (path["preferred"]) {
									char tmp[256];
									std::string addr = path["address"];
									const int64_t now = OSUtils::now();
									OSUtils::ztsnprintf(tmp,sizeof(tmp),"%s;%lld;%lld",addr.c_str(),now - (int64_t)path["lastSend"],now - (int64_t)path["lastReceive"]);
									bestPath = tmp;
									break;
								}
							}
						}
						if (bestPath.length() == 0) bestPath = "-";
						char ver[128];
						int64_t vmaj = p["versionMajor"];
						int64_t vmin = p["versionMinor"];
						int64_t vrev = p["versionRev"];
						if (vmaj >= 0) {
							OSUtils::ztsnprintf(ver,sizeof(ver),"%lld.%lld.%lld",vmaj,vmin,vrev);
						} else {
							ver[0] = '-';
							ver[1] = (char)0;
						}
						printf("200 listpeers %s %s %d %s %s" ZT_EOL_S,
							OSUtils::jsonString(p["address"],"-").c_str(),
							bestPath.c_str(),
							(int)OSUtils::jsonInt(p["latency"],0),
							ver,
							OSUtils::jsonString(p["role"],"-").c_str());
					}
				}
			}
			return 0;
		} else {
			printf("%u %s %s" ZT_EOL_S,scode,command.c_str(),responseBody.c_str());
			return 1;
		}
	} else if (command == "peers") {
		const unsigned int scode = Http::GET(1024 * 1024 * 16,60000,(const struct sockaddr *)&addr,"/peer",requestHeaders,responseHeaders,responseBody);

		if (scode == 0) {
			printf("Error connecting to the ZeroTier service: %s\n\nPlease check that the service is running and that TCP port 9993 can be contacted via 127.0.0.1." ZT_EOL_S, responseBody.c_str());
			return 1;
		}

		nlohmann::json j;
		try {
			j = OSUtils::jsonParse(responseBody);
		} catch (std::exception &exc) {
			printf("%u %s invalid JSON response (%s)" ZT_EOL_S,scode,command.c_str(),exc.what());
			return 1;
		} catch ( ... ) {
			printf("%u %s invalid JSON response (unknown exception)" ZT_EOL_S,scode,command.c_str());
			return 1;
		}

		if (scode == 200) {
			if (json) {
				printf("%s" ZT_EOL_S,OSUtils::jsonDump(j).c_str());
			} else {
				printf("200 peers\n<ztaddr>   <ver>  <role> <lat> <link> <lastTX> <lastRX> <path>" ZT_EOL_S);
				if (j.is_array()) {
					for(unsigned long k=0;k<j.size();++k) {
						nlohmann::json &p = j[k];
						std::string bestPath;
						nlohmann::json &paths = p["paths"];
						if (paths.is_array()) {
							for(unsigned long i=0;i<paths.size();++i) {
								nlohmann::json &path = paths[i];
								if (path["preferred"]) {
									char tmp[256];
									std::string addr = path["address"];
									const int64_t now = OSUtils::now();
									OSUtils::ztsnprintf(tmp,sizeof(tmp),"%-8lld %-8lld %s",now - (int64_t)path["lastSend"],now - (int64_t)path["lastReceive"],addr.c_str());
									bestPath = std::string("DIRECT ") + tmp;
									break;
								}
							}
						}
						if (bestPath.length() == 0) bestPath = "RELAY";
						char ver[128];
						int64_t vmaj = p["versionMajor"];
						int64_t vmin = p["versionMinor"];
						int64_t vrev = p["versionRev"];
						if (vmaj >= 0) {
							OSUtils::ztsnprintf(ver,sizeof(ver),"%lld.%lld.%lld",vmaj,vmin,vrev);
						} else {
							ver[0] = '-';
							ver[1] = (char)0;
						}
						printf("%s %-6s %-6s %5d %s" ZT_EOL_S,
							OSUtils::jsonString(p["address"],"-").c_str(),
							ver,
							OSUtils::jsonString(p["role"],"-").c_str(),
							(int)OSUtils::jsonInt(p["latency"],0),
							bestPath.c_str());
					}
				}
			}
			return 0;
		} else {
			printf("%u %s %s" ZT_EOL_S,scode,command.c_str(),responseBody.c_str());
			return 1;
		}
	} else if (command == "listbonds") {
		const unsigned int scode = Http::GET(1024 * 1024 * 16,60000,(const struct sockaddr *)&addr,"/bonds",requestHeaders,responseHeaders,responseBody);

		if (scode == 0) {
			printf("Error connecting to the ZeroTier service: %s\n\nPlease check that the service is running and that TCP port 9993 can be contacted via 127.0.0.1." ZT_EOL_S, responseBody.c_str());
			return 1;
		}

		nlohmann::json j;
		try {
			j = OSUtils::jsonParse(responseBody);
		} catch (std::exception &exc) {
			printf("%u %s invalid JSON response (%s)" ZT_EOL_S,scode,command.c_str(),exc.what());
			return 1;
		} catch ( ... ) {
			printf("%u %s invalid JSON response (unknown exception)" ZT_EOL_S,scode,command.c_str());
			return 1;
		}

		if (scode == 200) {
			if (json) {
				printf("%s" ZT_EOL_S,OSUtils::jsonDump(j).c_str());
			} else {
				bool bFoundBond = false;
				printf("    <peer>                        <bondtype>    <status>    <links>" ZT_EOL_S);
				if (j.is_array()) {
					for(unsigned long k=0;k<j.size();++k) {
						nlohmann::json &p = j[k];

						bool isBonded = p["isBonded"];
						int8_t bondingPolicy = p["bondingPolicy"];
						bool isHealthy = p["isHealthy"];
						int8_t numAliveLinks = p["numAliveLinks"];
						int8_t numTotalLinks = p["numTotalLinks"];

						if (isBonded) {
							bFoundBond = true;
							std::string healthStr;
							if (isHealthy) {
								healthStr = "HEALTHY";
							} else {
								healthStr = "DEGRADED";
							}
							std::string policyStr = "none";
							if (bondingPolicy >= ZT_BONDING_POLICY_NONE && bondingPolicy <= ZT_BONDING_POLICY_BALANCE_AWARE) {
								policyStr = BondController::getPolicyStrByCode(bondingPolicy);
							}

							printf("%10s  %32s    %8s        %d/%d" ZT_EOL_S,
								OSUtils::jsonString(p ["address"],"-").c_str(),
								policyStr.c_str(),
								healthStr.c_str(),
								numAliveLinks,
								numTotalLinks);
						}
					}
				}
				if (!bFoundBond) {
					printf("      NONE\t\t\t\tNONE\t    NONE       NONE" ZT_EOL_S);
				}
			}
			return 0;
		} else {
			printf("%u %s %s" ZT_EOL_S,scode,command.c_str(),responseBody.c_str());
			return 1;
		}
	} else if (command == "listnetworks") {
		const unsigned int scode = Http::GET(1024 * 1024 * 16,60000,(const struct sockaddr *)&addr,"/network",requestHeaders,responseHeaders,responseBody);

		if (scode == 0) {
			printf("Error connecting to the ZeroTier service: %s\n\nPlease check that the service is running and that TCP port 9993 can be contacted via 127.0.0.1." ZT_EOL_S, responseBody.c_str());
			return 1;
		}

		nlohmann::json j;
		try {
			j = OSUtils::jsonParse(responseBody);
		} catch (std::exception &exc) {
			printf("%u %s invalid JSON response (%s)" ZT_EOL_S,scode,command.c_str(),exc.what());
			return 1;
		} catch ( ... ) {
			printf("%u %s invalid JSON response (unknown exception)" ZT_EOL_S,scode,command.c_str());
			return 1;
		}

		if (scode == 200) {
			if (json) {
				printf("%s" ZT_EOL_S,OSUtils::jsonDump(j).c_str());
			} else {
				printf("200 listnetworks <nwid> <name> <mac> <status> <type> <dev> <ZT assigned ips>" ZT_EOL_S);
				if (j.is_array()) {
					for(unsigned long i=0;i<j.size();++i) {
						nlohmann::json &n = j[i];
						if (n.is_object()) {
							std::string aa;
							nlohmann::json &assignedAddresses = n["assignedAddresses"];
							if (assignedAddresses.is_array()) {
								for(unsigned long j=0;j<assignedAddresses.size();++j) {
									nlohmann::json &addr = assignedAddresses[j];
									if (addr.is_string()) {
										if (aa.length() > 0) aa.push_back(',');
										aa.append(addr.get<std::string>());
									}
								}
							}
							if (aa.length() == 0) aa = "-";
							printf("200 listnetworks %s %s %s %s %s %s %s" ZT_EOL_S,
								OSUtils::jsonString(n["nwid"],"-").c_str(),
								OSUtils::jsonString(n["name"],"-").c_str(),
								OSUtils::jsonString(n["mac"],"-").c_str(),
								OSUtils::jsonString(n["status"],"-").c_str(),
								OSUtils::jsonString(n["type"],"-").c_str(),
								OSUtils::jsonString(n["portDeviceName"],"-").c_str(),
								aa.c_str());
						}
					}
				}
			}
			return 0;
		} else {
			printf("%u %s %s" ZT_EOL_S,scode,command.c_str(),responseBody.c_str());
			return 1;
		}
	} else if (command == "join") {
		if (arg1.length() != 16) {
			printf("invalid network id" ZT_EOL_S);
			return 2;
		}
		requestHeaders["Content-Type"] = "application/json";
		requestHeaders["Content-Length"] = "2";
		unsigned int scode = Http::POST(
			1024 * 1024 * 16,
			60000,
			(const struct sockaddr *)&addr,
			(std::string("/network/") + arg1).c_str(),
			requestHeaders,
			"{}",
			2,
			responseHeaders,
			responseBody);
		if (scode == 200) {
			if (json) {
				printf("%s",cliFixJsonCRs(responseBody).c_str());
			} else {
				printf("200 join OK" ZT_EOL_S);
			}
			return 0;
		} else {
			printf("%u %s %s" ZT_EOL_S,scode,command.c_str(),responseBody.c_str());
			return 1;
		}
	} else if (command == "leave") {
		if (arg1.length() != 16) {
			printf("invalid network id" ZT_EOL_S);
			return 2;
		}
		unsigned int scode = Http::DEL(
			1024 * 1024 * 16,
			60000,
			(const struct sockaddr *)&addr,
			(std::string("/network/") + arg1).c_str(),
			requestHeaders,
			responseHeaders,
			responseBody);
		if (scode == 200) {
			if (json) {
				printf("%s",cliFixJsonCRs(responseBody).c_str());
			} else {
				printf("200 leave OK" ZT_EOL_S);
			}
			return 0;
		} else {
			printf("%u %s %s" ZT_EOL_S,scode,command.c_str(),responseBody.c_str());
			return 1;
		}
	} else if (command == "listmoons") {
		const unsigned int scode = Http::GET(1024 * 1024 * 16,60000,(const struct sockaddr *)&addr,"/moon",requestHeaders,responseHeaders,responseBody);

		if (scode == 0) {
			printf("Error connecting to the ZeroTier service: %s\n\nPlease check that the service is running and that TCP port 9993 can be contacted via 127.0.0.1." ZT_EOL_S, responseBody.c_str());
			return 1;
		}

		nlohmann::json j;
		try {
			j = OSUtils::jsonParse(responseBody);
		} catch (std::exception &exc) {
			printf("%u %s invalid JSON response (%s)" ZT_EOL_S,scode,command.c_str(),exc.what());
			return 1;
		} catch ( ... ) {
			printf("%u %s invalid JSON response (unknown exception)" ZT_EOL_S,scode,command.c_str());
			return 1;
		}

		if (scode == 200) {
			printf("%s" ZT_EOL_S,OSUtils::jsonDump(j).c_str());
			return 0;
		} else {
			printf("%u %s %s" ZT_EOL_S,scode,command.c_str(),responseBody.c_str());
			return 1;
		}
	} else if (command == "orbit") {
		const uint64_t worldId = Utils::hexStrToU64(arg1.c_str());
		const uint64_t seed = Utils::hexStrToU64(arg2.c_str());
		if ((worldId)&&(seed)) {
			char jsons[1024];
			OSUtils::ztsnprintf(jsons,sizeof(jsons),"{\"seed\":\"%s\"}",arg2.c_str());
			char cl[128];
			OSUtils::ztsnprintf(cl,sizeof(cl),"%u",(unsigned int)strlen(jsons));
			requestHeaders["Content-Type"] = "application/json";
			requestHeaders["Content-Length"] = cl;
			unsigned int scode = Http::POST(
				1024 * 1024 * 16,
				60000,
				(const struct sockaddr *)&addr,
				(std::string("/moon/") + arg1).c_str(),
				requestHeaders,
				jsons,
				(unsigned long)strlen(jsons),
				responseHeaders,
				responseBody);
			if (scode == 200) {
				printf("200 orbit OK" ZT_EOL_S);
				return 0;
			} else {
				printf("%u %s %s" ZT_EOL_S,scode,command.c_str(),responseBody.c_str());
				return 1;
			}
		}
	} else if (command == "deorbit") {
		unsigned int scode = Http::DEL(
			1024 * 1024 * 16,
			60000,
			(const struct sockaddr *)&addr,
			(std::string("/moon/") + arg1).c_str(),
			requestHeaders,
			responseHeaders,
			responseBody);
		if (scode == 200) {
			if (json) {
				printf("%s",cliFixJsonCRs(responseBody).c_str());
			} else {
				printf("200 deorbit OK" ZT_EOL_S);
			}
			return 0;
		} else {
			printf("%u %s %s" ZT_EOL_S,scode,command.c_str(),responseBody.c_str());
			return 1;
		}
	} else if (command == "set") {
		if (arg1.length() != 16) {
			fprintf(stderr,"invalid format: must be a 16-digit (network) ID\n");
			return 2;
		}
		if (!arg2.length()) {
			fprintf(stderr,"invalid format: include a property name to set\n");
			return 2;
		}
		std::size_t eqidx = arg2.find('=');
		if (eqidx != std::string::npos) {
			if ((arg2.substr(0,eqidx) == "allowManaged")||(arg2.substr(0,eqidx) == "allowGlobal")||(arg2.substr(0,eqidx) == "allowDefault")||(arg2.substr(0,eqidx) == "allowDNS")) {
				char jsons[1024];
				OSUtils::ztsnprintf(jsons,sizeof(jsons),"{\"%s\":%s}",
					arg2.substr(0,eqidx).c_str(),
					(((arg2.substr(eqidx,2) == "=t")||(arg2.substr(eqidx,2) == "=1")) ? "true" : "false"));
				char cl[128];
				OSUtils::ztsnprintf(cl,sizeof(cl),"%u",(unsigned int)strlen(jsons));
				requestHeaders["Content-Type"] = "application/json";
				requestHeaders["Content-Length"] = cl;
				unsigned int scode = Http::POST(
					1024 * 1024 * 16,
					60000,
					(const struct sockaddr *)&addr,
					(std::string("/network/") + arg1).c_str(),
					requestHeaders,
					jsons,
					(unsigned long)strlen(jsons),
					responseHeaders,
					responseBody);
				if (scode == 200) {
					printf("%s",cliFixJsonCRs(responseBody).c_str());
					return 0;
				} else {
					printf("%u %s %s" ZT_EOL_S,scode,command.c_str(),responseBody.c_str());
					return 1;
				}
			}
		} else {
			cliPrintHelp(argv[0],stderr);
			return 2;
		}
	} else if (command == "get") {
		if (arg1.length() != 16) {
			fprintf(stderr,"invalid format: must be a 16-digit (network) ID\n");
			return 2;
		}
		if (!arg2.length()) {
			fprintf(stderr,"invalid format: include a property name to get\n");
			return 2;
		}
		const unsigned int scode = Http::GET(1024 * 1024 * 16,60000,(const struct sockaddr *)&addr,"/network",requestHeaders,responseHeaders,responseBody);
		if (scode == 0) {
			printf("Error connecting to the ZeroTier service: %s\n\nPlease check that the service is running and that TCP port 9993 can be contacted via 127.0.0.1." ZT_EOL_S, responseBody.c_str());
			return 1;
		}
		nlohmann::json j;
		try {
			j = OSUtils::jsonParse(responseBody);
		} catch (std::exception &exc) {
			printf("%u %s invalid JSON response (%s)" ZT_EOL_S,scode,command.c_str(),exc.what());
			return 1;
		} catch ( ... ) {
			printf("%u %s invalid JSON response (unknown exception)" ZT_EOL_S,scode,command.c_str());
			return 1;
		}
		bool bNetworkFound = false;
		if (j.is_array()) {
			for(unsigned long i=0;i<j.size();++i) {
				nlohmann::json &n = j[i];
				if (n.is_object()) {
					if (n["id"] == arg1) {
						bNetworkFound = true;
						std::string aa;
						if (arg2 != "ip" && arg2 != "ip4" && arg2 != "ip6" && arg2 != "ip6plane" && arg2 != "ip6prefix") {
							aa.append(OSUtils::jsonString(n[arg2],"-")); // Standard network property field
							if (aa == "-") {
								printf("error, unknown property name\n");
								break;
							}
							printf("%s\n",aa.c_str());
							break;
						}
						nlohmann::json &assignedAddresses = n["assignedAddresses"];
						if (assignedAddresses.is_array()) {
							int matchingIdxs[ZT_MAX_ZT_ASSIGNED_ADDRESSES];
							int addressCountOfType = 0;
							for (int k = 0; k<std::min(ZT_MAX_ZT_ASSIGNED_ADDRESSES, (int)assignedAddresses.size());++k) {
								nlohmann::json &addr = assignedAddresses[k];
								if ((arg2 == "ip4" && addr.get<std::string>().find(".") != std::string::npos)
									|| ((arg2.find("ip6") == 0) && addr.get<std::string>().find(":") != std::string::npos)
									|| (arg2 == "ip")
									) {
									matchingIdxs[addressCountOfType++] = k;
								}
							}
							for (int k=0; k<addressCountOfType; k++) {
								nlohmann::json &addr = assignedAddresses[matchingIdxs[k]];
								if (!addr.is_string()) {
									continue;
								}
								if (arg2.find("ip6p") == 0) {
									if (arg2 == "ip6plane") {
										if (addr.get<std::string>().find("fc") == 0) {
											aa.append(addr.get<std::string>().substr(0,addr.get<std::string>().find("/")));
											if (k < addressCountOfType-1) aa.append("\n");
										}
									}
									if (arg2 == "ip6prefix") {
										if (addr.get<std::string>().find("fc") == 0) {
											aa.append(addr.get<std::string>().substr(0,addr.get<std::string>().find("/")).substr(0,24));
											if (k < addressCountOfType-1) aa.append("\n");
										}
									}
								}
								else {
									aa.append(addr.get<std::string>().substr(0,addr.get<std::string>().find("/")));
									if (k < addressCountOfType-1) aa.append("\n");
								}
							}
						}
						printf("%s\n",aa.c_str());
					}
				}
			}
		}
		if (!bNetworkFound) {
			fprintf(stderr,"unknown network ID, check that you are a member of the network\n");
		}
		if (scode == 200) {
			return 0;
		} else {
			printf("%u %s %s" ZT_EOL_S,scode,command.c_str(),responseBody.c_str());
			return 1;
		}
	} else if (command == "dump") {
		std::stringstream dump;
		dump << "platform: ";
#ifdef __APPLE__ 
		dump << "macOS" << ZT_EOL_S;
#elif defined(_WIN32)
		dump << "Windows" << ZT_EOL_S;
#elif defined(__LINUX__)
		dump << "Linux" << ZT_EOL_S;
#else
		dump << "other unix based OS" << ZT_EOL_S;
#endif
		dump << "zerotier version: " << ZEROTIER_ONE_VERSION_MAJOR << "."
			<< ZEROTIER_ONE_VERSION_MINOR << "." << ZEROTIER_ONE_VERSION_REVISION << ZT_EOL_S << ZT_EOL_S;

		// grab status
		dump << "status" << ZT_EOL_S << "------" << ZT_EOL_S;
		unsigned int scode = Http::GET(1024 * 1024 * 16,60000,(const struct sockaddr *)&addr,"/status",requestHeaders,responseHeaders,responseBody);
		if (scode != 200) {
			printf("Error connecting to the ZeroTier service: %s\n\nPlease check that the service is running and that TCP port 9993 can be contacted via 127.0.0.1." ZT_EOL_S, responseBody.c_str());
			return 1;
		}
		dump << responseBody << ZT_EOL_S;

		responseHeaders.clear();
		responseBody = "";

		// grab network list
		dump << ZT_EOL_S << "networks" << ZT_EOL_S << "--------" << ZT_EOL_S;
		scode = Http::GET(1024 * 1024 * 16,60000,(const struct sockaddr *)&addr,"/network",requestHeaders,responseHeaders,responseBody);
		if (scode != 200) {
			printf("Error connecting to the ZeroTier service: %s\n\nPlease check that the service is running and that TCP port 9993 can be contacted via 127.0.0.1." ZT_EOL_S, responseBody.c_str());
			return 1;
		}
		dump << responseBody << ZT_EOL_S;

		responseHeaders.clear();
		responseBody = "";

		// list peers
		dump << ZT_EOL_S << "peers" << ZT_EOL_S << "-----" << ZT_EOL_S;
		scode = Http::GET(1024 * 1024 * 16,60000,(const struct sockaddr *)&addr,"/peer",requestHeaders,responseHeaders,responseBody);
		if (scode != 200) {
			printf("Error connecting to the ZeroTier service: %s\n\nPlease check that the service is running and that TCP port 9993 can be contacted via 127.0.0.1." ZT_EOL_S, responseBody.c_str());
			return 1;
		}
		dump << responseBody << ZT_EOL_S;

		// get bonds
		dump << ZT_EOL_S << "bonds" << ZT_EOL_S << "-----" << ZT_EOL_S;
		scode = Http::GET(1024 * 1024 * 16,60000,(const struct sockaddr *)&addr,"/bonds",requestHeaders,responseHeaders,responseBody);
		if (scode != 200) {
			printf("Error connecting to the ZeroTier service: %s\n\nPlease check that the service is running and that TCP port 9993 can be contacted via 127.0.0.1." ZT_EOL_S, responseBody.c_str());
			return 1;
		}
		dump << responseBody << ZT_EOL_S;

		responseHeaders.clear();
		responseBody = "";

		dump << ZT_EOL_S << "local.conf" << ZT_EOL_S << "----------" << ZT_EOL_S;
		std::string localConf;
		OSUtils::readFile((homeDir + ZT_PATH_SEPARATOR_S + "local.conf").c_str(), localConf);
		if (localConf.empty()) {
			dump << "None Present" << ZT_EOL_S;
		}
		else {
			dump << localConf << ZT_EOL_S;
		}

		dump << ZT_EOL_S << "Network Interfaces" << ZT_EOL_S << "------------------" << ZT_EOL_S << ZT_EOL_S;
#ifdef __APPLE__
		CFArrayRef interfaces = SCNetworkInterfaceCopyAll();
		CFIndex size = CFArrayGetCount(interfaces);
		for(CFIndex i = 0; i < size; ++i) {
			SCNetworkInterfaceRef iface = (SCNetworkInterfaceRef)CFArrayGetValueAtIndex(interfaces, i);

			dump << "Interface " << i << ZT_EOL_S << "-----------" << ZT_EOL_S;
			CFStringRef tmp = SCNetworkInterfaceGetBSDName(iface);
			char stringBuffer[512] = {};
			CFStringGetCString(tmp,stringBuffer, sizeof(stringBuffer), kCFStringEncodingUTF8);
			dump << "Name: " << stringBuffer << ZT_EOL_S;
			std::string ifName(stringBuffer);
			int mtuCur, mtuMin, mtuMax;
			SCNetworkInterfaceCopyMTU(iface, &mtuCur, &mtuMin, &mtuMax);
			dump << "MTU: " << mtuCur << ZT_EOL_S;
			tmp = SCNetworkInterfaceGetHardwareAddressString(iface);
			CFStringGetCString(tmp, stringBuffer, sizeof(stringBuffer), kCFStringEncodingUTF8);
			dump << "MAC: " << stringBuffer << ZT_EOL_S;
			tmp = SCNetworkInterfaceGetInterfaceType(iface);
			CFStringGetCString(tmp, stringBuffer, sizeof(stringBuffer), kCFStringEncodingUTF8);
			dump << "Type: " << stringBuffer << ZT_EOL_S;
			dump << "Addresses:" << ZT_EOL_S;

			struct ifaddrs *ifap, *ifa;
			void *addr;
			getifaddrs(&ifap);
			for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
				if (strcmp(ifName.c_str(), ifa->ifa_name) == 0) {
					if (ifa->ifa_addr->sa_family == AF_INET) {
						struct sockaddr_in *ipv4 = (struct sockaddr_in*)ifa->ifa_addr;
						addr = &ipv4->sin_addr;
					} else if (ifa->ifa_addr->sa_family == AF_INET6) {
						struct sockaddr_in6 *ipv6 = (struct sockaddr_in6*)ifa->ifa_addr;
						addr = &ipv6->sin6_addr;
					} else {
						continue;
					}
					inet_ntop(ifa->ifa_addr->sa_family, addr, stringBuffer, sizeof(stringBuffer));
					dump << stringBuffer << ZT_EOL_S;
				}
			}

			dump << ZT_EOL_S;
		}


		FSRef fsref;
		UInt8 path[PATH_MAX];
		if (FSFindFolder(kUserDomain, kDesktopFolderType, kDontCreateFolder, &fsref) == noErr &&
				FSRefMakePath(&fsref, path, sizeof(path)) == noErr) {
			
		} else if (getenv("SUDO_USER")) {
			sprintf((char*)path, "/Users/%s/Desktop/", getenv("SUDO_USER"));
		} else {
			fprintf(stdout, "%s", dump.str().c_str());
			return 0;
		}

		sprintf((char*)path, "%s%szerotier_dump.txt", (char*)path, ZT_PATH_SEPARATOR_S);

		fprintf(stdout, "Writing dump to: %s\n", path);
		int fd = open((char*)path, O_CREAT|O_RDWR,0664);
		if (fd == -1) {
			fprintf(stderr, "Error creating file.\n");
			return 1;
		}
		write(fd, dump.str().c_str(), dump.str().size());	
		close(fd);
#elif defined(_WIN32)
		ULONG buffLen = 16384;
		PIP_ADAPTER_ADDRESSES addresses;
		
		ULONG ret = 0;
		do {
			addresses = (PIP_ADAPTER_ADDRESSES)malloc(buffLen);

			ret = GetAdaptersAddresses(AF_UNSPEC, 0, NULL, addresses, &buffLen);
			if (ret == ERROR_BUFFER_OVERFLOW) {
				free(addresses);
				addresses = NULL;
			}
			else {
				break;
			}
		} while (ret == ERROR_BUFFER_OVERFLOW);
		
		int i = 0;
		if (ret == NO_ERROR) {
			PIP_ADAPTER_ADDRESSES curAddr = addresses;
			while (curAddr) {
				dump << "Interface " << i << ZT_EOL_S << "-----------" << ZT_EOL_S;
				dump << "Name: " << curAddr->AdapterName << ZT_EOL_S;
				dump << "MTU: " << curAddr->Mtu << ZT_EOL_S;
				dump << "MAC: ";
				char macBuffer[64] = {};
				sprintf(macBuffer, "%02x:%02x:%02x:%02x:%02x:%02x",
					curAddr->PhysicalAddress[0],
					curAddr->PhysicalAddress[1],
					curAddr->PhysicalAddress[2],
					curAddr->PhysicalAddress[3],
					curAddr->PhysicalAddress[4],
					curAddr->PhysicalAddress[5]);
				dump << macBuffer << ZT_EOL_S;
				dump << "Type: " << curAddr->IfType << ZT_EOL_S;
				dump << "Addresses:" << ZT_EOL_S;
				PIP_ADAPTER_UNICAST_ADDRESS pUnicast = NULL;
				pUnicast = curAddr->FirstUnicastAddress;
				if (pUnicast) {
					for (int j = 0; pUnicast != NULL; ++j) {
						char buf[128] = {};
						DWORD bufLen = 128;
						LPSOCKADDR a = pUnicast->Address.lpSockaddr;
						WSAAddressToStringA(
							pUnicast->Address.lpSockaddr,
							pUnicast->Address.iSockaddrLength,
							NULL,
							buf,
							&bufLen
						);
						dump << buf << ZT_EOL_S;
						pUnicast = pUnicast->Next;
					}
				}

				curAddr = curAddr->Next;
				++i;
			}
		}
		if (addresses) {
			free(addresses);
			addresses = NULL;
		}
#elif defined(__LINUX__)
		struct ifreq ifr;
		struct ifconf ifc;
		char buf[1024];
		char stringBuffer[128];
		int success = 0;
		
		int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
		
		ifc.ifc_len = sizeof(buf);
		ifc.ifc_buf = buf;
		ioctl(sock, SIOCGIFCONF, &ifc);

		struct ifreq *it = ifc.ifc_req;
		const struct ifreq * const end = it + (ifc.ifc_len / sizeof(struct ifreq));
		int count = 0;
		for(; it != end; ++it) {
			strcpy(ifr.ifr_name, it->ifr_name);
			if(ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
				if (!(ifr.ifr_flags & IFF_LOOPBACK)) { // skip loopback
					dump << "Interface " << count++ << ZT_EOL_S << "-----------" << ZT_EOL_S;
					dump << "Name: " << ifr.ifr_name << ZT_EOL_S;
					if (ioctl(sock, SIOCGIFMTU, &ifr) == 0) {
						dump << "MTU: " << ifr.ifr_mtu << ZT_EOL_S;
					}
					if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
						unsigned char mac_addr[6];
						memcpy(mac_addr, ifr.ifr_hwaddr.sa_data, 6);
						char macStr[16];
						sprintf(macStr, "%02x:%02x:%02x:%02x:%02x:%02x",
								mac_addr[0],
								mac_addr[1],
								mac_addr[2],
								mac_addr[3],
								mac_addr[4],
								mac_addr[5]);
						dump << "MAC: " << macStr << ZT_EOL_S;
					}

					dump << "Addresses: " << ZT_EOL_S;
					struct ifaddrs *ifap, *ifa;
					void *addr;
					getifaddrs(&ifap);
					for(ifa = ifap; ifa; ifa = ifa->ifa_next) {
						if(strcmp(ifr.ifr_name, ifa->ifa_name) == 0) {
							if(ifa->ifa_addr->sa_family == AF_INET) {
								struct sockaddr_in *ipv4 = (struct sockaddr_in*)ifa->ifa_addr;
								addr = &ipv4->sin_addr;
							} else if (ifa->ifa_addr->sa_family == AF_INET6) {
								struct sockaddr_in6 *ipv6 = (struct sockaddr_in6*)ifa->ifa_addr;
								addr = &ipv6->sin6_addr;
							} else {
								continue;
							}
							inet_ntop(ifa->ifa_addr->sa_family, addr, stringBuffer, sizeof(stringBuffer));
							dump << stringBuffer << ZT_EOL_S;
						}
					}
				}
			}
		}
		close(sock);
		char cwd[PATH_MAX];
		getcwd(cwd, sizeof(cwd));
		sprintf(cwd, "%s%szerotier_dump.txt", cwd, ZT_PATH_SEPARATOR_S);
		fprintf(stdout, "Writing dump to: %s\n", cwd);
		int fd = open(cwd, O_CREAT|O_RDWR,0664);
		if (fd == -1) {
			fprintf(stderr, "Error creating file.\n");
			return 1;
		}
		write(fd, dump.str().c_str(), dump.str().size());	
		close(fd);
#else
	fprintf(stderr, "%s", dump.str().c_str());
#endif

		// fprintf(stderr, "%s\n", dump.str().c_str());

	} else {
		cliPrintHelp(argv[0],stderr);
		return 0;
	}

	return 0;
}

/****************************************************************************/
/* zerotier-idtool personality                                              */
/****************************************************************************/

static void idtoolPrintHelp(FILE *out,const char *pn)
{
	fprintf(out,
		"%s version %d.%d.%d" ZT_EOL_S,
		PROGRAM_NAME,
		ZEROTIER_ONE_VERSION_MAJOR, ZEROTIER_ONE_VERSION_MINOR, ZEROTIER_ONE_VERSION_REVISION);
	fprintf(out,
		COPYRIGHT_NOTICE ZT_EOL_S
		LICENSE_GRANT ZT_EOL_S);
	fprintf(out,"Usage: %s <command> [<args>]" ZT_EOL_S"" ZT_EOL_S"Commands:" ZT_EOL_S,pn);
	fprintf(out,"  generate [<identity.secret>] [<identity.public>] [<vanity>]" ZT_EOL_S);
	fprintf(out,"  validate <identity.secret/public>" ZT_EOL_S);
	fprintf(out,"  getpublic <identity.secret>" ZT_EOL_S);
	fprintf(out,"  sign <identity.secret> <file>" ZT_EOL_S);
	fprintf(out,"  verify <identity.secret/public> <file> <signature>" ZT_EOL_S);
	fprintf(out,"  initmoon <identity.public of first seed>" ZT_EOL_S);
	fprintf(out,"  genmoon <moon json>" ZT_EOL_S);
}

static Identity getIdFromArg(char *arg)
{
	Identity id;
	if ((strlen(arg) > 32)&&(arg[10] == ':')) { // identity is a literal on the command line
		if (id.fromString(arg))
			return id;
	} else { // identity is to be read from a file
		std::string idser;
		if (OSUtils::readFile(arg,idser)) {
			if (id.fromString(idser.c_str()))
				return id;
		}
	}
	return Identity();
}

#ifdef __WINDOWS__
static int idtool(int argc, _TCHAR* argv[])
#else
static int idtool(int argc,char **argv)
#endif
{
	if (argc < 2) {
		idtoolPrintHelp(stdout,argv[0]);
		return 1;
	}

	if (!strcmp(argv[1],"generate")) {
		uint64_t vanity = 0;
		int vanityBits = 0;
		if (argc >= 5) {
			vanity = Utils::hexStrToU64(argv[4]) & 0xffffffffffULL;
			vanityBits = 4 * (int)strlen(argv[4]);
			if (vanityBits > 40)
				vanityBits = 40;
		}

		Identity id;
		for(;;) {
			id.generate();
			if ((id.address().toInt() >> (40 - vanityBits)) == vanity) {
				if (vanityBits > 0) {
					fprintf(stderr,"vanity address: found %.10llx !\n",(unsigned long long)id.address().toInt());
				}
				break;
			} else {
				fprintf(stderr,"vanity address: tried %.10llx looking for first %d bits of %.10llx\n",(unsigned long long)id.address().toInt(),vanityBits,(unsigned long long)(vanity << (40 - vanityBits)));
			}
		}

		char idtmp[1024];
		std::string idser = id.toString(true,idtmp);
		if (argc >= 3) {
			if (!OSUtils::writeFile(argv[2],idser)) {
				fprintf(stderr,"Error writing to %s" ZT_EOL_S,argv[2]);
				return 1;
			} else printf("%s written" ZT_EOL_S,argv[2]);
			if (argc >= 4) {
				idser = id.toString(false,idtmp);
				if (!OSUtils::writeFile(argv[3],idser)) {
					fprintf(stderr,"Error writing to %s" ZT_EOL_S,argv[3]);
					return 1;
				} else printf("%s written" ZT_EOL_S,argv[3]);
			}
		} else printf("%s",idser.c_str());
	} else if (!strcmp(argv[1],"validate")) {
		if (argc < 3) {
			idtoolPrintHelp(stdout,argv[0]);
			return 1;
		}

		Identity id = getIdFromArg(argv[2]);
		if (!id) {
			fprintf(stderr,"Identity argument invalid or file unreadable: %s" ZT_EOL_S,argv[2]);
			return 1;
		}

		if (!id.locallyValidate()) {
			fprintf(stderr,"%s FAILED validation." ZT_EOL_S,argv[2]);
			return 1;
		} else printf("%s is a valid identity" ZT_EOL_S,argv[2]);
	} else if (!strcmp(argv[1],"getpublic")) {
		if (argc < 3) {
			idtoolPrintHelp(stdout,argv[0]);
			return 1;
		}

		Identity id = getIdFromArg(argv[2]);
		if (!id) {
			fprintf(stderr,"Identity argument invalid or file unreadable: %s" ZT_EOL_S,argv[2]);
			return 1;
		}

		char idtmp[1024];
		printf("%s",id.toString(false,idtmp));
	} else if (!strcmp(argv[1],"sign")) {
		if (argc < 4) {
			idtoolPrintHelp(stdout,argv[0]);
			return 1;
		}

		Identity id = getIdFromArg(argv[2]);
		if (!id) {
			fprintf(stderr,"Identity argument invalid or file unreadable: %s" ZT_EOL_S,argv[2]);
			return 1;
		}

		if (!id.hasPrivate()) {
			fprintf(stderr,"%s does not contain a private key (must use private to sign)" ZT_EOL_S,argv[2]);
			return 1;
		}

		std::string inf;
		if (!OSUtils::readFile(argv[3],inf)) {
			fprintf(stderr,"%s is not readable" ZT_EOL_S,argv[3]);
			return 1;
		}
		C25519::Signature signature = id.sign(inf.data(),(unsigned int)inf.length());
		char hexbuf[1024];
		printf("%s",Utils::hex(signature.data,ZT_C25519_SIGNATURE_LEN,hexbuf));
	} else if (!strcmp(argv[1],"verify")) {
		if (argc < 5) {
			idtoolPrintHelp(stdout,argv[0]);
			return 1;
		}

		Identity id = getIdFromArg(argv[2]);
		if (!id) {
			fprintf(stderr,"Identity argument invalid or file unreadable: %s" ZT_EOL_S,argv[2]);
			return 1;
		}

		std::string inf;
		if (!OSUtils::readFile(argv[3],inf)) {
			fprintf(stderr,"%s is not readable" ZT_EOL_S,argv[3]);
			return 1;
		}

		char buf[4096];
		std::string signature(buf,Utils::unhex(argv[4],buf,(unsigned int)sizeof(buf)));
		if ((signature.length() > ZT_ADDRESS_LENGTH)&&(id.verify(inf.data(),(unsigned int)inf.length(),signature.data(),(unsigned int)signature.length()))) {
			printf("%s signature valid" ZT_EOL_S,argv[3]);
		} else {
			signature.clear();
			if (OSUtils::readFile(argv[4],signature)) {
				signature.assign(buf,Utils::unhex(signature.c_str(),buf,(unsigned int)sizeof(buf)));
				if ((signature.length() > ZT_ADDRESS_LENGTH)&&(id.verify(inf.data(),(unsigned int)inf.length(),signature.data(),(unsigned int)signature.length()))) {
					printf("%s signature valid" ZT_EOL_S,argv[3]);
				} else {
					fprintf(stderr,"%s signature check FAILED" ZT_EOL_S,argv[3]);
					return 1;
				}
			} else {
				fprintf(stderr,"%s signature check FAILED" ZT_EOL_S,argv[3]);
				return 1;
			}
		}
	} else if (!strcmp(argv[1],"initmoon")) {
		if (argc < 3) {
			idtoolPrintHelp(stdout,argv[0]);
		} else {
			const Identity id = getIdFromArg(argv[2]);
			if (!id) {
				fprintf(stderr,"%s is not a valid identity" ZT_EOL_S,argv[2]);
				return 1;
			}

			C25519::Pair kp(C25519::generate());

			char idtmp[4096];
			nlohmann::json mj;
			mj["objtype"] = "world";
			mj["worldType"] = "moon";
			mj["updatesMustBeSignedBy"] = mj["signingKey"] = Utils::hex(kp.pub.data,ZT_C25519_PUBLIC_KEY_LEN,idtmp);
			mj["signingKey_SECRET"] = Utils::hex(kp.priv.data,ZT_C25519_PRIVATE_KEY_LEN,idtmp);
			mj["id"] = id.address().toString(idtmp);
			nlohmann::json seedj;
			seedj["identity"] = id.toString(false,idtmp);
			seedj["stableEndpoints"] = nlohmann::json::array();
			(mj["roots"] = nlohmann::json::array()).push_back(seedj);
			std::string mjd(OSUtils::jsonDump(mj));

			printf("%s" ZT_EOL_S,mjd.c_str());
		}
	} else if (!strcmp(argv[1],"genmoon")) {
		if (argc < 3) {
			idtoolPrintHelp(stdout,argv[0]);
		} else {
			std::string buf;
			if (!OSUtils::readFile(argv[2],buf)) {
				fprintf(stderr,"cannot read %s" ZT_EOL_S,argv[2]);
				return 1;
			}
			nlohmann::json mj(OSUtils::jsonParse(buf));

			const uint64_t id = Utils::hexStrToU64(OSUtils::jsonString(mj["id"],"0").c_str());
			if (!id) {
				fprintf(stderr,"ID in %s is invalid" ZT_EOL_S,argv[2]);
				return 1;
			}

			World::Type t;
			if (mj["worldType"] == "moon") {
				t = World::TYPE_MOON;
			} else if (mj["worldType"] == "planet") {
				t = World::TYPE_PLANET;
			} else {
				fprintf(stderr,"invalid worldType" ZT_EOL_S);
				return 1;
			}

			C25519::Pair signingKey;
			C25519::Public updatesMustBeSignedBy;
			Utils::unhex(OSUtils::jsonString(mj["signingKey"],"").c_str(),signingKey.pub.data,ZT_C25519_PUBLIC_KEY_LEN);
			Utils::unhex(OSUtils::jsonString(mj["signingKey_SECRET"],"").c_str(),signingKey.priv.data,ZT_C25519_PRIVATE_KEY_LEN);
			Utils::unhex(OSUtils::jsonString(mj["updatesMustBeSignedBy"],"").c_str(),updatesMustBeSignedBy.data,ZT_C25519_PUBLIC_KEY_LEN);

			std::vector<World::Root> roots;
			nlohmann::json &rootsj = mj["roots"];
			if (rootsj.is_array()) {
				for(unsigned long i=0;i<(unsigned long)rootsj.size();++i) {
					nlohmann::json &r = rootsj[i];
					if (r.is_object()) {
						roots.push_back(World::Root());
						roots.back().identity = Identity(OSUtils::jsonString(r["identity"],"").c_str());
						nlohmann::json &stableEndpointsj = r["stableEndpoints"];
						if (stableEndpointsj.is_array()) {
							for(unsigned long k=0;k<(unsigned long)stableEndpointsj.size();++k)
								roots.back().stableEndpoints.push_back(InetAddress(OSUtils::jsonString(stableEndpointsj[k],"").c_str()));
							std::sort(roots.back().stableEndpoints.begin(),roots.back().stableEndpoints.end());
						}
					}
				}
			}
			std::sort(roots.begin(),roots.end());

			const int64_t now = OSUtils::now();
			World w(World::make(t,id,now,updatesMustBeSignedBy,roots,signingKey));
			Buffer<ZT_WORLD_MAX_SERIALIZED_LENGTH> wbuf;
			w.serialize(wbuf);
			char fn[128];
			OSUtils::ztsnprintf(fn,sizeof(fn),"%.16llx.moon",w.id());
			OSUtils::writeFile(fn,wbuf.data(),wbuf.size());
			printf("wrote %s (signed world with timestamp %llu)" ZT_EOL_S,fn,(unsigned long long)now);
		}
	} else {
		idtoolPrintHelp(stdout,argv[0]);
		return 1;
	}

	return 0;
}

/****************************************************************************/
/* Unix helper functions and signal handlers                                */
/****************************************************************************/

#ifdef __UNIX_LIKE__
static void _sighandlerHup(int sig)
{
}
static void _sighandlerQuit(int sig)
{
	OneService *s = zt1Service;
	if (s)
		s->terminate();
	else exit(0);
}
#endif

// Drop privileges on Linux, if supported by libc etc. and "zerotier-one" user exists on system
#if defined(__LINUX__) && !defined(ZT_NO_CAPABILITIES)
#ifndef PR_CAP_AMBIENT
#define PR_CAP_AMBIENT 47
#define PR_CAP_AMBIENT_IS_SET 1
#define PR_CAP_AMBIENT_RAISE 2
#define PR_CAP_AMBIENT_LOWER 3
#define PR_CAP_AMBIENT_CLEAR_ALL 4
#endif
#define ZT_LINUX_USER "zerotier-one"
#define ZT_HAVE_DROP_PRIVILEGES 1
namespace {

// libc doesn't export capset, it is instead located in libcap
// We ignore libcap and call it manually.
struct cap_header_struct {
	__u32 version;
	int pid;
};
struct cap_data_struct {
	__u32 effective;
	__u32 permitted;
	__u32 inheritable;
};
static inline int _zt_capset(cap_header_struct* hdrp, cap_data_struct* datap) { return syscall(SYS_capset, hdrp, datap); }

static void _notDropping(const char *procName,const std::string &homeDir)
{
	struct stat buf;
	if (lstat(homeDir.c_str(),&buf) < 0) {
		if (buf.st_uid != 0 || buf.st_gid != 0) {
			fprintf(stderr, "%s: FATAL: failed to drop privileges and can't run as root since privileges were previously dropped (home directory not owned by root)" ZT_EOL_S,procName);
			exit(1);
		}
	}
	fprintf(stderr, "%s: WARNING: failed to drop privileges (kernel may not support required prctl features), running as root" ZT_EOL_S,procName);
}

static int _setCapabilities(int flags)
{
	cap_header_struct capheader = {_LINUX_CAPABILITY_VERSION_1, 0};
	cap_data_struct capdata;
	capdata.inheritable = capdata.permitted = capdata.effective = flags;
	return _zt_capset(&capheader, &capdata);
}

static void _recursiveChown(const char *path,uid_t uid,gid_t gid)
{
	struct dirent de;
	struct dirent *dptr;
	lchown(path,uid,gid);
	DIR *d = opendir(path);
	if (!d)
		return;
	dptr = (struct dirent *)0;
	for(;;) {
		if (readdir_r(d,&de,&dptr) != 0)
			break;
		if (!dptr)
			break;
		if ((strcmp(dptr->d_name,".") != 0)&&(strcmp(dptr->d_name,"..") != 0)&&(strlen(dptr->d_name) > 0)) {
			std::string p(path);
			p.push_back(ZT_PATH_SEPARATOR);
			p.append(dptr->d_name);
			_recursiveChown(p.c_str(),uid,gid); // will just fail and return on regular files
		}
	}
	closedir(d);
}

static void dropPrivileges(const char *procName,const std::string &homeDir)
{
	if (getuid() != 0)
		return;

	// dropPrivileges switches to zerotier-one user while retaining CAP_NET_ADMIN
	// and CAP_NET_RAW capabilities.
	struct passwd *targetUser = getpwnam(ZT_LINUX_USER);
	if (!targetUser)
		return;

	if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_IS_SET, CAP_NET_RAW, 0, 0) < 0) {
		// Kernel has no support for ambient capabilities.
		_notDropping(procName,homeDir);
		return;
	}
	if (prctl(PR_SET_SECUREBITS, SECBIT_KEEP_CAPS | SECBIT_NOROOT) < 0) {
		_notDropping(procName,homeDir);
		return;
	}

	// Change ownership of our home directory if everything looks good (does nothing if already chown'd)
	_recursiveChown(homeDir.c_str(),targetUser->pw_uid,targetUser->pw_gid);

	if (_setCapabilities((1 << CAP_NET_ADMIN) | (1 << CAP_NET_RAW) | (1 << CAP_SETUID) | (1 << CAP_SETGID) | (1 << CAP_NET_BIND_SERVICE)) < 0) {
		_notDropping(procName,homeDir);
		return;
	}

	int oldDumpable = prctl(PR_GET_DUMPABLE);
	if (prctl(PR_SET_DUMPABLE, 0) < 0) {
		// Disable ptracing. Otherwise there is a small window when previous
		// compromised ZeroTier process could ptrace us, when we still have CAP_SETUID.
		// (this is mitigated anyway on most distros by ptrace_scope=1)
		fprintf(stderr,"%s: FATAL: prctl(PR_SET_DUMPABLE) failed while attempting to relinquish root permissions" ZT_EOL_S,procName);
		exit(1);
	}

	// Relinquish root
	if (setgid(targetUser->pw_gid) < 0) {
		perror("setgid");
		exit(1);
	}
	if (setuid(targetUser->pw_uid) < 0) {
		perror("setuid");
		exit(1);
	}

	if (_setCapabilities((1 << CAP_NET_ADMIN) | (1 << CAP_NET_RAW) | (1 << CAP_NET_BIND_SERVICE)) < 0) {
		fprintf(stderr,"%s: FATAL: unable to drop capabilities after relinquishing root" ZT_EOL_S,procName);
		exit(1);
	}

	if (prctl(PR_SET_DUMPABLE, oldDumpable) < 0) {
		fprintf(stderr,"%s: FATAL: prctl(PR_SET_DUMPABLE) failed while attempting to relinquish root permissions" ZT_EOL_S,procName);
		exit(1);
	}

	if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, CAP_NET_ADMIN, 0, 0) < 0) {
		fprintf(stderr,"%s: FATAL: prctl(PR_CAP_AMBIENT,PR_CAP_AMBIENT_RAISE,CAP_NET_ADMIN) failed while attempting to relinquish root permissions" ZT_EOL_S,procName);
		exit(1);
	}
	if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, CAP_NET_RAW, 0, 0) < 0) {
		fprintf(stderr,"%s: FATAL: prctl(PR_CAP_AMBIENT,PR_CAP_AMBIENT_RAISE,CAP_NET_RAW) failed while attempting to relinquish root permissions" ZT_EOL_S,procName);
		exit(1);
	}
}

} // anonymous namespace
#endif // __LINUX__

/****************************************************************************/
/* Windows helper functions and signal handlers                             */
/****************************************************************************/

#ifdef __WINDOWS__
// Console signal handler routine to allow CTRL+C to work, mostly for testing
static BOOL WINAPI _winConsoleCtrlHandler(DWORD dwCtrlType)
{
	switch(dwCtrlType) {
		case CTRL_C_EVENT:
		case CTRL_BREAK_EVENT:
		case CTRL_CLOSE_EVENT:
		case CTRL_SHUTDOWN_EVENT:
			OneService *s = zt1Service;
			if (s)
				s->terminate();
			return TRUE;
	}
	return FALSE;
}

// TODO: revisit this with https://support.microsoft.com/en-us/help/947709/how-to-use-the-netsh-advfirewall-firewall-context-instead-of-the-netsh
static void _winPokeAHole()
{
	char myPath[MAX_PATH];
	DWORD ps = GetModuleFileNameA(NULL,myPath,sizeof(myPath));
	if ((ps > 0)&&(ps < (DWORD)sizeof(myPath))) {
		STARTUPINFOA startupInfo;
		PROCESS_INFORMATION processInfo;

		startupInfo.cb = sizeof(startupInfo);
		memset(&startupInfo,0,sizeof(STARTUPINFOA));
		memset(&processInfo,0,sizeof(PROCESS_INFORMATION));
		if (CreateProcessA(NULL,(LPSTR)(std::string("C:\\Windows\\System32\\netsh.exe advfirewall firewall delete rule name=\"ZeroTier One\" program=\"") + myPath + "\"").c_str(),NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&startupInfo,&processInfo)) {
			WaitForSingleObject(processInfo.hProcess,INFINITE);
			CloseHandle(processInfo.hProcess);
			CloseHandle(processInfo.hThread);
		}

		startupInfo.cb = sizeof(startupInfo);
		memset(&startupInfo,0,sizeof(STARTUPINFOA));
		memset(&processInfo,0,sizeof(PROCESS_INFORMATION));
		if (CreateProcessA(NULL,(LPSTR)(std::string("C:\\Windows\\System32\\netsh.exe advfirewall firewall add rule name=\"ZeroTier One\" dir=in action=allow program=\"") + myPath + "\" enable=yes").c_str(),NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&startupInfo,&processInfo)) {
			WaitForSingleObject(processInfo.hProcess,INFINITE);
			CloseHandle(processInfo.hProcess);
			CloseHandle(processInfo.hThread);
		}

		startupInfo.cb = sizeof(startupInfo);
		memset(&startupInfo,0,sizeof(STARTUPINFOA));
		memset(&processInfo,0,sizeof(PROCESS_INFORMATION));
		if (CreateProcessA(NULL,(LPSTR)(std::string("C:\\Windows\\System32\\netsh.exe advfirewall firewall add rule name=\"ZeroTier One\" dir=out action=allow program=\"") + myPath + "\" enable=yes").c_str(),NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&startupInfo,&processInfo)) {
			WaitForSingleObject(processInfo.hProcess,INFINITE);
			CloseHandle(processInfo.hProcess);
			CloseHandle(processInfo.hThread);
		}
	}
}

// Returns true if this is running as the local administrator
static BOOL IsCurrentUserLocalAdministrator(void)
{
	BOOL   fReturn         = FALSE;
	DWORD  dwStatus;
	DWORD  dwAccessMask;
	DWORD  dwAccessDesired;
	DWORD  dwACLSize;
	DWORD  dwStructureSize = sizeof(PRIVILEGE_SET);
	PACL   pACL            = NULL;
	PSID   psidAdmin       = NULL;

	HANDLE hToken              = NULL;
	HANDLE hImpersonationToken = NULL;

	PRIVILEGE_SET   ps;
	GENERIC_MAPPING GenericMapping;

	PSECURITY_DESCRIPTOR     psdAdmin           = NULL;
	SID_IDENTIFIER_AUTHORITY SystemSidAuthority = SECURITY_NT_AUTHORITY;

	const DWORD ACCESS_READ  = 1;
	const DWORD ACCESS_WRITE = 2;

	__try
	{
		if (!OpenThreadToken(GetCurrentThread(), TOKEN_DUPLICATE|TOKEN_QUERY,TRUE,&hToken))
		{
			if (GetLastError() != ERROR_NO_TOKEN)
				__leave;
			if (!OpenProcessToken(GetCurrentProcess(),TOKEN_DUPLICATE|TOKEN_QUERY, &hToken))
				__leave;
		}
		if (!DuplicateToken (hToken, SecurityImpersonation,&hImpersonationToken))
			__leave;
		if (!AllocateAndInitializeSid(&SystemSidAuthority, 2,
			SECURITY_BUILTIN_DOMAIN_RID,
			DOMAIN_ALIAS_RID_ADMINS,
			0, 0, 0, 0, 0, 0, &psidAdmin))
			__leave;
		psdAdmin = LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
		if (psdAdmin == NULL)
			__leave;
		if (!InitializeSecurityDescriptor(psdAdmin,SECURITY_DESCRIPTOR_REVISION))
			__leave;
		dwACLSize = sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE) + GetLengthSid(psidAdmin) - sizeof(DWORD);
		pACL = (PACL)LocalAlloc(LPTR, dwACLSize);
		if (pACL == NULL)
			__leave;
		if (!InitializeAcl(pACL, dwACLSize, ACL_REVISION2))
			__leave;
		dwAccessMask= ACCESS_READ | ACCESS_WRITE;
		if (!AddAccessAllowedAce(pACL, ACL_REVISION2, dwAccessMask, psidAdmin))
			__leave;
		if (!SetSecurityDescriptorDacl(psdAdmin, TRUE, pACL, FALSE))
			__leave;

		SetSecurityDescriptorGroup(psdAdmin, psidAdmin, FALSE);
		SetSecurityDescriptorOwner(psdAdmin, psidAdmin, FALSE);

		if (!IsValidSecurityDescriptor(psdAdmin))
			__leave;
		dwAccessDesired = ACCESS_READ;

		GenericMapping.GenericRead    = ACCESS_READ;
		GenericMapping.GenericWrite   = ACCESS_WRITE;
		GenericMapping.GenericExecute = 0;
		GenericMapping.GenericAll     = ACCESS_READ | ACCESS_WRITE;

		if (!AccessCheck(psdAdmin, hImpersonationToken, dwAccessDesired,
			&GenericMapping, &ps, &dwStructureSize, &dwStatus,
			&fReturn))
		{
			fReturn = FALSE;
			__leave;
		}
	}
	__finally
	{
		// Clean up.
		if (pACL) LocalFree(pACL);
		if (psdAdmin) LocalFree(psdAdmin);
		if (psidAdmin) FreeSid(psidAdmin);
		if (hImpersonationToken) CloseHandle (hImpersonationToken);
		if (hToken) CloseHandle (hToken);
	}

	return fReturn;
}
#endif // __WINDOWS__

/****************************************************************************/
/* main() and friends                                                       */
/****************************************************************************/

static void printHelp(const char *cn,FILE *out)
{
	fprintf(out,
		"%s version %d.%d.%d" ZT_EOL_S,
		PROGRAM_NAME,
		ZEROTIER_ONE_VERSION_MAJOR, ZEROTIER_ONE_VERSION_MINOR, ZEROTIER_ONE_VERSION_REVISION);
	fprintf(out,
		COPYRIGHT_NOTICE ZT_EOL_S
		LICENSE_GRANT ZT_EOL_S);
	fprintf(out,"Usage: %s [-switches] [home directory]" ZT_EOL_S"" ZT_EOL_S,cn);
	fprintf(out,"Available switches:" ZT_EOL_S);
	fprintf(out,"  -h                - Display this help" ZT_EOL_S);
	fprintf(out,"  -v                - Show version" ZT_EOL_S);
	fprintf(out,"  -U                - Skip privilege check and do not attempt to drop privileges" ZT_EOL_S);
	fprintf(out,"  -p<port>          - Port for UDP and TCP/HTTP (default: 9993, 0 for random)" ZT_EOL_S);

#ifdef __UNIX_LIKE__
	fprintf(out,"  -d                - Fork and run as daemon (Unix-ish OSes)" ZT_EOL_S);
#endif // __UNIX_LIKE__

#ifdef __WINDOWS__
	fprintf(out,"  -C                - Run from command line instead of as service (Windows)" ZT_EOL_S);
	fprintf(out,"  -I                - Install Windows service (Windows)" ZT_EOL_S);
	fprintf(out,"  -R                - Uninstall Windows service (Windows)" ZT_EOL_S);
	fprintf(out,"  -D                - Remove all instances of Windows tap device (Windows)" ZT_EOL_S);
#endif // __WINDOWS__

	fprintf(out,"  -i                - Generate and manage identities (zerotier-idtool)" ZT_EOL_S);
	fprintf(out,"  -q                - Query API (zerotier-cli)" ZT_EOL_S);
}

class _OneServiceRunner
{
public:
	_OneServiceRunner(const char *pn,const std::string &hd,unsigned int p) : progname(pn),returnValue(0),port(p),homeDir(hd) {}
	void threadMain()
		throw()
	{
		try {
			for(;;) {
				zt1Service = OneService::newInstance(homeDir.c_str(),port);
				switch(zt1Service->run()) {
					case OneService::ONE_STILL_RUNNING: // shouldn't happen, run() won't return until done
					case OneService::ONE_NORMAL_TERMINATION:
						break;
					case OneService::ONE_UNRECOVERABLE_ERROR:
						fprintf(stderr,"%s: fatal error: %s" ZT_EOL_S,progname,zt1Service->fatalErrorMessage().c_str());
						returnValue = 1;
						break;
					case OneService::ONE_IDENTITY_COLLISION: {
						delete zt1Service;
						zt1Service = (OneService *)0;
						std::string oldid;
						OSUtils::readFile((homeDir + ZT_PATH_SEPARATOR_S + "identity.secret").c_str(),oldid);
						if (oldid.length()) {
							OSUtils::writeFile((homeDir + ZT_PATH_SEPARATOR_S + "identity.secret.saved_after_collision").c_str(),oldid);
							OSUtils::rm((homeDir + ZT_PATH_SEPARATOR_S + "identity.secret").c_str());
							OSUtils::rm((homeDir + ZT_PATH_SEPARATOR_S + "identity.public").c_str());
						}
					}	continue; // restart!
				}
				break; // terminate loop -- normally we don't keep restarting
			}

			delete zt1Service;
			zt1Service = (OneService *)0;
		} catch ( ... ) {
			fprintf(stderr,"%s: unexpected exception starting main OneService instance" ZT_EOL_S,progname);
			returnValue = 1;
		}
	}
	const char *progname;
	unsigned int returnValue;
	unsigned int port;
	const std::string &homeDir;
};

#ifdef __WINDOWS__
int __cdecl _tmain(int argc, _TCHAR* argv[])
#else
int main(int argc,char **argv)
#endif
{
#ifdef __UNIX_LIKE__
	signal(SIGHUP,&_sighandlerHup);
	signal(SIGPIPE,SIG_IGN);
	signal(SIGIO,SIG_IGN);
	signal(SIGUSR1,SIG_IGN);
	signal(SIGUSR2,SIG_IGN);
	signal(SIGALRM,SIG_IGN);
	signal(SIGINT,&_sighandlerQuit);
	signal(SIGTERM,&_sighandlerQuit);
	signal(SIGQUIT,&_sighandlerQuit);
	signal(SIGINT,&_sighandlerQuit);

	/* Ensure that there are no inherited file descriptors open from a previous
	 * incarnation. This is a hack to ensure that GitHub issue #61 or variants
	 * of it do not return, and should not do anything otherwise bad. */
	{
		int mfd = STDIN_FILENO;
		if (STDOUT_FILENO > mfd) mfd = STDOUT_FILENO;
		if (STDERR_FILENO > mfd) mfd = STDERR_FILENO;
		for(int f=mfd+1;f<1024;++f)
			::close(f);
	}

	bool runAsDaemon = false;
#endif // __UNIX_LIKE__

#ifdef __WINDOWS__
	{
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2,2),&wsaData);
	}

#ifdef ZT_WIN_RUN_IN_CONSOLE
	bool winRunFromCommandLine = true;
#else
	bool winRunFromCommandLine = false;
#endif
#endif // __WINDOWS__

	if ((strstr(argv[0],"zerotier-idtool"))||(strstr(argv[0],"ZEROTIER-IDTOOL")))
		return idtool(argc,argv);
	if ((strstr(argv[0],"zerotier-cli"))||(strstr(argv[0],"ZEROTIER-CLI")))
		return cli(argc,argv);

	std::string homeDir;
	unsigned int port = ZT_DEFAULT_PORT;
	bool skipRootCheck = false;

	for(int i=1;i<argc;++i) {
		if (argv[i][0] == '-') {
			switch(argv[i][1]) {

				case 'p': // port -- for both UDP and TCP, packets and control plane
					port = Utils::strToUInt(argv[i] + 2);
					if (port > 0xffff) {
						printHelp(argv[0],stdout);
						return 1;
					}
					break;

#ifdef __UNIX_LIKE__
				case 'd': // Run in background as daemon
					runAsDaemon = true;
					break;
#endif // __UNIX_LIKE__

				case 'U':
					skipRootCheck = true;
					break;

				case 'v': // Display version
					printf("%d.%d.%d" ZT_EOL_S,ZEROTIER_ONE_VERSION_MAJOR,ZEROTIER_ONE_VERSION_MINOR,ZEROTIER_ONE_VERSION_REVISION);
					return 0;

				case 'i': // Invoke idtool personality
					if (argv[i][2]) {
						printHelp(argv[0],stdout);
						return 0;
					} else return idtool(argc-1,argv+1);

				case 'q': // Invoke cli personality
					if (argv[i][2]) {
						printHelp(argv[0],stdout);
						return 0;
					} else return cli(argc,argv);

#ifdef __WINDOWS__
				case 'C': // Run from command line instead of as Windows service
					winRunFromCommandLine = true;
					break;

				case 'I': { // Install this binary as a Windows service
						if (IsCurrentUserLocalAdministrator() != TRUE) {
							fprintf(stderr,"%s: must be run as a local administrator." ZT_EOL_S,argv[0]);
							return 1;
						}
						std::string ret(InstallService(ZT_SERVICE_NAME,ZT_SERVICE_DISPLAY_NAME,ZT_SERVICE_START_TYPE,ZT_SERVICE_DEPENDENCIES,ZT_SERVICE_ACCOUNT,ZT_SERVICE_PASSWORD));
						if (ret.length()) {
							fprintf(stderr,"%s: unable to install service: %s" ZT_EOL_S,argv[0],ret.c_str());
							return 3;
						}
						return 0;
					} break;

				case 'R': { // Uninstall this binary as Windows service
						if (IsCurrentUserLocalAdministrator() != TRUE) {
							fprintf(stderr,"%s: must be run as a local administrator." ZT_EOL_S,argv[0]);
							return 1;
						}
						std::string ret(UninstallService(ZT_SERVICE_NAME));
						if (ret.length()) {
							fprintf(stderr,"%s: unable to uninstall service: %s" ZT_EOL_S,argv[0],ret.c_str());
							return 3;
						}
						return 0;
					} break;

				case 'D': {
						std::string err = WindowsEthernetTap::destroyAllPersistentTapDevices();
						if (err.length() > 0) {
							fprintf(stderr,"%s: unable to uninstall one or more persistent tap devices: %s" ZT_EOL_S,argv[0],err.c_str());
							return 3;
						}
						return 0;
					} break;
#endif // __WINDOWS__

				case 'h':
				case '?':
				default:
					printHelp(argv[0],stdout);
					return 0;
			}
		} else {
			if (homeDir.length()) {
				printHelp(argv[0],stdout);
				return 0;
			} else {
				homeDir = argv[i];
			}
		}
	}

	if (!homeDir.length())
		homeDir = OneService::platformDefaultHomePath();
	if (!homeDir.length()) {
		fprintf(stderr,"%s: no home path specified and no platform default available" ZT_EOL_S,argv[0]);
		return 1;
	} else {
		std::vector<std::string> hpsp(OSUtils::split(homeDir.c_str(),ZT_PATH_SEPARATOR_S,"",""));
		std::string ptmp;
		if (homeDir[0] == ZT_PATH_SEPARATOR)
			ptmp.push_back(ZT_PATH_SEPARATOR);
		for(std::vector<std::string>::iterator pi(hpsp.begin());pi!=hpsp.end();++pi) {
			if (ptmp.length() > 0)
				ptmp.push_back(ZT_PATH_SEPARATOR);
			ptmp.append(*pi);
			if ((*pi != ".")&&(*pi != "..")) {
				if (!OSUtils::mkdir(ptmp))
					throw std::runtime_error("home path does not exist, and could not create. Please verify local system permissions.");
			}
		}
	}

	// This can be removed once the new controller code has been around for many versions
	if (OSUtils::fileExists((homeDir + ZT_PATH_SEPARATOR_S + "controller.db").c_str(),true)) {
		fprintf(stderr,"%s: FATAL: an old controller.db exists in %s -- see instructions in controller/README.md for how to migrate!" ZT_EOL_S,argv[0],homeDir.c_str());
		return 1;
	}

#ifdef __UNIX_LIKE__
#ifndef ZT_ONE_NO_ROOT_CHECK
	if ((!skipRootCheck)&&(getuid() != 0)) {
		fprintf(stderr,"%s: must be run as root (uid 0)" ZT_EOL_S,argv[0]);
		return 1;
	}
#endif // !ZT_ONE_NO_ROOT_CHECK
	if (runAsDaemon) {
		long p = (long)fork();
		if (p < 0) {
			fprintf(stderr,"%s: could not fork" ZT_EOL_S,argv[0]);
			return 1;
		} else if (p > 0)
			return 0; // forked
		// else p == 0, so we are daemonized
	}
#endif // __UNIX_LIKE__

#ifdef __WINDOWS__
	// Uninstall legacy tap devices. New devices will automatically be installed and configured
	// when tap instances are created.
	WindowsEthernetTap::destroyAllLegacyPersistentTapDevices();

	if (winRunFromCommandLine) {
		// Running in "interactive" mode (mostly for debugging)
		if (IsCurrentUserLocalAdministrator() != TRUE) {
			if (!skipRootCheck) {
				fprintf(stderr,"%s: must be run as a local administrator." ZT_EOL_S,argv[0]);
				return 1;
			}
		} else {
			_winPokeAHole();
		}
		SetConsoleCtrlHandler(&_winConsoleCtrlHandler,TRUE);
		// continues on to ordinary command line execution code below...
	} else {
		// Running from service manager
		_winPokeAHole();
		ZeroTierOneService zt1WindowsService;
		if (CServiceBase::Run(zt1WindowsService) == TRUE) {
			return 0;
		} else {
			fprintf(stderr,"%s: unable to start service (try -h for help)" ZT_EOL_S,argv[0]);
			return 1;
		}
	}
#endif // __WINDOWS__

#ifdef __UNIX_LIKE__
#ifdef ZT_HAVE_DROP_PRIVILEGES
	if (!skipRootCheck)
		dropPrivileges(argv[0],homeDir);
#endif

	std::string pidPath(homeDir + ZT_PATH_SEPARATOR_S + ZT_PID_PATH);
	{
		// Write .pid file to home folder
		FILE *pf = fopen(pidPath.c_str(),"w");
		if (pf) {
			fprintf(pf,"%ld",(long)getpid());
			fclose(pf);
		}
	}
#endif // __UNIX_LIKE__

	_OneServiceRunner thr(argv[0],homeDir,port);
	thr.threadMain();
	//Thread::join(Thread::start(&thr));

#ifdef __UNIX_LIKE__
	OSUtils::rm(pidPath.c_str());
#endif

	return thr.returnValue;
}
