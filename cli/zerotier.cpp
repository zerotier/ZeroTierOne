/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
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
 */

// Note: unlike the rest of ZT's code base, this requires C++11 due to
// the JSON library it uses and other things.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "../node/Constants.hpp"
#include "../version.h"
#include "../osdep/OSUtils.hpp"
#include "../ext/json/json.hpp"

#ifdef __WINDOWS__
#include <WinSock2.h>
#include <windows.h>
#include <tchar.h>
#include <wchar.h>
#else
#include <ctype.h>
#include <unistd.h>
#endif

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <tuple>

#include <curl/curl.h>

using json = nlohmann::json;
using namespace ZeroTier;

#define ZT_CLI_FLAG_VERBOSE 'v'
#define ZT_CLI_FLAG_UNSAFE_SSL 'X'

struct CLIState
{
	std::string atname;
	std::string command;
	std::vector<std::string> args;
	std::map<char,std::string> opts;
	json settings;
};

namespace {

static std::string trimString(const std::string &s)
{
	unsigned long end = (unsigned long)s.length();
	while (end) {
		char c = s[end - 1];
		if ((c == ' ')||(c == '\r')||(c == '\n')||(!c)||(c == '\t'))
			--end;
		else break;
	}
	unsigned long start = 0;
	while (start < end) {
		char c = s[start];
		if ((c == ' ')||(c == '\r')||(c == '\n')||(!c)||(c == '\t'))
			++start;
		else break;
	}
	return s.substr(start,end - start);
}

static inline std::string getSettingsFilePath()
{
#ifdef __WINDOWS__
#else
	const char *home = getenv("HOME");
	if (!home)
		home = "/";
	return (std::string(home) + "/.zerotierCliSettings");
#endif
}

static bool saveSettings(const json &settings)
{
	std::string sfp(getSettingsFilePath().c_str());
	std::string buf(settings.dump(2));
	if (OSUtils::writeFile(sfp.c_str(),buf)) {
		OSUtils::lockDownFile(sfp.c_str(),false);
		return true;
	}
	return false;
}

static void dumpHelp()
{
	std::cout << "ZeroTier Newer-Spiffier CLI " << ZEROTIER_ONE_VERSION_MAJOR << "." << ZEROTIER_ONE_VERSION_MINOR << "." << ZEROTIER_ONE_VERSION_REVISION << std::endl;
	std::cout << "(c)2016 ZeroTier, Inc. / Licensed under the GNU GPL v3" << std::endl;
	std::cout << std::endl;
	std::cout << "Configuration path: " << getSettingsFilePath() << std::endl;
	std::cout << std::endl;
	std::cout << "Usage: zerotier [-option] [@name] <command> [<command options>]" << std::endl;
	std::cout << std::endl;
	std::cout << "Options:" << std::endl;
	std::cout << "  -v                                  - Verbose JSON output" << std::endl;
	std::cout << "  -X                                  - Do not check SSL certs (CAUTION!)" << std::endl;
	std::cout << std::endl;
	std::cout << "CLI Configuration Commands:" << std::endl;
	std::cout << "  cli-set <setting> <value>           - Set a CLI option ('cli-set help')" << std::endl;
	std::cout << "  cli-ls                              - List configured @things" << std::endl;
	std::cout << "  cli-rm @name                        - Remove a configured @thing" << std::endl;
	std::cout << "  cli-add-zt @name <url> <auth>       - Add a ZeroTier service" << std::endl;
	std::cout << "  cli-add-central @name <url> <auth>  - Add ZeroTier Central instance" << std::endl;
	std::cout << std::endl;
	std::cout << "ZeroTier One Service Commands:" << std::endl;
	std::cout << "  ls                                  - List currently joined networks" << std::endl;
	std::cout << "  join <network> [opt=value ...]      - Join a network" << std::endl;
	std::cout << "  leave <network>                     - Leave a network" << std::endl;
	std::cout << "  peers                               - List ZeroTier VL1 peers" << std::endl;
	std::cout << "  show [<network/peer address>]       - Get info about self or object" << std::endl;
	std::cout << std::endl;
	std::cout << "Network Controller Commands:" << std::endl;
	std::cout << "  net-create                          - Create a new network" << std::endl;
	std::cout << "  net-rm <network>                    - Delete a network (CAUTION!)" << std::endl;
	std::cout << "  net-ls                              - List administered networks" << std::endl;
	std::cout << "  net-members <network>               - List members of a network" << std::endl;
	std::cout << "  net-show <network> [<address>]      - Get network or member info" << std::endl;
	std::cout << "  net-auth <network> <address>        - Authorize a member" << std::endl;
	std::cout << "  net-set <path> <value>              - See 'net-set help'" << std::endl;
	std::cout << std::endl;
	std::cout << "Identity Commands:" << std::endl;
	std::cout << "  id-generate [<vanity prefix>]       - Generate a ZeroTier identity" << std::endl;
	std::cout << "  id-validate <identity>              - Locally validate an identity" << std::endl;
	std::cout << "  id-sign <identity> <file>           - Sign a file" << std::endl;
	std::cout << "  id-verify <secret> <file> <sig>     - Verify a file's signature" << std::endl;
	std::cout << "  id-getpublic <secret>               - Get full identity's public portion" << std::endl;
	std::cout << std::endl;
}

static size_t _curlStringAppendCallback(void *contents,size_t size,size_t nmemb,void *stdstring)
{
	size_t totalSize = size * nmemb;
	reinterpret_cast<std::string *>(stdstring)->append((const char *)contents,totalSize);
	return totalSize;
}

static std::tuple<int,std::string> GET(const CLIState &state,const std::map<std::string,std::string> &headers,const std::string &url)
{
	std::string body;
	char errbuf[CURL_ERROR_SIZE];
	char urlbuf[4096];

	CURL *curl = curl_easy_init();
	if (!curl) {
		std::cerr << "FATAL: curl_easy_init() failed" << std::endl;
		exit(-1);
	}

	curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,_curlStringAppendCallback);
	curl_easy_setopt(curl,CURLOPT_WRITEDATA,(void *)&body);
	curl_easy_setopt(curl,CURLOPT_USERAGENT,"ZeroTier-CLI");
	curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,(state.opts.count(ZT_CLI_FLAG_UNSAFE_SSL) > 0) ? 0L : 1L);
	curl_easy_setopt(curl,CURLOPT_ERRORBUFFER,errbuf);
	curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);

	Utils::scopy(urlbuf,sizeof(urlbuf),url.c_str());
	curl_easy_setopt(curl,CURLOPT_URL,urlbuf);

	struct curl_slist *hdrs = (struct curl_slist *)0;
	for(std::map<std::string,std::string>::const_iterator i(headers.begin());i!=headers.end();++i) {
		std::string htmp(i->first);
		htmp.append(": ");
		htmp.append(i->second);
		hdrs = curl_slist_append(hdrs,htmp.c_str());
	}
	if (hdrs)
		curl_easy_setopt(curl,CURLOPT_HTTPHEADER,hdrs);

	memset(errbuf,0,sizeof(errbuf));
	CURLcode res = curl_easy_perform(curl);
	errbuf[CURL_ERROR_SIZE-1] = (char)0; // sanity check

	if (res != CURLE_OK)
		return std::make_tuple(-1,std::string(errbuf));

	int rc = (int)curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE);

	curl_easy_cleanup(curl);
	if (hdrs)
		curl_slist_free_all(hdrs);

	return std::make_tuple(rc,body);
}

} // anonymous namespace

//////////////////////////////////////////////////////////////////////////////

#ifdef __WINDOWS__
int _tmain(int argc, _TCHAR* argv[])
#else
int main(int argc,char **argv)
#endif
{
#ifdef __WINDOWS__
	{
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2,2),&wsaData);
	}
#endif

	curl_global_init(CURL_GLOBAL_DEFAULT);

	CLIState state;

	for(int i=1;i<argc;++i) {
		if ((i == 1)&&(argv[i][0] == '@')) {
			state.atname = argv[i];
		} else if (state.command.length() == 0) {
			if (argv[i][0] == '-') {
				if (!argv[i][1]) {
					dumpHelp();
					return -1;
				} else if (argv[i][2]) {
					state.opts[argv[i][1]] = argv[i] + 2;
				} else {
					state.opts[argv[i][1]] = "";
				}
			} else {
				state.command = argv[i];
			}
		} else {
			state.args.push_back(std::string(argv[i]));
		}
	}

	{
		std::string buf;
		if (OSUtils::readFile(getSettingsFilePath().c_str(),buf))
			state.settings = json::parse(buf);

		if (state.settings.empty()) {
			// Default settings
			state.settings = {
				{ "configVersion", 1 },
				{ "things", {
					{ "my.zerotier.com", {
						{ "type", "central" },
						{ "url", "https://my.zerotier.com/" },
						{ "auth", "" }
					}},
					{ "local", {
						{ "type", "one" },
						{ "url", "" },
						{ "auth", "" }
					}}
				}},
				{ "defaultController", "@my.zerotier.com" },
				{ "defaultOne", "@local" }
			};

			std::string oneHome(OSUtils::platformDefaultHomePath());
			std::string authToken,portStr;
			bool initSuccess = false;
			if (OSUtils::readFile((oneHome + ZT_PATH_SEPARATOR_S + "authtoken.secret").c_str(),authToken)&&OSUtils::readFile((oneHome + ZT_PATH_SEPARATOR_S + "zerotier-one.port").c_str(),portStr)) {
				portStr = trimString(portStr);
				authToken = trimString(authToken);
				int port = Utils::strToInt(portStr.c_str());
				if (((port > 0)&&(port < 65536))&&(authToken.length() > 0)) {
					state.settings["things"]["local"]["url"] = (std::string("http://127.0.0.1:") + portStr + "/");
					state.settings["things"]["local"]["auth"] = authToken;
					initSuccess = true;
				}
			}

			if (!saveSettings(state.settings)) {
				std::cerr << "FATAL: unable to write " << getSettingsFilePath() << std::endl;
				exit(-1);
			}

			if (initSuccess) {
				std::cerr << "INFO: initialized new config at " << getSettingsFilePath() << std::endl;
			} else {
				std::cerr << "INFO: initialized new config at " << getSettingsFilePath() << " but could not auto-init local ZeroTier One service config from " << oneHome << " -- you will need to set local service URL and port manually if you want to control a local instance of ZeroTier One. (This happens if you are not root/administrator.)" << std::endl;
			}
		}
	}

	if ((state.command.length() == 0)||(state.command == "help")) {
		dumpHelp();
		return -1;
	} else if (state.command == "cli-set") {
	} else if (state.command == "cli-ls") {
	} else if (state.command == "cli-rm") {
	} else if (state.command == "cli-add-zt") {
	} else if (state.command == "cli-add-central") {
	} else if (state.command == "ls") {
	} else if (state.command == "join") {
	} else if (state.command == "leave") {
	} else if (state.command == "peers") {
	} else if (state.command == "show") {
	} else if (state.command == "net-create") {
	} else if (state.command == "net-rm") {
	} else if (state.command == "net-ls") {
	} else if (state.command == "net-members") {
	} else if (state.command == "net-show") {
	} else if (state.command == "net-auth") {
	} else if (state.command == "net-set") {
	} else if (state.command == "id-generate") {
	} else if (state.command == "id-validate") {
	} else if (state.command == "id-sign") {
	} else if (state.command == "id-verify") {
	} else if (state.command == "id-getpublic") {
	} else {
		dumpHelp();
		return -1;
	}

	curl_global_cleanup();

	return 0;
}
