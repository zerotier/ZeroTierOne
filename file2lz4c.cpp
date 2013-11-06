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

/* Converts files to LZ4-compressed C arrays, used in building installers. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <iostream>

#include "node/Utils.hpp"

#include "ext/lz4/lz4.h"
#include "ext/lz4/lz4hc.h"

using namespace ZeroTier;

int main(int argc,char **argv)
{
	char tmp[16];

	if (argc != 3) {
		std::cerr << "Usage: " << argv[0] << " <file> <name of C array>" << std::endl;
		return -1;
	}

	std::string buf;
	if (!Utils::readFile(argv[1],buf)) {
		std::cerr << "Could not read " << argv[1] << std::endl;
		return -1;
	}

	unsigned char *compbuf = new unsigned char[LZ4_compressBound((int)buf.length())];
	int complen = LZ4_compressHC(buf.data(),(char *)compbuf,(int)buf.length());

	if (complen <= 0) {
		std::cerr << "Error compressing data." << std::endl;
		return -1;
	}

	std::cout << "#define " << argv[2] << "_UNCOMPRESSED_LEN " << buf.length() << std::endl;
	std::cout << "#define " << argv[2] << "_LZ4_LEN " << complen << std::endl;
	std::cout << "static const unsigned char " << argv[2] << '[' << argv[2] << "_LZ4_LEN] = {";
	for(int i=0;i<complen;++i) {
		if (!(i % 15))
			std::cout << std::endl << '\t';
		Utils::snprintf(tmp,sizeof(tmp),"%.2x",(unsigned int)compbuf[i]);
		std::cout << "0x" << tmp;
		if (i != (complen - 1))
			std::cout << ',';
	}
	std::cout << std::endl << "};" << std::endl;

	delete [] compbuf;
	return 0;
}
