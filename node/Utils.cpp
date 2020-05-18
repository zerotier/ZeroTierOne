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

#include "Utils.hpp"
#include "Mutex.hpp"
#include "AES.hpp"
#include "SHA512.hpp"

#ifdef __UNIX_LIKE__
#include <unistd.h>
#include <fcntl.h>
#include <sys/uio.h>
#endif

#include <time.h>

#ifdef __WINDOWS__
#include <wincrypt.h>
#endif

namespace ZeroTier {

namespace Utils {

#ifdef ZT_ARCH_X64
CPUIDRegisters::CPUIDRegisters() noexcept
{
	uint32_t eax,ebx,ecx,edx;
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
	aes = ( ((ecx & (1U << 25U)) != 0) && ((ecx & (1U << 19U)) != 0) && ((ecx & (1U << 1U)) != 0) );
	avx = ((ecx & (1U << 25U)) != 0);
#ifdef __WINDOWS__
TODO
#else
	__asm__ __volatile__ (
		"cpuid"
		: "=a"(eax),"=b"(ebx),"=c"(ecx),"=d"(edx)
		: "a"(7),"c"(0)
	);
#endif
	vaes = aes && avx && ((ecx & (1U << 9U)) != 0);
	vpclmulqdq = aes && avx && ((ecx & (1U << 10U)) != 0);
	avx2 = avx && ((ebx & (1U << 5U)) != 0);
	avx512f = avx && ((ebx & (1U << 16U)) != 0);
	sha = ((ebx & (1U << 29U)) != 0);
	fsrm = sha = ((edx & (1U << 4U)) != 0);
}
const CPUIDRegisters CPUID;
#endif

const uint64_t ZERO256[4] = { 0,0,0,0 };
const char HEXCHARS[16] = { '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f' };
const uint64_t s_mapNonce = getSecureRandomU64();

bool secureEq(const void *a,const void *b,unsigned int len) noexcept
{
	uint8_t diff = 0;
	for(unsigned int i=0;i<len;++i)
		diff |= ( (reinterpret_cast<const uint8_t *>(a))[i] ^ (reinterpret_cast<const uint8_t *>(b))[i] );
	return (diff == 0);
}

// Crazy hack to force memory to be securely zeroed in spite of the best efforts of optimizing compilers.
static void _Utils_doBurn(volatile uint8_t *ptr,unsigned int len)
{
	for(unsigned int i=0;i<len;++i)
		ptr[i] = 0;
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
char *decimal(unsigned long n,char s[24]) noexcept
{
	if (n == 0) {
		s[0] = '0';
		s[1] = (char)0;
		return s;
	}
	s[_Utils_itoa(n,s)] = (char)0;
	return s;
}

char *hex(uint64_t i,char buf[17]) noexcept
{
	if (i) {
		char *p = buf + 16;
		*p = 0;
		while (i) {
			*(--p) = HEXCHARS[i & 0xfU];
			i >>= 4;
		}
		return p;
	} else {
		buf[0] = '0';
		buf[1] = 0;
		return buf;
	}
}

uint64_t unhex(const char *s) noexcept
{
	uint64_t n = 0;
	if (s) {
		int k = 0;
		while (k < 16) {
			char hc = *(s++);
			if (!hc) break;

			uint8_t c = 0;
			if ((hc >= 48)&&(hc <= 57))
				c = (uint8_t)hc - 48;
			else if ((hc >= 97)&&(hc <= 102))
				c = (uint8_t)hc - 87;
			else if ((hc >= 65)&&(hc <= 70))
				c = (uint8_t)hc - 55;

			n <<= 4U;
			n |= (uint64_t)c;
			++k;
		}
	}
	return n;
}

char *hex(const void *d,unsigned int l,char *s) noexcept
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

unsigned int unhex(const char *h,unsigned int hlen,void *buf,unsigned int buflen) noexcept
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

		c <<= 4U;
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

#define ZT_GETSECURERANDOM_STATE_SIZE 64
#define ZT_GETSECURERANDOM_ITERATIONS_PER_GENERATOR 1048576

void getSecureRandom(void *const buf,unsigned int bytes) noexcept
{
	static Mutex globalLock;
	static bool initialized = false;
	static uint64_t randomState[ZT_GETSECURERANDOM_STATE_SIZE];
	static unsigned int randomByteCounter = ZT_GETSECURERANDOM_ITERATIONS_PER_GENERATOR; // init on first run
	static AES randomGen;

	Mutex::Lock gl(globalLock);

	// Re-initialize the generator every ITERATIONS_PER_GENERATOR bytes.
	if (unlikely((randomByteCounter += bytes) >= ZT_GETSECURERANDOM_ITERATIONS_PER_GENERATOR)) {
		// On first run fill randomState with random bits from the system.
		if (unlikely(!initialized)) {
			initialized = true;

			// Don't let randomState be swapped to disk (if supported by OS).
			Utils::memoryLock(randomState,sizeof(randomState));

			// Fill randomState with entropy from the system. Failure equals hard exit.
			Utils::zero<sizeof(randomState)>(randomState);
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
				CryptReleaseContext(cryptProvider,0);
#else
			int devURandomFd = ::open("/dev/urandom",O_RDONLY);
			if (devURandomFd < 0) {
				fprintf(stderr,"FATAL: Utils::getSecureRandom() unable to open /dev/urandom\n");
				exit(1);
			}
			if ((long)::read(devURandomFd,randomState,sizeof(randomState)) != (long)sizeof(randomState)) {
				::close(devURandomFd);
				fprintf(stderr,"FATAL: Utils::getSecureRandom() unable to read from /dev/urandom\n");
				exit(1);
			}
			close(devURandomFd);
#endif

			// Mix in additional entropy from time, the address of 'buf', CPU RDRAND if present, etc.
			randomState[0] += (uint64_t)time(nullptr);
			randomState[1] += (uint64_t)((uintptr_t)buf);
#ifdef __UNIX_LIKE__
			randomState[2] += (uint64_t)getpid();
			randomState[3] += (uint64_t)getppid();
#endif
#ifdef ZT_ARCH_X64
			if (CPUID.rdrand) {
				uint64_t tmp = 0;
				for(int k=0;k<ZT_GETSECURERANDOM_STATE_SIZE;++k) {
					_rdrand64_step((unsigned long long *)&tmp);
					randomState[k] ^= tmp;
				}
			}
#endif
		}

		// Initialize or re-initialize generator by hashing the full state,
		// replacing the first 64 bytes with this hash, and then re-initializing
		// AES with the first 32 bytes.
		randomByteCounter = 0;
		SHA512(randomState,randomState,sizeof(randomState));
		randomGen.init(randomState);
	}

	// Generate random bytes using AES and bytes 32-48 of randomState as an in-place
	// AES-CTR counter. Counter can be machine endian; we don't care about portability
	// for a random generator.
	uint64_t *const ctr = randomState + 4;
	uint8_t *out = reinterpret_cast<uint8_t *>(buf);
	while (bytes >= 16) {
		++*ctr;
		randomGen.encrypt(ctr,out);
		out += 16;
		bytes -= 16;
	}
	if (bytes > 0) {
		uint8_t tmp[16];
		++*ctr;
		randomGen.encrypt(ctr,tmp);
		for(unsigned int i=0;i<bytes;++i)
			out[i] = tmp[i];
		Utils::burn(tmp,sizeof(tmp)); // don't leave used cryptographic randomness lying around!
	}
}

uint64_t getSecureRandomU64() noexcept
{
	uint64_t tmp = 0;
	getSecureRandom(&tmp,sizeof(tmp));
	return tmp;
}

int b32e(const uint8_t *data,int length,char *result,int bufSize) noexcept
{
  if (length < 0 || length > (1 << 28U)) {
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

int b32d(const char *encoded,uint8_t *result,int bufSize) noexcept
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

uint64_t random() noexcept
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
	const uint64_t s1x5 = s1 * 5;
	const uint64_t result = ((s1x5 << 7U)|(s1x5 >> 57U)) * 9;
	const uint64_t t = s1 << 17U;
	s2 ^= s0;
	s3 ^= s1;
	s1 ^= s2;
	s0 ^= s3;
	s2 ^= t;
	s3 = ((s3 << 45U)|(s3 >> 19U));
	s_s0 = s0;
	s_s1 = s1;
	s_s2 = s2;
	s_s3 = s3;

	return result;
}

bool scopy(char *const dest,const unsigned int len,const char *const src) noexcept
{
	if (!len)
		return false; // sanity check
	if (!src) {
		*dest = (char)0;
		return true;
	}
	unsigned int i = 0;
	for(;;) {
		if (i >= len) {
			dest[len - 1] = 0;
			return false;
		}
		if ((dest[i] = src[i]) == 0)
			return true;
		++i;
	}
}

} // namespace Utils

} // namespace ZeroTier
