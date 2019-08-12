/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier,Inc.  https://www.zerotier.com/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation,either version 3 of the License,or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not,see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
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

static unsigned long _Utils_itoa(unsigned long n,char *s)
{
	if (n == 0)
		return 0;
	unsigned long pos = _Utils_itoa(n / 10,s);
	if (pos >= 22) // sanity check,should be impossible
		pos = 22;
	s[pos] = '0' + (char)(n % 10);
	return pos + 1;
}
char *Utils::decimal(unsigned long n,char s[24])
{
	if (n == 0) {
		s[0] = '0';
		s[1] = (char)0;
		return s;
	}
	s[_Utils_itoa(n,s)] = (char)0;
	return s;
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
	 * a bit of extra entropy and further randomizing the result,and comes
	 * at almost no cost and with no real downside if the random source is
	 * good. */
	if (!s20Initialized) {
		s20Initialized = true;
		uint64_t s20Key[4];
		s20Key[0] = (uint64_t)time(0); // system clock
		s20Key[1] = (uint64_t)buf; // address of buf
		s20Key[2] = (uint64_t)s20Key; // address of s20Key[]
		s20Key[3] = (uint64_t)&s20; // address of s20
		s20.init(s20Key,s20Key);
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
			s20.init(randomBuf,randomBuf);
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
			s20.init(randomBuf,randomBuf);
		}
		((uint8_t *)buf)[i] = randomBuf[randomPtr++];
	}

#endif // __WINDOWS__ or not
}

int Utils::b32d(const char *encoded,uint8_t *result,int bufSize)
{
  int buffer = 0;
  int bitsLeft = 0;
  int count = 0;
  for (const uint8_t *ptr = (const uint8_t *)encoded;count<bufSize && *ptr; ++ptr) {
    uint8_t ch = *ptr;
    if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '-' || ch == '.') {
      continue;
    }
    buffer <<= 5;

    if (ch == '0') {
      ch = 'O';
    } else if (ch == '1') {
      ch = 'L';
    } else if (ch == '8') {
      ch = 'B';
    }

    if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) {
      ch = (ch & 0x1F) - 1;
    } else if (ch >= '2' && ch <= '7') {
      ch -= '2' - 26;
    } else {
      return -1;
    }

    buffer |= ch;
    bitsLeft += 5;
    if (bitsLeft >= 8) {
      result[count++] = buffer >> (bitsLeft - 8);
      bitsLeft -= 8;
    }
  }
  if (count < bufSize)
    result[count] = (uint8_t)0;
  return count;
}

int Utils::b32e(const uint8_t *data,int length,char *result,int bufSize)
{
  if (length < 0 || length > (1 << 28)) {
		result[0] = (char)0;
    return -1;
	}
	int count = 0;
  if (length > 0) {
    int buffer = data[0];
    int next = 1;
    int bitsLeft = 8;
    while (count < bufSize && (bitsLeft > 0 || next < length)) {
      if (bitsLeft < 5) {
        if (next < length) {
          buffer <<= 8;
          buffer |= data[next++] & 0xFF;
          bitsLeft += 8;
        } else {
          int pad = 5 - bitsLeft;
          buffer <<= pad;
          bitsLeft += pad;
        }
      }
      int index = 0x1F & (buffer >> (bitsLeft - 5));
      bitsLeft -= 5;
      result[count++] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567"[index];
    }
  }
  if (count < bufSize) {
		result[count] = (char)0;
		return count;
	}
	result[0] = (char)0;
	return -1;
}

unsigned int Utils::b64e(const uint8_t *in,unsigned int inlen,char *out,unsigned int outlen)
{
	static const char base64en[64] = { 'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/' };
	unsigned int i = 0,j = 0;
	uint8_t l = 0;
	int s = 0;
	for (;i<inlen;++i) {
		uint8_t c = in[i];
		switch (s) {
		case 0:
			s = 1;
			if (j >= outlen) return 0;
			out[j++] = base64en[(c >> 2) & 0x3f];
			break;
		case 1:
			s = 2;
			if (j >= outlen) return 0;
			out[j++] = base64en[((l & 0x3) << 4) | ((c >> 4) & 0xf)];
			break;
		case 2:
			s = 0;
			if (j >= outlen) return 0;
			out[j++] = base64en[((l & 0xf) << 2) | ((c >> 6) & 0x3)];
			if (j >= outlen) return 0;
			out[j++] = base64en[c & 0x3f];
			break;
		}
		l = c;
	}
	switch (s) {
	case 1:
		if (j >= outlen) return 0;
		out[j++] = base64en[(l & 0x3) << 4];
		//out[j++] = '=';
		//out[j++] = '=';
		break;
	case 2:
		if (j >= outlen) return 0;
		out[j++] = base64en[(l & 0xf) << 2];
		//out[j++] = '=';
		break;
	}
	if (j >= outlen) return 0;
	out[j] = 0;
	return j;
}

unsigned int Utils::b64d(const char *in,unsigned char *out,unsigned int outlen)
{
	static const uint8_t base64de[256] = { 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,62,255,255,255,63,52,53,54,55,56,57,58,59,60,61,255,255,255,255,255,255,255,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,255,255,255,255,255,255,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,255,255,255,255,255 };
	unsigned int i = 0;
	unsigned int j = 0;
	while ((in[i] != '=')&&(in[i] != 0)) {
		if (j >= outlen)
			break;
		uint8_t c = base64de[(unsigned char)in[i]];
		if (c != 255) {
			switch (i & 0x3) {
				case 0:
					out[j] = (c << 2) & 0xff;
					break;
				case 1:
					out[j++] |= (c >> 4) & 0x3;
					out[j] = (c & 0xf) << 4; 
					break;
				case 2:
					out[j++] |= (c >> 2) & 0xf;
					out[j] = (c & 0x3) << 6;
					break;
				case 3:
					out[j++] |= c;
					break;
			}
		}
		++i;
	}
	return j;
}

} // namespace ZeroTier
