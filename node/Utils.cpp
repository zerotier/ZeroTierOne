/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include <cstdio>
#include <cstdlib>
#include <ctime>

#include "Utils.hpp"
#include "Mutex.hpp"
#include "AES.hpp"
#include "SHA512.hpp"

#ifdef __UNIX_LIKE__
#include <unistd.h>
#include <fcntl.h>
#include <sys/uio.h>
#endif

#ifdef __WINDOWS__
#include <wincrypt.h>
#endif

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

const uint64_t ZERO256[4] = { 0,0,0,0 };
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
	s[pos] = (char)('0' + (n % 10));
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

char *hex(uint8_t i,char s[3])
{
	s[0] = HEXCHARS[(i >> 4U) & 0xfU];
	s[1] = HEXCHARS[i & 0xfU];
	s[2] = 0;
	return s;
}

char *hex(uint16_t i,char s[5])
{
	s[0] = HEXCHARS[(i >> 12U) & 0xfU];
	s[1] = HEXCHARS[(i >> 8U) & 0xfU];
	s[2] = HEXCHARS[(i >> 4U) & 0xfU];
	s[3] = HEXCHARS[i & 0xfU];
	s[4] = 0;
	return s;
}

char *hex(uint32_t i,char s[9])
{
	s[0] = HEXCHARS[(i >> 28U) & 0xfU];
	s[1] = HEXCHARS[(i >> 24U) & 0xfU];
	s[2] = HEXCHARS[(i >> 20U) & 0xfU];
	s[3] = HEXCHARS[(i >> 16U) & 0xfU];
	s[4] = HEXCHARS[(i >> 12U) & 0xfU];
	s[5] = HEXCHARS[(i >> 8U) & 0xfU];
	s[6] = HEXCHARS[(i >> 4U) & 0xfU];
	s[7] = HEXCHARS[i & 0xfU];
	s[8] = 0;
	return s;
}

char *hex(uint64_t i,char s[17])
{
	s[0] = HEXCHARS[(i >> 60U) & 0xfU];
	s[1] = HEXCHARS[(i >> 56U) & 0xfU];
	s[2] = HEXCHARS[(i >> 52U) & 0xfU];
	s[3] = HEXCHARS[(i >> 48U) & 0xfU];
	s[4] = HEXCHARS[(i >> 44U) & 0xfU];
	s[5] = HEXCHARS[(i >> 40U) & 0xfU];
	s[6] = HEXCHARS[(i >> 36U) & 0xfU];
	s[7] = HEXCHARS[(i >> 32U) & 0xfU];
	s[8] = HEXCHARS[(i >> 28U) & 0xfU];
	s[9] = HEXCHARS[(i >> 24U) & 0xfU];
	s[10] = HEXCHARS[(i >> 20U) & 0xfU];
	s[11] = HEXCHARS[(i >> 16U) & 0xfU];
	s[12] = HEXCHARS[(i >> 12U) & 0xfU];
	s[13] = HEXCHARS[(i >> 8U) & 0xfU];
	s[14] = HEXCHARS[(i >> 4U) & 0xfU];
	s[15] = HEXCHARS[i & 0xfU];
	s[16] = 0;
	return s;
}

uint64_t unhex(const char *s)
{
	uint64_t n = 0;
	if (s) {
		int k = 0;
		while (k < 16) {
			char hc = *(s++);
			if (!hc) break;

			uint8_t c = 0;
			if ((hc >= 48)&&(hc <= 57))
				c = hc - 48;
			else if ((hc >= 97)&&(hc <= 102))
				c = hc - 87;
			else if ((hc >= 65)&&(hc <= 70))
				c = hc - 55;

			n <<= 4U;
			n |= (uint64_t)c;
			++k;
		}
	}
	return n;
}

char *hex10(uint64_t i,char s[11])
{
	s[0] = HEXCHARS[(i >> 36U) & 0xfU];
	s[1] = HEXCHARS[(i >> 32U) & 0xfU];
	s[2] = HEXCHARS[(i >> 28U) & 0xfU];
	s[3] = HEXCHARS[(i >> 24U) & 0xfU];
	s[4] = HEXCHARS[(i >> 20U) & 0xfU];
	s[5] = HEXCHARS[(i >> 16U) & 0xfU];
	s[6] = HEXCHARS[(i >> 12U) & 0xfU];
	s[7] = HEXCHARS[(i >> 8U) & 0xfU];
	s[8] = HEXCHARS[(i >> 4U) & 0xfU];
	s[9] = HEXCHARS[i & 0xfU];
	s[10] = (char)0;
	return s;
}

char *hex(const void *d,unsigned int l,char *s)
{
	char *const save = s;
	for(unsigned int i=0;i<l;++i) {
		const unsigned int b = reinterpret_cast<const uint8_t *>(d)[i];
		*(s++) = HEXCHARS[b >> 4U];
		*(s++) = HEXCHARS[b & 0xfU];
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
	static uint64_t randomState[8];
	static uint64_t randomBuf[8192];
	static unsigned int randomPtr = 65536;

	Mutex::Lock gl(globalLock);

	for(unsigned int i=0;i<bytes;++i) {
		if (randomPtr >= 65536) {
			randomPtr = 0;

			if (!initialized) {
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

			++randomState[0];
			SHA512(randomState,randomState,sizeof(randomState));
			AES aes(reinterpret_cast<const uint8_t *>(randomState));
			uint64_t ctr[2],tmp[2];
			ctr[0] = randomState[6];
			ctr[1] = randomState[7];
			for(int k=0;k<8192;) {
				++ctr[0];
				aes.encrypt(reinterpret_cast<const uint8_t *>(ctr),reinterpret_cast<uint8_t *>(tmp));
				randomBuf[k] ^= tmp[0];
				randomBuf[k+1] ^= tmp[1];
				k += 2;
			}
		}

		reinterpret_cast<uint8_t *>(buf)[i] = reinterpret_cast<uint8_t *>(randomBuf)[randomPtr++];
	}
}

uint64_t getSecureRandomU64()
{
	uint64_t tmp = 0;
	getSecureRandom(&tmp,sizeof(tmp));
	return tmp;
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
          buffer <<= 8U;
          buffer |= data[next++] & 0xffU;
          bitsLeft += 8;
        } else {
          int pad = 5 - bitsLeft;
          buffer <<= pad;
          bitsLeft += pad;
        }
      }
      int index = 0x1f & (buffer >> (unsigned int)(bitsLeft - 5));
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
      ch = (ch & 0x1f) - 1;
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

#define ROL64(x,k) (((x) << (k)) | ((x) >> (64 - (k))))
uint64_t random()
{
	// https://en.wikipedia.org/wiki/Xorshift#xoshiro256**
	static volatile uint64_t s_s0 = getSecureRandomU64();
	static volatile uint64_t s_s1 = getSecureRandomU64();
	static volatile uint64_t s_s2 = getSecureRandomU64();
	static volatile uint64_t s_s3 = getSecureRandomU64();

	uint64_t s0 = s_s0;
	uint64_t s1 = s_s1;
	uint64_t s2 = s_s2;
	uint64_t s3 = s_s3;
	const uint64_t result = ROL64(s1 * 5,7U) * 9;
	const uint64_t t = s1 << 17U;
	s2 ^= s0;
	s3 ^= s1;
	s1 ^= s2;
	s0 ^= s3;
	s2 ^= t;
	s3 = ROL64(s3,45U);
	s_s0 = s0;
	s_s1 = s1;
	s_s2 = s2;
	s_s3 = s3;

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
