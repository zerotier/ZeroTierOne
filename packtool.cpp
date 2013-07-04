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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <string>

#include "node/Identity.hpp"
#include "node/Pack.hpp"
#include "node/Utils.hpp"

#include <unistd.h>

using namespace ZeroTier;

static void printHelp(const char *pn)
{
	std::cout << "Usage: " << pn << " <command> [<args>]" << std::endl << std::endl;
	std::cout << "Commands:" << std::endl;
	std::cout << "	list <packfile> [<identity.secret/public>]" << std::endl;
	std::cout << "	create <packfile> <identity.secret> <file> [<file> ...]" << std::endl;
	std::cout << "	extract <packfile> <destination directory>" << std::endl;
	std::cout << std::endl << "To check signatures, use 'list' with an identity argument." << std::endl;
}

static Pack *readPack(const char *path)
{
	std::string tmp;
	if (!Utils::readFile(path,tmp))
		return (Pack *)0;
	Pack *p = new Pack();
	if (!p->deserialize(tmp)) {
		delete p;
		return (Pack *)0;
	}
	return p;
}

int main(int argc,char **argv)
{
	if (argc < 2) {
		printHelp(argv[0]);
		return -1;
	}

	if (!strcmp(argv[1],"list")) {
		if (argc < 3) {
			printHelp(argv[0]);
			return -1;
		}

		Pack *pack = readPack(argv[2]);
		if (!pack) {
			std::cout << "Could not read " << argv[2] << std::endl;
			return -1;
		}

		std::vector<const Pack::Entry *> entries = pack->getAll();
		for(std::vector<const Pack::Entry *>::iterator e=entries.begin();e!=entries.end();++e) {
			std::cout << (*e)->name << '\t' << (*e)->content.length() << '\t' << Utils::hex((*e)->sha256,32) << "\tsigned by: " << (*e)->signedBy.toString() << std::endl;
		}

		if (argc >= 4) {
			std::string idser;
			if (!Utils::readFile(argv[3],idser)) {
				std::cout << "Unable to read identity from " << argv[3] << std::endl;
				return -1;
			}
			Identity id;
			if (!id.fromString(idser)) {
				std::cout << "Invalid identity" << std::endl;
				return -1;
			}

			entries = pack->verifyAll(id,true);
			for(std::vector<const Pack::Entry *>::iterator e=entries.begin();e!=entries.end();++e) {
				std::cout << "!!! Signature verification FAILED for: " << (*e)->name << std::endl;
			}
			if (!entries.size())
				std::cout << "Signature for all entries verified OK" << std::endl;
		}

		delete pack;
	} else if (!strcmp(argv[1],"create")) {
		if (argc < 5) {
			printHelp(argv[0]);
			return -1;
		}

		std::string idser;
		if (!Utils::readFile(argv[3],idser)) {
			std::cout << "Unable to read identity from " << argv[3] << std::endl;
			return -1;
		}
		Identity id;
		if (!id.fromString(idser)) {
			std::cout << "Invalid identity" << std::endl;
			return -1;
		}

		if (!id.hasPrivate()) {
			std::cout << "Identity must include private key to sign" << std::endl;
			return -1;
		}

		Pack pack;
		for(int i=4;i<argc;++i) {
			std::string fdata;
			if (!Utils::readFile(argv[i],fdata)) {
				std::cout << "Unable to read " << argv[i] << std::endl;
				return -1;
			}
			pack.put(std::string(argv[i]),fdata);
			std::cout << "Added " << argv[i] << std::endl;
		}
		if (!pack.signAll(id)) {
			std::cout << "Unable to sign with identity" << std::endl;
			return -1;
		} else std::cout << "Signed all entries with identity " << id.address().toString() << std::endl;
		std::string packser = pack.serialize();

		if (!Utils::writeFile(argv[2],packser)) {
			std::cout << "Unable to write " << argv[2] << std::endl;
			return -1;
		}
		std::cout << "Wrote " << packser.length() << " bytes to " << argv[2] << std::endl;
	} else if (!strcmp(argv[1],"extract")) {
		if (argc < 4) {
			printHelp(argv[0]);
			return -1;
		}

		Pack *pack = readPack(argv[2]);
		if (!pack) {
			std::cout << "Could not read " << argv[2] << std::endl;
			return -1;
		}

		if (chdir(argv[3])) {
			std::cout << "Unable to change to " << argv[3] << " for output." << std::endl;
			return -1;
		}

		std::vector<const Pack::Entry *> entries = pack->getAll();
		for(std::vector<const Pack::Entry *>::iterator e=entries.begin();e!=entries.end();++e) {
			if (!Utils::writeFile((*e)->name.c_str(),(*e)->content))
				std::cout << "Error writing " << (*e)->name << std::endl;
			else std::cout << "Wrote " << (*e)->name << " (" << (*e)->content.length() << ")" << std::endl;
		}
	} else {
		printHelp(argv[0]);
		return -1;
	}

	return 0;
}
