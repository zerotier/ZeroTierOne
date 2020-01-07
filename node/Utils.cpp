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

#if (defined(__amd64) || defined(__amd64__) || defined(__x86_64) || defined(__x86_64__) || defined(__AMD64) || defined(__AMD64__) || defined(_M_X64))
#include <immintrin.h>
static bool _zt_rdrand_supported()
{
#ifdef __WINDOWS__
	int regs[4];
	__cpuid(regs,1);
	return (((regs[2] >> 30) & 1) != 0);
#else
	uint32_t eax,ebx,ecx,edx;
	__asm__ __volatile__ (
		"cpuid"
		: "=a"(eax),"=b"(ebx),"=c"(ecx),"=d"(edx)
		: "a"(1),"c"(0)
	);
	return ((ecx & (1 << 30)) != 0);
#endif
}
static const bool _rdrandSupported = _zt_rdrand_supported();
#endif

namespace ZeroTier {

namespace Utils {

const char HEXCHARS[16] = { '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f' };

bool secureEq(const void *a,const void *b,unsigned int len)
{
	uint8_t diff = 0;
	for(unsigned int i=0;i<len;++i)
		diff |= ( (reinterpret_cast<const uint8_t *>(a))[i] ^ (reinterpret_cast<const uint8_t *>(b))[i] );
	return (diff == 0);
}

// Crazy hack to force memory to be securely zeroed in spite of the best efforts of optimizing compilers.
static void _Utils_doBurn(volatile uint8_t *ptr,unsigned int len)
{
	volatile uint8_t *const end = ptr + len;
	while (ptr != end) *(ptr++) = (uint8_t)0;
}
static void (*volatile _Utils_doBurn_ptr)(volatile uint8_t *,unsigned int) = _Utils_doBurn;
void burn(void *ptr,unsigned int len) { (_Utils_doBurn_ptr)((volatile uint8_t *)ptr,len); }

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
char *decimal(unsigned long n,char s[24])
{
	if (n == 0) {
		s[0] = '0';
		s[1] = (char)0;
		return s;
	}
	s[_Utils_itoa(n,s)] = (char)0;
	return s;
}

unsigned short crc16(const void *buf,unsigned int len)
{
	static const uint16_t crc16tab[256]= { 0x0000,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef,0x1231,0x0210,0x3273,0x2252,0x52b5,0x4294,0x72f7,0x62d6,0x9339,0x8318,0xb37b,0xa35a,0xd3bd,0xc39c,0xf3ff,0xe3de,0x2462,0x3443,0x0420,0x1401,0x64e6,0x74c7,0x44a4,0x5485,0xa56a,0xb54b,0x8528,0x9509,0xe5ee,0xf5cf,0xc5ac,0xd58d,0x3653,0x2672,0x1611,0x0630,0x76d7,0x66f6,0x5695,0x46b4,0xb75b,0xa77a,0x9719,0x8738,0xf7df,0xe7fe,0xd79d,0xc7bc,0x48c4,0x58e5,0x6886,0x78a7,0x0840,0x1861,0x2802,0x3823,0xc9cc,0xd9ed,0xe98e,0xf9af,0x8948,0x9969,0xa90a,0xb92b,0x5af5,0x4ad4,0x7ab7,0x6a96,0x1a71,0x0a50,0x3a33,0x2a12,0xdbfd,0xcbdc,0xfbbf,0xeb9e,0x9b79,0x8b58,0xbb3b,0xab1a,0x6ca6,0x7c87,0x4ce4,0x5cc5,0x2c22,0x3c03,0x0c60,0x1c41,0xedae,0xfd8f,0xcdec,0xddcd,0xad2a,0xbd0b,0x8d68,0x9d49,0x7e97,0x6eb6,0x5ed5,0x4ef4,0x3e13,0x2e32,0x1e51,0x0e70,0xff9f,0xefbe,0xdfdd,0xcffc,0xbf1b,0xaf3a,0x9f59,0x8f78,0x9188,0x81a9,0xb1ca,0xa1eb,0xd10c,0xc12d,0xf14e,0xe16f,0x1080,0x00a1,0x30c2,0x20e3,0x5004,0x4025,0x7046,0x6067,0x83b9,0x9398,0xa3fb,0xb3da,0xc33d,0xd31c,0xe37f,0xf35e,0x02b1,0x1290,0x22f3,0x32d2,0x4235,0x5214,0x6277,0x7256,0xb5ea,0xa5cb,0x95a8,0x8589,0xf56e,0xe54f,0xd52c,0xc50d,0x34e2,0x24c3,0x14a0,0x0481,0x7466,0x6447,0x5424,0x4405,0xa7db,0xb7fa,0x8799,0x97b8,0xe75f,0xf77e,0xc71d,0xd73c,0x26d3,0x36f2,0x0691,0x16b0,0x6657,0x7676,0x4615,0x5634,0xd94c,0xc96d,0xf90e,0xe92f,0x99c8,0x89e9,0xb98a,0xa9ab,0x5844,0x4865,0x7806,0x6827,0x18c0,0x08e1,0x3882,0x28a3,0xcb7d,0xdb5c,0xeb3f,0xfb1e,0x8bf9,0x9bd8,0xabbb,0xbb9a,0x4a75,0x5a54,0x6a37,0x7a16,0x0af1,0x1ad0,0x2ab3,0x3a92,0xfd2e,0xed0f,0xdd6c,0xcd4d,0xbdaa,0xad8b,0x9de8,0x8dc9,0x7c26,0x6c07,0x5c64,0x4c45,0x3ca2,0x2c83,0x1ce0,0x0cc1,0xef1f,0xff3e,0xcf5d,0xdf7c,0xaf9b,0xbfba,0x8fd9,0x9ff8,0x6e17,0x7e36,0x4e55,0x5e74,0x2e93,0x3eb2,0x0ed1,0x1ef0 };
	uint16_t crc = 0;
	const uint8_t *p = (const uint8_t *)buf;
	for(unsigned int i=0;i<len;++i)
		crc = (crc << 8) ^ crc16tab[((crc >> 8) ^ *(p++)) & 0x00ff];
	return crc;
}

char *hex10(uint64_t i,char s[11])
{
	s[0] = HEXCHARS[(i >> 36) & 0xf];
	s[1] = HEXCHARS[(i >> 32) & 0xf];
	s[2] = HEXCHARS[(i >> 28) & 0xf];
	s[3] = HEXCHARS[(i >> 24) & 0xf];
	s[4] = HEXCHARS[(i >> 20) & 0xf];
	s[5] = HEXCHARS[(i >> 16) & 0xf];
	s[6] = HEXCHARS[(i >> 12) & 0xf];
	s[7] = HEXCHARS[(i >> 8) & 0xf];
	s[8] = HEXCHARS[(i >> 4) & 0xf];
	s[9] = HEXCHARS[i & 0xf];
	s[10] = (char)0;
	return s;
}

char *hex(const void *d,unsigned int l,char *s)
{
	char *const save = s;
	for(unsigned int i=0;i<l;++i) {
		const unsigned int b = reinterpret_cast<const uint8_t *>(d)[i];
		*(s++) = HEXCHARS[b >> 4];
		*(s++) = HEXCHARS[b & 0xf];
	}
	*s = (char)0;
	return save;
}

unsigned int unhex(const char *h,unsigned int hlen,void *buf,unsigned int buflen)
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

void getSecureRandom(void *buf,unsigned int bytes)
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
					fprintf(stderr,"FATAL: Utils::getSecureRandom() unable to obtain WinCrypt context!\r\n");
					exit(1);
				}
				if (!CryptGenRandom(cryptProvider,(DWORD)sizeof(randomState),(BYTE *)randomState)) {
					fprintf(stderr,"FATAL: Utils::getSecureRandom() CryptGenRandom failed!\r\n");
					exit(1);
				}
				if (!CryptGenRandom(cryptProvider,(DWORD)sizeof(randomBuf),(BYTE *)randomBuf)) {
					fprintf(stderr,"FATAL: Utils::getSecureRandom() CryptGenRandom failed!\r\n");
					exit(1);
				}
				CryptReleaseContext(cryptProvider,0);
#else
				int devURandomFd = ::open("/dev/urandom",O_RDONLY);
				if (devURandomFd < 0) {
					fprintf(stderr,"FATAL: Utils::getSecureRandom() unable to open /dev/urandom\n");
					exit(1);
				}
				if ((int)::read(devURandomFd,randomState,sizeof(randomState)) != (int)sizeof(randomState)) {
					::close(devURandomFd);
					fprintf(stderr,"FATAL: Utils::getSecureRandom() unable to read from /dev/urandom\n");
					exit(1);
				}
				if ((int)::read(devURandomFd,randomBuf,sizeof(randomBuf)) != (int)sizeof(randomBuf)) {
					::close(devURandomFd);
					fprintf(stderr,"FATAL: Utils::getSecureRandom() unable to read from /dev/urandom\n");
					exit(1);
				}
				close(devURandomFd);
#endif

				// Mix in additional entropy just in case the standard random source is wonky somehow
				randomState[0] ^= (uint64_t)time(nullptr);
				randomState[1] ^= (uint64_t)((uintptr_t)buf);
#if (defined(__amd64) || defined(__amd64__) || defined(__x86_64) || defined(__x86_64__) || defined(__AMD64) || defined(__AMD64__) || defined(_M_X64))
				if (_rdrandSupported) {
					uint64_t tmp = 0;
					_rdrand64_step((unsigned long long *)&tmp);
					randomState[2] ^= tmp;
					_rdrand64_step((unsigned long long *)&tmp);
					randomState[3] ^= tmp;
				}
#endif
			}

			for(unsigned int k=0;k<4;++k) { // treat random state like a 256-bit counter; endian-ness is irrelevant since we just want random
				if (++randomState[k] != 0)
					break;
			}
			uint8_t h[48];
			HMACSHA384((const uint8_t *)randomState,randomBuf,sizeof(randomBuf),h); // compute HMAC on random buffer using state as secret key
			AES c(h);
			c.ctr(h + 32,randomBuf,sizeof(randomBuf),randomBuf); // encrypt random buffer with AES-CTR using HMAC result as key
		}

		((uint8_t *)buf)[i] = randomBuf[randomPtr++];
	}
}

int b32e(const uint8_t *data,int length,char *result,int bufSize)
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
      result[count++] = "abcdefghijklmnopqrstuvwxyz234567"[index];
    }
  }
  if (count < bufSize) {
		result[count] = (char)0;
		return count;
	}
	result[0] = (char)0;
	return -1;
}

int b32d(const char *encoded,uint8_t *result,int bufSize)
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

static uint64_t _secureRandom64()
{
	uint64_t tmp = 0;
	getSecureRandom(&tmp,sizeof(tmp));
	return tmp;
}

#define ROL64(x,k) (((x) << (k)) | ((x) >> (64 - (k))))
uint64_t random()
{
	// https://en.wikipedia.org/wiki/Xorshift#xoshiro256**
	static Mutex l;
	static uint64_t s0 = _secureRandom64();
	static uint64_t s1 = _secureRandom64();
	static uint64_t s2 = _secureRandom64();
	static uint64_t s3 = _secureRandom64();

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

bool scopy(char *dest,unsigned int len,const char *src)
{
	if (!len)
		return false; // sanity check
	if (!src) {
		*dest = (char)0;
		return true;
	}
	char *const end = dest + len;
	while ((*dest++ = *src++)) {
		if (dest == end) {
			*(--dest) = (char)0;
			return false;
		}
	}
	return true;
}

} // namespace Utils

} // namespace ZeroTier
