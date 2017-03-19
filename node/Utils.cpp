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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>

#include "Constants.hpp"

#ifdef __UNIX_LIKE__
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <dirent.h>
#endif

#ifdef __WINDOWS__
#include <wincrypt.h>
#endif

#include "Utils.hpp"
#include "Mutex.hpp"
#include "Salsa20.hpp"

namespace ZeroTier {

const char Utils::HEXCHARS[16] = { '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f' };

// Crazy hack to force memory to be securely zeroed in spite of the best efforts of optimizing compilers.
static void _Utils_doBurn(volatile uint8_t *ptr,unsigned int len)
{
	volatile uint8_t *const end = ptr + len;
	while (ptr != end) *(ptr++) = (uint8_t)0;
}
static void (*volatile _Utils_doBurn_ptr)(volatile uint8_t *,unsigned int) = _Utils_doBurn;
void Utils::burn(void *ptr,unsigned int len) { (_Utils_doBurn_ptr)((volatile uint8_t *)ptr,len); }

std::string Utils::hex(const void *data,unsigned int len)
{
	std::string r;
	r.reserve(len * 2);
	for(unsigned int i=0;i<len;++i) {
		r.push_back(HEXCHARS[(((const unsigned char *)data)[i] & 0xf0) >> 4]);
		r.push_back(HEXCHARS[((const unsigned char *)data)[i] & 0x0f]);
	}
	return r;
}

std::string Utils::unhex(const char *hex,unsigned int maxlen)
{
	int n = 1;
	unsigned char c,b = 0;
	const char *eof = hex + maxlen;
	std::string r;

	if (!maxlen)
		return r;

	while ((c = (unsigned char)*(hex++))) {
		if ((c >= 48)&&(c <= 57)) { // 0..9
			if ((n ^= 1))
				r.push_back((char)(b | (c - 48)));
			else b = (c - 48) << 4;
		} else if ((c >= 65)&&(c <= 70)) { // A..F
			if ((n ^= 1))
				r.push_back((char)(b | (c - (65 - 10))));
			else b = (c - (65 - 10)) << 4;
		} else if ((c >= 97)&&(c <= 102)) { // a..f
			if ((n ^= 1))
				r.push_back((char)(b | (c - (97 - 10))));
			else b = (c - (97 - 10)) << 4;
		}
		if (hex == eof)
			break;
	}

	return r;
}

unsigned int Utils::unhex(const char *hex,unsigned int maxlen,void *buf,unsigned int len)
{
	int n = 1;
	unsigned char c,b = 0;
	unsigned int l = 0;
	const char *eof = hex + maxlen;

	if (!maxlen)
		return 0;

	while ((c = (unsigned char)*(hex++))) {
		if ((c >= 48)&&(c <= 57)) { // 0..9
			if ((n ^= 1)) {
				if (l >= len) break;
				((unsigned char *)buf)[l++] = (b | (c - 48));
			} else b = (c - 48) << 4;
		} else if ((c >= 65)&&(c <= 70)) { // A..F
			if ((n ^= 1)) {
				if (l >= len) break;
				((unsigned char *)buf)[l++] = (b | (c - (65 - 10)));
			} else b = (c - (65 - 10)) << 4;
		} else if ((c >= 97)&&(c <= 102)) { // a..f
			if ((n ^= 1)) {
				if (l >= len) break;
				((unsigned char *)buf)[l++] = (b | (c - (97 - 10)));
			} else b = (c - (97 - 10)) << 4;
		}
		if (hex == eof)
			break;
	}

	return l;
}

void Utils::getSecureRandom(void *buf,unsigned int bytes)
{
	static Mutex globalLock;
	static Salsa20 s20;
	static bool s20Initialized = false;
	static uint8_t randomBuf[65536];
	static unsigned int randomPtr = sizeof(randomBuf);

	Mutex::Lock _l(globalLock);

	/* Just for posterity we Salsa20 encrypt the result of whatever system
	 * CSPRNG we use. There have been several bugs at the OS or OS distribution
	 * level in the past that resulted in systematically weak or predictable
	 * keys due to random seeding problems. This mitigates that by grabbing
	 * a bit of extra entropy and further randomizing the result, and comes
	 * at almost no cost and with no real downside if the random source is
	 * good. */
	if (!s20Initialized) {
		s20Initialized = true;
		uint64_t s20Key[4];
		s20Key[0] = (uint64_t)time(0); // system clock
		s20Key[1] = (uint64_t)buf; // address of buf
		s20Key[2] = (uint64_t)s20Key; // address of s20Key[]
		s20Key[3] = (uint64_t)&s20; // address of s20
		s20.init(s20Key,256,s20Key);
	}

#ifdef __WINDOWS__

	static HCRYPTPROV cryptProvider = NULL;

	for(unsigned int i=0;i<bytes;++i) {
		if (randomPtr >= sizeof(randomBuf)) {
			if (cryptProvider == NULL) {
				if (!CryptAcquireContextA(&cryptProvider,NULL,NULL,PROV_RSA_FULL,CRYPT_VERIFYCONTEXT|CRYPT_SILENT)) {
					fprintf(stderr,"FATAL ERROR: Utils::getSecureRandom() unable to obtain WinCrypt context!\r\n");
					exit(1);
				}
			}
			if (!CryptGenRandom(cryptProvider,(DWORD)sizeof(randomBuf),(BYTE *)randomBuf)) {
				fprintf(stderr,"FATAL ERROR: Utils::getSecureRandom() CryptGenRandom failed!\r\n");
				exit(1);
			}
			randomPtr = 0;
			s20.crypt12(randomBuf,randomBuf,sizeof(randomBuf));
		}
		((uint8_t *)buf)[i] = randomBuf[randomPtr++];
	}

#else // not __WINDOWS__

	static int devURandomFd = -1;

	if (devURandomFd < 0) {
		devURandomFd = ::open("/dev/urandom",O_RDONLY);
		if (devURandomFd < 0) {
			fprintf(stderr,"FATAL ERROR: Utils::getSecureRandom() unable to open /dev/urandom\n");
			exit(1);
			return;
		}
	}

	for(unsigned int i=0;i<bytes;++i) {
		if (randomPtr >= sizeof(randomBuf)) {
			for(;;) {
				if ((int)::read(devURandomFd,randomBuf,sizeof(randomBuf)) != (int)sizeof(randomBuf)) {
					::close(devURandomFd);
					devURandomFd = ::open("/dev/urandom",O_RDONLY);
					if (devURandomFd < 0) {
						fprintf(stderr,"FATAL ERROR: Utils::getSecureRandom() unable to open /dev/urandom\n");
						exit(1);
						return;
					}
				} else break;
			}
			randomPtr = 0;
			s20.crypt12(randomBuf,randomBuf,sizeof(randomBuf));
		}
		((uint8_t *)buf)[i] = randomBuf[randomPtr++];
	}

#endif // __WINDOWS__ or not
}

bool Utils::scopy(char *dest,unsigned int len,const char *src)
{
	if (!len)
		return false; // sanity check
	if (!src) {
		*dest = (char)0;
		return true;
	}
	char *end = dest + len;
	while ((*dest++ = *src++)) {
		if (dest == end) {
			*(--dest) = (char)0;
			return false;
		}
	}
	return true;
}

unsigned int Utils::snprintf(char *buf,unsigned int len,const char *fmt,...)
	throw(std::length_error)
{
	va_list ap;

	va_start(ap,fmt);
	int n = (int)vsnprintf(buf,len,fmt,ap);
	va_end(ap);

	if ((n >= (int)len)||(n < 0)) {
		if (len)
			buf[len - 1] = (char)0;
		throw std::length_error("buf[] overflow in Utils::snprintf");
	}

	return (unsigned int)n;
}

} // namespace ZeroTier
