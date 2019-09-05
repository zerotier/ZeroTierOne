/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

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
#include "AES.hpp"
#include "SHA512.hpp"

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

unsigned int Utils::unhex(const char *h,void *buf,unsigned int buflen)
{
	unsigned int l = 0;
	while (l < buflen) {
		uint8_t hc = *(reinterpret_cast<const uint8_t *>(h++));
		if (!hc) break;

		uint8_t c = 0;
		if ((hc >= 48)&&(hc <= 57)) // 0..9
			c = hc - 48;
		else if ((hc >= 97)&&(hc <= 102)) // a..f
			c = hc - 87;
		else if ((hc >= 65)&&(hc <= 70)) // A..F
			c = hc - 55;

		hc = *(reinterpret_cast<const uint8_t *>(h++));
		if (!hc) break;

		c <<= 4;
		if ((hc >= 48)&&(hc <= 57))
			c |= hc - 48;
		else if ((hc >= 97)&&(hc <= 102))
			c |= hc - 87;
		else if ((hc >= 65)&&(hc <= 70))
			c |= hc - 55;

		reinterpret_cast<uint8_t *>(buf)[l++] = c;
	}
	return l;
}

unsigned int Utils::unhex(const char *h,unsigned int hlen,void *buf,unsigned int buflen)
{
	unsigned int l = 0;
	const char *hend = h + hlen;
	while (l < buflen) {
		if (h == hend) break;
		uint8_t hc = *(reinterpret_cast<const uint8_t *>(h++));
		if (!hc) break;

		uint8_t c = 0;
		if ((hc >= 48)&&(hc <= 57))
			c = hc - 48;
		else if ((hc >= 97)&&(hc <= 102))
			c = hc - 87;
		else if ((hc >= 65)&&(hc <= 70))
			c = hc - 55;

		if (h == hend) break;
		hc = *(reinterpret_cast<const uint8_t *>(h++));
		if (!hc) break;

		c <<= 4;
		if ((hc >= 48)&&(hc <= 57))
			c |= hc - 48;
		else if ((hc >= 97)&&(hc <= 102))
			c |= hc - 87;
		else if ((hc >= 65)&&(hc <= 70))
			c |= hc - 55;

		reinterpret_cast<uint8_t *>(buf)[l++] = c;
	}
	return l;
}

void Utils::getSecureRandom(void *buf,unsigned int bytes)
{
	static Mutex globalLock;
	static bool initialized = false;
	static uint64_t randomState[4];
	static uint8_t randomBuf[16384];
	static unsigned long randomPtr = sizeof(randomBuf);

	Mutex::Lock gl(globalLock);

	for(unsigned int i=0;i<bytes;++i) {
		if (randomPtr >= sizeof(randomBuf)) {
			randomPtr = 0;

			if (unlikely(!initialized)) {
				initialized = true;
#ifdef __WINDOWS__
				HCRYPTPROV cryptProvider = NULL;
				if (!CryptAcquireContextA(&cryptProvider,NULL,NULL,PROV_RSA_FULL,CRYPT_VERIFYCONTEXT|CRYPT_SILENT)) {
					fprintf(stderr,"FATAL ERROR: Utils::getSecureRandom() unable to obtain WinCrypt context!\r\n");
					exit(1);
				}
				if (!CryptGenRandom(cryptProvider,(DWORD)sizeof(randomState),(BYTE *)randomState)) {
					fprintf(stderr,"FATAL ERROR: Utils::getSecureRandom() CryptGenRandom failed!\r\n");
					exit(1);
				}
				if (!CryptGenRandom(cryptProvider,(DWORD)sizeof(randomBuf),(BYTE *)randomBuf)) {
					fprintf(stderr,"FATAL ERROR: Utils::getSecureRandom() CryptGenRandom failed!\r\n");
					exit(1);
				}
				CryptReleaseContext(cryptProvider,0);
#else
				int devURandomFd = ::open("/dev/urandom",O_RDONLY);
				if (devURandomFd < 0) {
					fprintf(stderr,"FATAL ERROR: Utils::getSecureRandom() unable to open /dev/urandom\n");
					exit(1);
				}
				if ((int)::read(devURandomFd,randomState,sizeof(randomState)) != (int)sizeof(randomState)) {
					::close(devURandomFd);
					fprintf(stderr,"FATAL ERROR: Utils::getSecureRandom() unable to read from /dev/urandom\n");
					exit(1);
				}
				if ((int)::read(devURandomFd,randomBuf,sizeof(randomBuf)) != (int)sizeof(randomBuf)) {
					::close(devURandomFd);
					fprintf(stderr,"FATAL ERROR: Utils::getSecureRandom() unable to read from /dev/urandom\n");
					exit(1);
				}
				close(devURandomFd);
#endif
				randomState[0] ^= (uint64_t)time(nullptr);
				randomState[1] ^= (uint64_t)((uintptr_t)buf); // XOR in some other entropy just in case the system random source is wonky
			}

			uint8_t h[48];
			for(unsigned int k=0;k<4;++k) {
				if (++randomState[k] != 0)
					break;
			}
			HMACSHA384((const uint8_t *)randomState,randomBuf,sizeof(randomBuf),h);
			AES c(h);
			c.ctr(h + 32,randomBuf,sizeof(randomBuf),randomBuf);
		}

		((uint8_t *)buf)[i] = randomBuf[randomPtr++];
	}
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
      result[count++] = "abcdefghijklmnopqrstuvwxyZ234567"[index];
    }
  }
  if (count < bufSize) {
		result[count] = (char)0;
		return count;
	}
	result[0] = (char)0;
	return -1;
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

#define ROL64(x,k) (((x) << (k)) | ((x) >> (64 - (k))))
uint64_t Utils::random()
{
	// https://en.wikipedia.org/wiki/Xorshift#xoshiro256**
	static Mutex l;
	static uint64_t s0 = Utils::getSecureRandom64();
	static uint64_t s1 = Utils::getSecureRandom64();
	static uint64_t s2 = Utils::getSecureRandom64();
	static uint64_t s3 = Utils::getSecureRandom64();

	l.lock();
	const uint64_t result = ROL64(s1 * 5,7) * 9;
	const uint64_t t = s1 << 17;
	s2 ^= s0;
	s3 ^= s1;
	s1 ^= s2;
	s0 ^= s3;
	s2 ^= t;
	s3 = ROL64(s3,45);
	l.unlock();

	return result;
}

} // namespace ZeroTier
