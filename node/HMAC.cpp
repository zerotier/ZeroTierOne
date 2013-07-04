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

#include "HMAC.hpp"

#include <openssl/sha.h>

namespace ZeroTier {

void HMAC::sha256(const void *key,unsigned int klen,const void *message,unsigned int len,void *mac)
	throw()
{
	union {
		uint64_t q[12];
		uint8_t b[96];
	} key2,opad,ipad;
	SHA256_CTX sha;

	if (klen == 32) { // this is what we use, so handle this quickly
		key2.q[0] = ((const uint64_t *)key)[0];
		key2.q[1] = ((const uint64_t *)key)[1];
		key2.q[2] = ((const uint64_t *)key)[2];
		key2.q[3] = ((const uint64_t *)key)[3];
		key2.q[4] = 0ULL;
		key2.q[5] = 0ULL;
		key2.q[6] = 0ULL;
		key2.q[7] = 0ULL;
	} else { // for correctness and testing against test vectors
		if (klen > 64) {
			SHA256_Init(&sha);
			SHA256_Update(&sha,key,klen);
			SHA256_Final(key2.b,&sha);
			klen = 32;
		} else {
			for(unsigned int i=0;i<klen;++i)
				key2.b[i] = ((const uint8_t *)key)[i];
		}
		while (klen < 64)
			key2.b[klen++] = (uint8_t)0;
	}

	for(unsigned int i=0;i<8;++i)
		opad.q[i] = 0x5c5c5c5c5c5c5c5cULL ^ key2.q[i];
	for(unsigned int i=0;i<8;++i)
		ipad.q[i] = 0x3636363636363636ULL ^ key2.q[i];

	SHA256_Init(&sha);
	SHA256_Update(&sha,(const unsigned char *)ipad.b,64);
	SHA256_Update(&sha,(const unsigned char *)message,len);
	SHA256_Final((unsigned char *)(opad.b + 64),&sha);

	SHA256_Init(&sha);
	SHA256_Update(&sha,opad.b,96);
	SHA256_Final((unsigned char *)mac,&sha);
}

} // namespace ZeroTier
