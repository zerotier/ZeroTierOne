/* Makes topology dictionary out of source dictionary and signs with
 * 'topology.secret', which must be present (or symlinked) from where
 * this is run. */

/* Just type 'make' and then run (Only tested on Linux) */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <string>
#include <iostream>
#include <map>

#include "../node/Utils.hpp"
#include "../node/Identity.hpp"
#include "../node/Dictionary.hpp"

using namespace ZeroTier;

int main(int argc,char **argv)
{
	std::string buf;

	if (!Utils::readFile("topology.secret",buf)) {
		std::cerr << "Cannot read topology.secret" << std::endl;
		return 1;
	}
	Identity topologyAuthority(buf);

	Dictionary topology;

	Dictionary supernodes;
	std::map<std::string,bool> supernodeDictionaries(Utils::listDirectory("supernodes"));
	for(std::map<std::string,bool>::iterator sn(supernodeDictionaries.begin());sn!=supernodeDictionaries.end();++sn) {
		if ((sn->first.length() == 10)&&(!sn->second)) {
			buf.clear();
			if (!Utils::readFile((std::string("supernodes/")+sn->first).c_str(),buf)) {
				std::cerr << "Cannot read supernodes/" << sn->first << std::endl;
				return 1;
			}
			supernodes[sn->first] = buf;
		}
	}
	topology["supernodes"] = supernodes.toString();

	if (!topology.sign(topologyAuthority)) {
		std::cerr << "Unable to sign!" << std::endl;
		return 1;
	}

	std::cout << topology.toString();
	return 0;
}
