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

	if (!Utils::readFile("root-topology-authority.secret",buf)) {
		std::cerr << "Cannot read root-topology-authority.secret" << std::endl;
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

	Dictionary test(topology.toString());
	if (!test.verify(topologyAuthority)) {
		std::cerr << "Test verification of signed dictionary failed!" << std::endl;
		return 1;
	}

	std::cout << topology.toString();
	return 0;
}
