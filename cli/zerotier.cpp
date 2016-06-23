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

#include <curl/curl.h>

using json = nlohmann::json;
using OSUtils = ZeroTier::OSUtils;

namespace {

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

static json loadSettings()
{
	json settings;
	std::string buf;
	if (OSUtils::readFile(getSettingsFilePath().c_str(),buf))
		settings = json::parse(buf);
	return settings;
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
	std::cout << "  "
	std::cout << std::endl;
	std::cout << "CLI Configuration Commands:" << std::endl;
	std::cout << "  cli-set <setting> <value>           - Set a CLI config option" << std::endl;
	std::cout << "  cli-ls                              - List configured @things" << std::endl;
	std::cout << "  cli-rm @name                        - Remove a configured @thing" << std::endl;
	std::cout << "  cli-add-zt @name <url> <auth>       - Add a ZeroTier service" << std::endl;
	std::cout << "  cli-add-central @name <url> <auth>  - Add ZeroTier Central instance" << std::endl;
	std::cout << std::endl;
	std::cout << "ZeroTier Service Commands:" << std::endl;
	std::cout << "  ls                                  - List currently joined networks" << std::endl;
	std::cout << "  join <network> [opt=value ...]      - Join a network" << std::endl;
	std::cout << "  leave <network>                     - Leave a network" << std::endl;
	std::cout << "  peers                               - List ZeroTier VL1 peers" << std::endl;
	std::cout << "  show [<network/peer address>]       - Get info about self or object" << std::endl;
	std::cout << std::endl;
	std::cout << "Network Controller Commands:" << std::endl;
	std::cout << "  net-create                          - Create a new network" << std::endl;
	std::cout << "  net-rm <network>                    - Delete a network (BE CAREFUL!)" << std::endl;
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

	CURL *const curl = curl_easy_init();

	std::string atname;
	std::string command;
	std::vector<std::string> args;
	std::map<char,std::string> opts;
	char nextIsOptValue = 0;
	for(int i=1;i<argc;++i) {
		if ((i == 1)&&(argv[i][0] == '@')) {
			atname = argv[i];
		} else if (nextIsOptValue) {
			opts[nextIsOptValue] = argv[i];
			nextIsOptValue = 0;
		} else if (command.length() == 0) {
			if (argv[i][0] == '-') {
				if (!argv[i][1]) {
					dumpHelp();
					return -1;
				} else if (argv[i][2]) {
					opts[argv[i][1]] = argv[i] + 2;
				} else {
					nextIsOptValue = argv[i][1];
				}
			} else {
				command = argv[i];
			}
		} else {
			args.push_back(std::string(argv[i]));
		}
	}

	if ((command.length() == 0)||(command == "help")) {
		dumpHelp();
		return -1;
	} else if (command == "cli-set") {
	} else if (command == "cli-ls") {
	} else if (command == "cli-rm") {
	} else if (command == "cli-add-zt") {
	} else if (command == "cli-add-central") {
	} else if (command == "ls") {
	} else if (command == "join") {
	} else if (command == "leave") {
	} else if (command == "peers") {
	} else if (command == "show") {
	} else if (command == "net-create") {
	} else if (command == "net-rm") {
	} else if (command == "net-ls") {
	} else if (command == "net-members") {
	} else if (command == "net-show") {
	} else if (command == "net-auth") {
	} else if (command == "net-set") {
	} else if (command == "id-generate") {
	} else if (command == "id-validate") {
	} else if (command == "id-sign") {
	} else if (command == "id-verify") {
	} else if (command == "id-getpublic") {
	} else {
		dumpHelp();
		return -1;
	}

	curl_easy_cleanup(curl);

	return 0;
}
