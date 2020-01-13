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

#include <cstdio>
#include <cstdlib>
#include <ctime>

#include "Constants.hpp"

#ifdef __UNIX_LIKE__
#include <unistd.h>
#include <fcntl.h>
#include <sys/uio.h>
#endif

#ifdef __WINDOWS__
#include <wincrypt.h>
#endif

#include "Utils.hpp"
#include "Mutex.hpp"
#include "AES.hpp"
#include "SHA512.hpp"

namespace ZeroTier {

namespace Utils {

#if (defined(__amd64) || defined(__amd64__) || defined(__x86_64) || defined(__x86_64__) || defined(__AMD64) || defined(__AMD64__) || defined(_M_X64))
CPUIDRegisters::CPUIDRegisters()
{
#ifdef __WINDOWS__
	int regs[4];
	__cpuid(regs,1);
	eax = (uint32_t)regs[0];
	ebx = (uint32_t)regs[1];
	ecx = (uint32_t)regs[2];
	edx = (uint32_t)regs[3];
#else
	__asm__ __volatile__ (
		"cpuid"
		: "=a"(eax),"=b"(ebx),"=c"(ecx),"=d"(edx)
		: "a"(1),"c"(0)
	);
#endif
	rdrand = ((ecx & (1U << 30U)) != 0);
	aes = ( ((ecx & (1U << 25U)) != 0) && ((ecx & (1U << 19U)) != 0) && ((ecx & (1U << 1U)) != 0) ); // AES, PCLMUL, SSE4.1
}
CPUIDRegisters CPUID;
#endif

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
				if (CPUID.rdrand) {
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
