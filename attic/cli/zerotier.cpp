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
#include "../node/Identity.hpp"
#include "../version.h"
#include "../osdep/OSUtils.hpp"
#include "../ext/offbase/json/json.hpp"

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
#include <regex>

#include <curl/curl.h>

using json = nlohmann::json;
using namespace ZeroTier;

#define ZT_CLI_FLAG_VERBOSE 'v'
#define ZT_CLI_FLAG_UNSAFE_SSL 'X'

#define REQ_GET  0
#define REQ_POST 1
#define REQ_DEL  2

#define OK_STR   "[OK  ]: "
#define FAIL_STR "[FAIL]: "
#define WARN_STR "[WARN]: "
#define INVALID_ARGS_STR "Invalid args. Usage: " 

struct CLIState
{
	std::string atname;
	std::string command;
	std::string url;
	std::map<std::string,std::string> reqHeaders;
	std::vector<std::string> args;
	std::map<char,std::string> opts;
	json settings;
};

namespace {

static Identity getIdFromArg(char *arg)
{
	Identity id;
	if ((strlen(arg) > 32)&&(arg[10] == ':')) { // identity is a literal on the command line
		if (id.fromString(arg))
			return id;
	} else { // identity is to be read from a file
		std::string idser;
		if (OSUtils::readFile(arg,idser)) {
			if (id.fromString(idser))
				return id;
		}
	}
	return Identity();
}

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

static bool saveSettingsBackup(CLIState &state)
{
	std::string sfp(getSettingsFilePath().c_str());
	if(state.settings.find("generateBackupConfig") != state.settings.end() 
		&& state.settings["generateBackupConfig"].get<std::string>() == "true") {
		std::string backup_file = getSettingsFilePath() + ".bak";
		if(!OSUtils::writeFile(sfp.c_str(), state.settings.dump(2))) {
			OSUtils::lockDownFile(sfp.c_str(),false);
			std::cout << WARN_STR << "unable to write backup config file" << std::endl;
			return false;
		}
		return true;
	}
	return false;
}

static bool saveSettings(CLIState &state)
{
	std::string sfp(getSettingsFilePath().c_str());
	if(OSUtils::writeFile(sfp.c_str(), state.settings.dump(2))) {
		OSUtils::lockDownFile(sfp.c_str(),false);
		std::cout << OK_STR << "changes saved." << std::endl;
		return true;
	}
	std::cout << FAIL_STR << "unable to write to " << sfp << std::endl;
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
	std::cout << "  -verbose                            - Verbose JSON output" << std::endl;
	std::cout << "  -X                                  - Do not check SSL certs (CAUTION!)" << std::endl;
	std::cout << std::endl;
	std::cout << "CLI Configuration Commands:" << std::endl;
	std::cout << "  cli-set <setting> <value>           - Set a CLI option ('cli-set help')" << std::endl;
	std::cout << "  cli-unset <setting> <value>         - Un-sets a CLI option ('cli-unset help')" << std::endl;
	std::cout << "  cli-ls                              - List configured @things" << std::endl;
	std::cout << "  cli-rm @name                        - Remove a configured @thing" << std::endl;
	std::cout << "  cli-add-zt @name <url> <auth>       - Add a ZeroTier service" << std::endl;
	std::cout << "  cli-add-central @name <url> <auth>  - Add ZeroTier Central instance" << std::endl;
	std::cout << std::endl;
	std::cout << "ZeroTier One Service Commands:" << std::endl;
	std::cout << "  -v / -version                       - Displays default local instance's version'" << std::endl;
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
	std::cout << "  net-unauth <network> <address>      - De-authorize a member" << std::endl;
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

static std::tuple<int,std::string> REQUEST(int requestType, CLIState &state, const std::map<std::string,std::string> &headers, const std::string &postfield, const std::string &url)
{
	std::string body;
	char errbuf[CURL_ERROR_SIZE];
	char urlbuf[4096];

	CURL *curl;
	curl = curl_easy_init();
	if (!curl) {
		std::cerr << "FATAL: curl_easy_init() failed" << std::endl;
		exit(-1);
	}

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

	//curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
	curl_easy_setopt(curl,CURLOPT_WRITEDATA,(void *)&body);
	curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,_curlStringAppendCallback);

	if(std::find(state.args.begin(), state.args.end(), "-X") == state.args.end())
			curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,(state.opts.count(ZT_CLI_FLAG_UNSAFE_SSL) > 0) ? 0L : 1L);

	if(requestType == REQ_POST) {
		curl_easy_setopt(curl, CURLOPT_POST, 1);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfield.c_str());
	}
	if(requestType == REQ_DEL)
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
	if(requestType == REQ_GET) {
		curl_easy_setopt(curl,CURLOPT_ERRORBUFFER,errbuf);
		curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);
	}

	curl_easy_setopt(curl,CURLOPT_USERAGENT,"ZeroTier-CLI");
	CURLcode res = curl_easy_perform(curl);

	errbuf[CURL_ERROR_SIZE-1] = (char)0; // sanity check

	if (res != CURLE_OK)
		return std::make_tuple(-1,std::string(errbuf));

    long response_code;
	int rc = (int)curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE, &response_code);

	if(response_code == 401) { std::cout << FAIL_STR << response_code << "Unauthorized." << std::endl; exit(0); }
	else if(response_code == 403) { std::cout << FAIL_STR << response_code << "Forbidden." << std::endl; exit(0); }
	else if(response_code == 404) { std::cout << FAIL_STR << response_code << "Not found." << std::endl; exit(0); }
	else if(response_code == 408) { std::cout << FAIL_STR << response_code << "Request timed out." << std::endl; exit(0); }
	else if(response_code != 200) { std::cout << FAIL_STR << response_code << "Unable to process request." << std::endl; exit(0); }

	curl_easy_cleanup(curl);
	if (hdrs)
		curl_slist_free_all(hdrs);
	return std::make_tuple(response_code,body);
}

} // anonymous namespace

//////////////////////////////////////////////////////////////////////////////

// Check for user-specified @thing config
// Make sure it @thing makes sense
// Apply appropriate request headers
static void checkForThing(CLIState &state, std::string thingType, bool warnNoThingProvided)
{
	std::string configName;
	if(state.atname.length()) {
		configName = state.atname.erase(0,1);
		// make sure specified @thing makes sense in the context of the command
		if(thingType == "one" && state.settings["things"][configName]["type"].get<std::string>() != "one") {
			std::cout << FAIL_STR << "A ZeroTier Central config was specified for a ZeroTier One command." << std::endl;
			exit(0);
		}
		if(thingType == "central" && state.settings["things"][configName]["type"].get<std::string>() != "central") {
			std::cout << FAIL_STR << "A ZeroTier One config was specified for a ZeroTier Central command." << std::endl;
			exit(0);
		}
	}
	else { // no @thing specified, check for defaults depending on type
		if(thingType == "one") {
			if(state.settings.find("defaultOne") != state.settings.end()) {
				if(warnNoThingProvided) 
					std::cout << WARN_STR << "No @thing specified, assuming default for ZeroTier One command: " << state.settings["defaultOne"].get<std::string>().c_str() << std::endl;
				configName = state.settings["defaultOne"].get<std::string>().erase(0,1); // get default
			}
			else {
				std::cout << WARN_STR << "No @thing specified, and no default is known." << std::endl;
				std::cout << "HELP: To set a default: zerotier cli-set defaultOne @my_default_thing" << std::endl;
				exit(0);
			}
		}
		if(thingType == "central") {
			if(state.settings.find("defaultCentral") != state.settings.end()) {
				if(warnNoThingProvided) 
					std::cout << WARN_STR << "No @thing specified, assuming default for ZeroTier Central command: " << state.settings["defaultCentral"].get<std::string>().c_str() << std::endl;
				configName = state.settings["defaultCentral"].get<std::string>().erase(0,1); // get default
			}
			else {
				std::cout << WARN_STR << "No @thing specified, and no default is known." << std::endl;
				std::cout << "HELP: To set a default: zerotier cli-set defaultCentral @my_default_thing" << std::endl;
				exit(0);
			}
		}
	}
	// Apply headers
	if(thingType == "one") {
		state.reqHeaders["X-ZT1-Auth"] = state.settings["things"][configName]["auth"];
	}
	if(thingType == "central"){
		state.reqHeaders["Content-Type"] = "application/json";
		state.reqHeaders["Authorization"] = "Bearer " + state.settings["things"][configName]["auth"].get<std::string>();
		state.reqHeaders["Accept"] = "application/json";
	}
	state.url = state.settings["things"][configName]["url"];
}

static bool checkURL(std::string url)
{
	// TODO
	return true;
}

static std::string getLocalVersion(CLIState &state)
{
	json result;
	std::tuple<int,std::string> res;
	checkForThing(state,"one",false);
	res = REQUEST(REQ_GET,state,state.reqHeaders,"",state.url + "/status");
	if(std::get<0>(res) == 200) {		
		result = json::parse(std::get<1>(res));
		return result["version"].get<std::string>();
	}
	return "---";
}

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
	std::string arg1, arg2, authToken;

	for(int i=1;i<argc;++i) {
		if (argv[i][0] == '@') {
			state.atname = argv[i];
		} 
		else if (state.command.length() == 0) {
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
		} 
		else {
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
			std::string portStr;
			bool initSuccess = false;
			std::string path = oneHome + ZT_PATH_SEPARATOR_S ;
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

			if (!saveSettings(state)) {
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

	// PRE-REQUEST SETUP

	json result;
	std::tuple<int,std::string> res;
	std::string url = "";

	// META

	if ((state.command.length() == 0)||(state.command == "help")) {
		dumpHelp();
		return -1;
	} 

	// zerotier version
	else if (state.command == "v" || state.command == "version") {
		std::cout << getLocalVersion(state) << std::endl;
		return 1;
	}

	// zerotier cli-set <setting> <value>
	else if (state.command == "cli-set") {
		if(argc != 4) {
			std::cerr << INVALID_ARGS_STR << "zerotier cli-set <setting> <value>" << std::endl;
			return 1;
		}
		std::string settingName, settingValue;
		if(state.atname.length()) { // User provided @thing erroneously, we will ignore it and adjust argument indices
			settingName = argv[3];
			settingValue = argv[4];
		}
		else {
			settingName = argv[2];
			settingValue = argv[3];
		}
		saveSettingsBackup(state);
		state.settings[settingName] = settingValue; // changes
		saveSettings(state);
	}

	// zerotier cli-unset <setting>
	else if (state.command == "cli-unset") {
		if(argc != 3) {
			std::cerr << INVALID_ARGS_STR << "zerotier cli-unset <setting>" << std::endl;
			return 1;
		}
		std::string settingName;
		if(state.atname.length()) // User provided @thing erroneously, we will ignore it and adjust argument indices
			settingName = argv[3];
		else
			settingName = argv[2];
		saveSettingsBackup(state);
		state.settings.erase(settingName); // changes
		saveSettings(state);	
	} 

	// zerotier @thing_to_remove cli-rm --- removes the configuration
	else if (state.command == "cli-rm") {
		if(argc != 3) {
			std::cerr << INVALID_ARGS_STR << "zerotier cli-rm <@thing>" << std::endl;
			return 1;
		}
		if(state.settings["things"].find(state.atname) != state.settings["things"].end()) {
			if(state.settings["defaultOne"] == state.atname) {
				std::cout << "WARNING: The config you're trying to remove is currently set as your default. Set a new default first!" << std::endl;
				std::cout << "      | Usage: zerotier set defaultOne @your_other_thing" << std::endl;
			}
			else {
				state.settings["things"].erase(state.atname.c_str());
				saveSettings(state);
			}
		}
	} 

	// zerotier cli-add-zt <shortname> <url> <auth>
	// TODO: Check for malformed urls/auth
	else if (state.command == "cli-add-zt") {
		if(argc != 5) {
			std::cerr << INVALID_ARGS_STR << "zerotier cli-add-zt <shortname> <url> <authToken>" << std::endl;
			return 1;
		}
		std::string thing_name = argv[2], url = argv[3], auth = argv[4]; 
		if(!checkURL(url)) {
			std::cout << FAIL_STR << "Malformed URL" << std::endl;
			return 1;
		}
		if(state.settings.find(thing_name) != state.settings.end()) {
			std::cout << "WARNING: A @thing with the shortname " << thing_name.c_str() 
				<< " already exists. Choose another name or rename the old @thing" << std::endl;
			std::cout << "      | Usage: To rename a @thing: zerotier cli-rename @old_thing_name @new_thing_name" << std::endl;
		}
		else {
			result = json::parse("{ \"auth\": \"" + auth + "\", \"type\": \"" + "one" + "\", \"url\": \"" + url + "\" }");
			saveSettingsBackup(state);
			// TODO: Handle cases where user may or may not prepend an @
			state.settings["things"][thing_name] = result; // changes
			saveSettings(state);
		}
	} 

	// zerotier cli-add-central <shortname> <url> <auth>
	// TODO: Check for malformed urls/auth
	else if (state.command == "cli-add-central") {
		if(argc != 5) {
			std::cerr << INVALID_ARGS_STR << "zerotier cli-add-central <shortname> <url> <authToken>" << std::endl;
			return 1;
		}
		std::string thing_name = argv[2], url = argv[3], auth = argv[4]; 
		if(!checkURL(url)) {
			std::cout << FAIL_STR << "Malformed URL" << std::endl;
			return 1;
		}
		if(state.settings.find(thing_name) != state.settings.end()) {
			std::cout << "WARNING: A @thing with the shortname " << thing_name.c_str() 
				<< " already exists. Choose another name or rename the old @thing" << std::endl;
			std::cout << "      | Usage: To rename a @thing: zerotier cli-rename @old_thing_name @new_thing_name" << std::endl;
		}
		else {
			result = json::parse("{ \"auth\": \"" + auth + "\", \"type\": \"" + "central" + "\", \"url\": \"" + url + "\" }");
			saveSettingsBackup(state);
			// TODO: Handle cases where user may or may not prepend an @
			state.settings["things"]["@" + thing_name] = result; // changes
			saveSettings(state);
		}
	} 

	// ONE SERVICE

	// zerotier ls --- display all networks currently joined
	else if (state.command == "ls" || state.command == "listnetworks") {
		if(argc != 2) {
			std::cerr << INVALID_ARGS_STR << "zerotier ls" << std::endl;
			return 1;
		}
		checkForThing(state,"one",true);
		url = state.url + "network";
		res = REQUEST(REQ_GET,state,state.reqHeaders,"",(const std::string)url);
		if(std::get<0>(res) == 200) {
			std::cout << "listnetworks <nwid> <name> <mac> <status> <type> <dev> <ZT assigned ips>" << std::endl;
			auto j = json::parse(std::get<1>(res).c_str());
			if (j.type() == json::value_t::array) {
				for(int i=0;i<j.size();i++){
					std::string nwid = j[i]["nwid"].get<std::string>();
					std::string name = j[i]["name"].get<std::string>();
					std::string mac = j[i]["mac"].get<std::string>();
					std::string status = j[i]["status"].get<std::string>();
					std::string type = j[i]["type"].get<std::string>();
					std::string addrs;
					for(int m=0; m<j[i]["assignedAddresses"].size(); m++) {
						addrs += j[i]["assignedAddresses"][m].get<std::string>() + " ";
					}
					std::string dev = j[i]["portDeviceName"].get<std::string>();
					std::cout << "listnetworks " << nwid << " " << name << " " << mac << " " << status << " " << type << " " << dev << " " << addrs << std::endl;				
				}
			}
		}
	} 

	// zerotier join <nwid> --- joins a network
	else if (state.command == "join") {
		if(argc != 3) {
			std::cerr << INVALID_ARGS_STR << "zerotier join <nwid>" << std::endl;
			return 1;
		}
		checkForThing(state,"one",true);
		res = REQUEST(REQ_POST,state,state.reqHeaders,"{}",state.url + "/network/" + state.args[0]);
		if(std::get<0>(res) == 200) {
			std::cout << OK_STR << "connected to " << state.args[0] << std::endl;
		}
	} 

	// zerotier leave <nwid> --- leaves a network
	else if (state.command == "leave") {
		if(argc != 3) {
			std::cerr << INVALID_ARGS_STR << "zerotier leave <nwid>" << std::endl;
			return 1;
		}
		checkForThing(state,"one",true);
		res = REQUEST(REQ_DEL,state,state.reqHeaders,"{}",state.url + "/network/" + state.args[0]);
		if(std::get<0>(res) == 200) {
			std::cout << OK_STR << "disconnected from " << state.args[0] << std::endl;
		}
	} 

	// zerotier peers --- display address and role of all peers
	else if (state.command == "peers") {
		if(argc != 2) {
			std::cerr << INVALID_ARGS_STR << "zerotier peers" << std::endl;
			return 1;
		}
		checkForThing(state,"one",true);
		res = REQUEST(REQ_GET,state,state.reqHeaders,"",state.url + "/peer");
		if(std::get<0>(res) == 200) {		
			json result = json::parse(std::get<1>(res));
			for(int i=0; i<result.size(); i++) {
				std::cout << result[i]["address"] << " " << result[i]["role"] << std::endl;
			}
		}
	} 

	// zerotier show --- display status of local instance
	else if (state.command == "show" || state.command == "status") {
		if(argc != 2) {
			std::cerr << INVALID_ARGS_STR << "zerotier show" << std::endl;
			return 1;
		}
		checkForThing(state,"one",true);
		res = REQUEST(REQ_GET,state,state.reqHeaders,"",state.url + "/status");
		if(std::get<0>(res) == 200) {		
			result = json::parse(std::get<1>(res));
			std::string status_str = result["online"].get<bool>() ? "ONLINE" : "OFFLINE";
			std::cout << "info " << result["address"].get<std::string>() 
				<< " " << status_str << " " << result["version"].get<std::string>() << std::endl;
		}
	} 
	
	// REMOTE
	
	// zerotier @thing net-create --- creates a new network
	else if (state.command == "net-create") {
		if(argc > 3 || (argc == 3 && !state.atname.length())) {
			std::cerr << INVALID_ARGS_STR << "zerotier <@thing> net-create" << std::endl;
			return 1;
		}
		checkForThing(state,"central",true);
		res = REQUEST(REQ_POST,state,state.reqHeaders,"",state.url + "api/network");
		if(std::get<0>(res) == 200) {
			json result = json::parse(std::get<1>(res));
			std::cout << OK_STR << "created network " << result["config"]["nwid"].get<std::string>() << std::endl;
		}
	} 

	// zerotier @thing net-rm <nwid> --- deletes a network
	else if (state.command == "net-rm") {
		if(argc > 4 || (argc == 4 && !state.atname.length())) {
			std::cerr << INVALID_ARGS_STR << "zerotier <@thing> net-rm <nwid>" << std::endl;
			return 1;
		}
		checkForThing(state,"central",true);
		if(!state.args.size()) {
			std::cout << "Argument error: No network specified." << std::endl;
			std::cout << "      | Usage: zerotier net-rm <nwid>" << std::endl;
		}
		else {
			std::string nwid = state.args[0];
			res = REQUEST(REQ_DEL,state,state.reqHeaders,"",state.url + "api/network/" + nwid);
			if(std::get<0>(res) == 200) {
				std::cout << "deleted network " << nwid << std::endl;
			}
		}
	} 

	// zerotier @thing net-ls --- lists all networks
	else if (state.command == "net-ls") {
		if(argc > 3 || (argc == 3 && !state.atname.length())) {
			std::cerr << INVALID_ARGS_STR << "zerotier <@thing> net-ls" << std::endl;
			return 1;
		}
		checkForThing(state,"central",true);
		res = REQUEST(REQ_GET,state,state.reqHeaders,"",state.url + "api/network");
		if(std::get<0>(res) == 200) {
			json result = json::parse(std::get<1>(res));
			for(int m=0;m<result.size(); m++) {
				std::cout << "network " << result[m]["id"].get<std::string>() << std::endl;
			}
		}
	} 

	// zerotier @thing net-members <nwid> --- show all members of a network
	else if (state.command == "net-members") {
		if(argc > 4 || (argc == 4 && !state.atname.length())) {
			std::cerr << INVALID_ARGS_STR << "zerotier <@thing> net-members <nwid>" << std::endl;
			return 1;
		}
		checkForThing(state,"central",true);
		if(!state.args.size()) {
			std::cout << FAIL_STR << "Argument error: No network specified." << std::endl;
			std::cout << "      | Usage: zerotier net-members <nwid>" << std::endl;
		}
		else {
			std::string nwid = state.args[0];
			res = REQUEST(REQ_GET,state,state.reqHeaders,"",state.url + "api/network/" + nwid + "/member");
			json result = json::parse(std::get<1>(res));
			std::cout << "Members of " << nwid << ":" << std::endl;
			for (json::iterator it = result.begin(); it != result.end(); ++it) {
				std::cout << it.key() << std::endl;
			}
		}
	} 

	// zerotier @thing net-show <nwid> <devID> --- show info about a device on a specific network
	else if (state.command == "net-show") {
		if(argc > 5 || (argc == 5 && !state.atname.length())) {
			std::cerr << INVALID_ARGS_STR << "zerotier <@thing> net-show <nwid> <devID>" << std::endl;
			return 1;
		}
		checkForThing(state,"central",true);
		if(state.args.size() < 2) {
			std::cout << FAIL_STR << "Argument error: Too few arguments." << std::endl;
			std::cout << "      | Usage: zerotier net-show <nwid> <devID>" << std::endl;
		}
		else {
			std::string nwid = state.args[0];
			std::string devid = state.args[1];
			res = REQUEST(REQ_GET,state,state.reqHeaders,"",state.url + "api/network/" + nwid + "/member/" + devid);
			// TODO: More info, what would we like to show exactly?
			if(std::get<0>(res) == 200) {
				json result = json::parse(std::get<1>(res));
				std::cout << "Assigned IP: " << std::endl;
				for(int m=0; m<result["config"]["ipAssignments"].size();m++) {
					std::cout << "\t" << result["config"]["ipAssignments"][m].get<std::string>() << std::endl;
				}
			}
		}
	} 

	// zerotier @thing net-auth <nwid> <devID> --- authorize a device on a network
	else if (state.command == "net-auth") {
		if(argc > 5 || (argc == 5 && !state.atname.length())) {
			std::cerr << INVALID_ARGS_STR << "zerotier <@thing> net-auth <nwid> <devID>" << std::endl;
			return 1;
		}
		checkForThing(state,"central",true);
		if(state.args.size() != 2) {
			std::cout << FAIL_STR << "Argument error: Network and/or device ID not specified." << std::endl;
			std::cout << "      | Usage: zerotier net-auth <nwid> <devID>" << std::endl;
		}
		std::string nwid = state.args[0];
		std::string devid = state.args[1];
		url = state.url + "api/network/" + nwid + "/member/" + devid;
		// Add device to network
		res = REQUEST(REQ_POST,state,state.reqHeaders,"",(const std::string)url);
		if(std::get<0>(res) == 200) {
			result = json::parse(std::get<1>(res));
			res = REQUEST(REQ_GET,state,state.reqHeaders,"",(const std::string)url);
			result = json::parse(std::get<1>(res));
			result["config"]["authorized"] = "true";
			std::string newconfig = result.dump();
			res = REQUEST(REQ_POST,state,state.reqHeaders,newconfig,(const std::string)url);
			if(std::get<0>(res) == 200)
				std::cout << OK_STR << devid << " authorized on " << nwid << std::endl;
			else
				std::cout << FAIL_STR << "There was a problem authorizing that device." << std::endl;
		}
	} 

	// zerotier @thing net-unauth <nwid> <devID>
	else if (state.command == "net-unauth") {
		if(argc > 5 || (argc == 5 && !state.atname.length())) {
			std::cerr << INVALID_ARGS_STR << "zerotier <@thing> net-unauth <nwid> <devID>" << std::endl;
			return 1;
		}
		checkForThing(state,"central",true);
		if(state.args.size() != 2) {
			std::cout << FAIL_STR << "Bad argument. No network and/or device ID specified." << std::endl;
			std::cout << "      | Usage: zerotier net-unauth <nwid> <devID>" << std::endl;
		}
		std::string nwid = state.args[0];
		std::string devid = state.args[1];
		// If successful, get member config
		res = REQUEST(REQ_GET,state,state.reqHeaders,"",state.url + "api/network/" + nwid + "/member/" + devid);
		result = json::parse(std::get<1>(res));
		// modify auth field and re-POST
		result["config"]["authorized"] = "false";
		std::string newconfig = result.dump();
		res = REQUEST(REQ_POST,state,state.reqHeaders,newconfig,state.url + "api/network/" + nwid + "/member/" + devid);
		if(std::get<0>(res) == 200)
			std::cout << OK_STR << devid << " de-authorized from " << nwid << std::endl;
		else
			std::cout << FAIL_STR << "There was a problem de-authorizing that device." << std::endl;
	} 

	// zerotier @thing net-set
	else if (state.command == "net-set") {
	} 
	
	// ID

	// zerotier id-generate [<vanity prefix>]
	else if (state.command == "id-generate") {
		if(argc != 3) {
			std::cerr << INVALID_ARGS_STR << "zerotier id-generate [<vanity prefix>]" << std::endl;
			return 1;
		}
		uint64_t vanity = 0;
		int vanityBits = 0;
		if (argc >= 5) {
			vanity = Utils::hexStrToU64(argv[4]) & 0xffffffffffULL;
			vanityBits = 4 * strlen(argv[4]);
			if (vanityBits > 40)
				vanityBits = 40;
		}

		ZeroTier::Identity id;
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

		std::string idser = id.toString(true);
		if (argc >= 3) {
			if (!OSUtils::writeFile(argv[2],idser)) {
				std::cerr << "Error writing to " << argv[2] << std::endl;
				return 1;
			} else std::cout << argv[2] << " written" << std::endl;
			if (argc >= 4) {
				idser = id.toString(false);
				if (!OSUtils::writeFile(argv[3],idser)) {
					std::cerr << "Error writing to " << argv[3] << std::endl;
					return 1;
				} else std::cout << argv[3] << " written" << std::endl;
			}
		} else std::cout << idser << std::endl;
	} 

	// zerotier id-validate <identity>
	else if (state.command == "id-validate") {
		if(argc != 3) {
			std::cerr << INVALID_ARGS_STR << "zerotier id-validate <identity>" << std::endl;
			return 1;
		}
		Identity id = getIdFromArg(argv[2]);
		if (!id) {
			std::cerr << "Identity argument invalid or file unreadable: " << argv[2] << std::endl;
			return 1;
		}
		if (!id.locallyValidate()) {
			std::cerr << argv[2] << " FAILED validation." << std::endl;
			return 1;
		} else std::cout << argv[2] << "is a valid identity" << std::endl;
	} 

	// zerotier id-sign <identity> <file>
	else if (state.command == "id-sign") {
		if(argc != 4) {
			std::cerr << INVALID_ARGS_STR << "zerotier id-sign <identity> <file>" << std::endl;
			return 1;
		}
		Identity id = getIdFromArg(argv[2]);
		if (!id) {
			std::cerr << "Identity argument invalid or file unreadable: " << argv[2] << std::endl;
			return 1;
		}
		if (!id.hasPrivate()) {
			std::cerr << argv[2] << " does not contain a private key (must use private to sign)" << std::endl;
			return 1;
		}
		std::string inf;
		if (!OSUtils::readFile(argv[3],inf)) {
			std::cerr << argv[3] << " is not readable" << std::endl;
			return 1;
		}
		C25519::Signature signature = id.sign(inf.data(),(unsigned int)inf.length());
		std::cout << Utils::hex(signature.data,(unsigned int)signature.size()) << std::endl;
	} 

	// zerotier id-verify <secret> <file> <sig>
	else if (state.command == "id-verify") {
		if(argc != 4) {
			std::cerr << INVALID_ARGS_STR << "zerotier id-verify <secret> <file> <sig>" << std::endl;
			return 1;
		}
		Identity id = getIdFromArg(argv[2]);
		if (!id) {
			std::cerr << "Identity argument invalid or file unreadable: " << argv[2] << std::endl;
			return 1;
		}
		std::string inf;
		if (!OSUtils::readFile(argv[3],inf)) {
			std::cerr << argv[3] << " is not readable" << std::endl;
			return 1;
		}
		std::string signature(Utils::unhex(argv[4]));
		if ((signature.length() > ZT_ADDRESS_LENGTH)&&(id.verify(inf.data(),(unsigned int)inf.length(),signature.data(),(unsigned int)signature.length()))) {
			std::cout << argv[3] << " signature valid" << std::endl;
		} else {
			std::cerr << argv[3] << " signature check FAILED" << std::endl;
			return 1;
		}
	} 

	// zerotier id-getpublic <secret>
	else if (state.command == "id-getpublic") {
		if(argc != 3) {
			std::cerr << INVALID_ARGS_STR << "zerotier id-getpublic <secret>" << std::endl;
			return 1;
		}
		Identity id = getIdFromArg(argv[2]);
		if (!id) {
			std::cerr << "Identity argument invalid or file unreadable: " << argv[2] << std::endl;
			return 1;
		}
		std::cerr << id.toString(false) << std::endl;
	} 
	//
	else {
		dumpHelp();
		return -1;
	}
	if(std::find(state.args.begin(), state.args.end(), "-verbose") != state.args.end())
		std::cout << "\n\nAPI response = " << std::get<1>(res) << std::endl;
	curl_global_cleanup();
	return 0;
}
