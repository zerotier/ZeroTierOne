/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include "Utils.hpp"
#include "Mutex.hpp"
#include "AES.hpp"
#include "SHA512.hpp"

#include <time.h>

#ifdef __UNIX_LIKE__
#include <unistd.h>
#include <fcntl.h>
#include <sys/uio.h>
#endif

#ifdef __WINDOWS__
#include <intrin.h>
#include <wincrypt.h>
#endif

#ifdef ZT_ARCH_ARM_HAS_NEON

#ifdef __LINUX__
#include <sys/auxv.h>
#include <asm/hwcap.h>
#endif

#if defined(__FreeBSD__)
#include <elf.h>
#include <sys/auxv.h>
static inline long getauxval(int caps)
{
	long hwcaps = 0;
	elf_aux_info(caps, &hwcaps, sizeof(hwcaps));
	return hwcaps;
}
#endif

// If these are not even defined, obviously they are not supported.
#ifndef HWCAP_AES
#define HWCAP_AES 0
#endif
#ifndef HWCAP_CRC32
#define HWCAP_CRC32 0
#endif
#ifndef HWCAP_PMULL
#define HWCAP_PMULL 0
#endif
#ifndef HWCAP_SHA1
#define HWCAP_SHA1 0
#endif
#ifndef HWCAP_SHA2
#define HWCAP_SHA2 0
#endif

#endif /* ZT_ARCH_ARM_HAS_NEON */

namespace ZeroTier {

namespace Utils {

#ifdef ZT_ARCH_ARM_HAS_NEON
ARMCapabilities::ARMCapabilities() noexcept
{

#ifdef __APPLE__

	this->aes = true;
	this->crc32 = true;
	this->pmull = true;
	this->sha1 = true;
	this->sha2 = true;

#else

#ifdef __LINUX__
#ifdef HWCAP2_AES
	if (sizeof(void *) == 4) {
		const long hwcaps2 = getauxval(AT_HWCAP2);
		this->aes = (hwcaps2 & HWCAP2_AES) != 0;
		this->crc32 = (hwcaps2 & HWCAP2_CRC32) != 0;
		this->pmull = (hwcaps2 & HWCAP2_PMULL) != 0;
		this->sha1 = (hwcaps2 & HWCAP2_SHA1) != 0;
		this->sha2 = (hwcaps2 & HWCAP2_SHA2) != 0;
	} else {
#endif
		const long hwcaps = getauxval(AT_HWCAP);
		this->aes = (hwcaps & HWCAP_AES) != 0;
		this->crc32 = (hwcaps & HWCAP_CRC32) != 0;
		this->pmull = (hwcaps & HWCAP_PMULL) != 0;
		this->sha1 = (hwcaps & HWCAP_SHA1) != 0;
		this->sha2 = (hwcaps & HWCAP_SHA2) != 0;
#ifdef HWCAP2_AES
	}
#endif
#endif

#endif

}

const ARMCapabilities ARMCAP;
#endif /* ZT_ARCH_ARM_HAS_NEON */

#ifdef ZT_ARCH_X64
CPUIDRegisters::CPUIDRegisters() noexcept
{
	uint32_t eax, ebx, ecx, edx;

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
	: "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
	: "a"(1), "c"(0)
	);
#endif

	rdrand = ((ecx & (1U << 30U)) != 0);
	aes = (((ecx & (1U << 25U)) != 0) && ((ecx & (1U << 19U)) != 0) && ((ecx & (1U << 1U)) != 0));
	avx = ((ecx & (1U << 25U)) != 0);

#ifdef __WINDOWS__
	__cpuid(regs,7);
	eax = (uint32_t)regs[0];
	ebx = (uint32_t)regs[1];
	ecx = (uint32_t)regs[2];
	edx = (uint32_t)regs[3];
#else
	__asm__ __volatile__ (
	"cpuid"
	: "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
	: "a"(7), "c"(0)
	);
#endif

	vaes = aes && avx && ((ecx & (1U << 9U)) != 0);
	vpclmulqdq = aes && avx && ((ecx & (1U << 10U)) != 0);
	avx2 = avx && ((ebx & (1U << 5U)) != 0);
	avx512f = avx && ((ebx & (1U << 16U)) != 0);
	sha = ((ebx & (1U << 29U)) != 0);
	fsrm = ((edx & (1U << 4U)) != 0);
}

const CPUIDRegisters CPUID;
#endif /* ZT_ARCH_X64 */

const std::bad_alloc BadAllocException;
const std::out_of_range OutOfRangeException("access out of range");
const uint64_t ZERO256[4] = {0, 0, 0, 0};
const char HEXCHARS[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
const uint64_t s_mapNonce = getSecureRandomU64();

bool secureEq(const void *a, const void *b, unsigned int len) noexcept
{
	uint8_t diff = 0;
	for (unsigned int i = 0; i < len; ++i)
		diff |= ((reinterpret_cast<const uint8_t *>(a))[i] ^ (reinterpret_cast<const uint8_t *>(b))[i]);
	return (diff == 0);
}

void burn(volatile void *ptr, unsigned int len)
{
	static volatile uintptr_t foo = 0;
	Utils::zero((void *)ptr, len);
	// Force compiler not to optimize this function out by taking a volatile
	// parameter and also updating a volatile variable.
	foo += (uintptr_t)len ^ (uintptr_t)reinterpret_cast<volatile uint8_t *>(ptr)[0];
}

static unsigned long s_decimalRecursive(unsigned long n, char *s)
{
	if (n == 0)
		return 0;
	unsigned long pos = s_decimalRecursive(n / 10, s);
	if (pos >= 22) // sanity check,should be impossible
		pos = 22;
	s[pos] = (char)('0' + (n % 10));
	return pos + 1;
}

char *decimal(unsigned long n, char s[24]) noexcept
{
	if (n == 0) {
		s[0] = '0';
		s[1] = (char)0;
		return s;
	}
	s[s_decimalRecursive(n, s)] = (char)0;
	return s;
}

char *hex(uint64_t i, char buf[17]) noexcept
{
	if (i != 0) {
		char *p = nullptr;
		for (int b = 60; b >= 0; b -= 4) {
			const unsigned int nyb = (unsigned int)(i >> (unsigned int)b) & 0xfU;
			if (p) {
				*(p++) = HEXCHARS[nyb];
			} else if (nyb != 0) {
				p = buf;
				*(p++) = HEXCHARS[nyb];
			}
		}
		*p = 0;
		return buf;
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
			if ((hc >= 48) && (hc <= 57))
				c = (uint8_t)hc - 48;
			else if ((hc >= 97) && (hc <= 102))
				c = (uint8_t)hc - 87;
			else if ((hc >= 65) && (hc <= 70))
				c = (uint8_t)hc - 55;

			n <<= 4U;
			n |= (uint64_t)c;
			++k;
		}
	}
	return n;
}

char *hex(const void *d, unsigned int l, char *s) noexcept
{
	char *const save = s;
	for (unsigned int i = 0; i < l; ++i) {
		const unsigned int b = reinterpret_cast<const uint8_t *>(d)[i];
		*(s++) = HEXCHARS[b >> 4U];
		*(s++) = HEXCHARS[b & 0xfU];
	}
	*s = (char)0;
	return save;
}

unsigned int unhex(const char *h, unsigned int hlen, void *buf, unsigned int buflen) noexcept
{
	unsigned int l = 0;
	const char *hend = h + hlen;
	while (l < buflen) {
		if (h == hend) break;
		uint8_t hc = *(reinterpret_cast<const uint8_t *>(h++));
		if (!hc) break;

		uint8_t c = 0;
		if ((hc >= 48) && (hc <= 57))
			c = hc - 48;
		else if ((hc >= 97) && (hc <= 102))
			c = hc - 87;
		else if ((hc >= 65) && (hc <= 70))
			c = hc - 55;

		if (h == hend) break;
		hc = *(reinterpret_cast<const uint8_t *>(h++));
		if (!hc) break;

		c <<= 4U;
		if ((hc >= 48) && (hc <= 57))
			c |= hc - 48;
		else if ((hc >= 97) && (hc <= 102))
			c |= hc - 87;
		else if ((hc >= 65) && (hc <= 70))
			c |= hc - 55;

		reinterpret_cast<uint8_t *>(buf)[l++] = c;
	}
	return l;
}

#define ZT_GETSECURERANDOM_STATE_SIZE 64
#define ZT_GETSECURERANDOM_ITERATIONS_PER_GENERATOR 1048576

void getSecureRandom(void *const buf, unsigned int bytes) noexcept
{
	static Mutex globalLock;
	static bool initialized = false;
	static uint64_t randomState[ZT_GETSECURERANDOM_STATE_SIZE];
	static unsigned int randomByteCounter = ZT_GETSECURERANDOM_ITERATIONS_PER_GENERATOR; // init on first run
	static AES randomGen;

	Mutex::Lock gl(globalLock);

	// Re-initialize the PRNG every ZT_GETSECURERANDOM_ITERATIONS_PER_GENERATOR bytes. Note that
	// if 'bytes' is larger than ZT_GETSECURERANDOM_ITERATIONS_PER_GENERATOR we can generate more
	// than this, but this isn't an issue. ZT_GETSECURERANDOM_ITERATIONS_PER_GENERATOR could be
	// much larger if we wanted and this would still be safe.
	randomByteCounter += bytes;
	if (unlikely(randomByteCounter >= ZT_GETSECURERANDOM_ITERATIONS_PER_GENERATOR)) {
		randomByteCounter = 0;

		if (unlikely(!initialized)) {
			initialized = true;
			Utils::memoryLock(randomState, sizeof(randomState));
			Utils::zero< sizeof(randomState) >(randomState);
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
			int devURandomFd = ::open("/dev/urandom", O_RDONLY);
			if (devURandomFd < 0) {
				fprintf(stderr, "FATAL: Utils::getSecureRandom() unable to open /dev/urandom\n");
				exit(1);
			}
			if ((long)::read(devURandomFd, randomState, sizeof(randomState)) != (long)sizeof(randomState)) {
				::close(devURandomFd);
				fprintf(stderr, "FATAL: Utils::getSecureRandom() unable to read from /dev/urandom\n");
				exit(1);
			}
			close(devURandomFd);
#endif
#ifdef __UNIX_LIKE__
			randomState[0] += (uint64_t)getpid();
			randomState[1] += (uint64_t)getppid();
#endif
#ifdef ZT_ARCH_X64
			if (CPUID.rdrand) {
				uint64_t tmp = 0;
				for (unsigned long i = 0; i < ZT_GETSECURERANDOM_STATE_SIZE; ++i) {
					_rdrand64_step((unsigned long long *)&tmp);
					randomState[i] ^= tmp;
				}
			}
#endif
		}

		// Initialize or re-initialize generator by hashing the full state,
		// replacing the first 64 bytes with this hash, and then re-initializing
		// AES with the first 32 bytes.
		randomState[0] += (uint64_t)time(nullptr);
		SHA512(randomState, randomState, sizeof(randomState));
		randomGen.init(randomState);
	}

	// Generate random bytes using AES and bytes 32-48 of randomState as an in-place
	// AES-CTR counter. Counter can be machine endian; we don't care about portability
	// for a random generator.
	uint64_t *const ctr = randomState + 4;
	uint8_t *out = reinterpret_cast<uint8_t *>(buf);

	while (bytes >= 16) {
		++*ctr;
		randomGen.encrypt(ctr, out);
		out += 16;
		bytes -= 16;
	}

	if (bytes > 0) {
		uint8_t tmp[16];
		++*ctr;
		randomGen.encrypt(ctr, tmp);
		for (unsigned int i = 0; i < bytes; ++i)
			out[i] = tmp[i];
		Utils::burn(tmp, sizeof(tmp)); // don't leave used cryptographic randomness lying around!
	}
}

uint64_t getSecureRandomU64() noexcept
{
	uint64_t tmp;
	getSecureRandom(&tmp, sizeof(tmp));
	return tmp;
}

int b32e(const uint8_t *data, int length, char *result, int bufSize) noexcept
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

int b32d(const char *encoded, uint8_t *result, int bufSize) noexcept
{
	int buffer = 0;
	int bitsLeft = 0;
	int count = 0;
	for (const uint8_t *ptr = (const uint8_t *)encoded; count < bufSize && *ptr; ++ptr) {
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
	static volatile uint64_t s_s0 = getSecureRandomU64();
	static volatile uint64_t s_s1 = getSecureRandomU64();
	static volatile uint64_t s_s2 = getSecureRandomU64();
	static volatile uint64_t s_s3 = getSecureRandomU64();

	// https://en.wikipedia.org/wiki/Xorshift#xoshiro256**
	uint64_t s0 = s_s0;
	uint64_t s1 = s_s1;
	uint64_t s2 = s_s2;
	uint64_t s3 = s_s3;
	const uint64_t s1x5 = s1 * 5ULL;
	const uint64_t result = ((s1x5 << 7U) | (s1x5 >> 57U)) * 9ULL;
	const uint64_t t = s1 << 17U;
	s2 ^= s0;
	s3 ^= s1;
	s1 ^= s2;
	s0 ^= s3;
	s2 ^= t;
	s3 = ((s3 << 45U) | (s3 >> 19U));
	s_s0 = s0;
	s_s1 = s1;
	s_s2 = s2;
	s_s3 = s3;

	return result;
}

bool scopy(char *const dest, const unsigned int len, const char *const src) noexcept
{
	if (unlikely((len == 0)||(dest == nullptr))) {
		return false;
	}
	if (unlikely(src == nullptr)) {
		*dest = (char)0;
		return true;
	}
	unsigned int i = 0;
	for (;;) {
		if (i >= len) {
			dest[len - 1] = 0;
			return false;
		}
		if ((dest[i] = src[i]) == 0) {
			return true;
		}
		++i;
	}
}

uint32_t fnv1a32(const void *const restrict data, const unsigned int len) noexcept
{
	uint32_t h = 0x811c9dc5;
	const uint32_t p = 0x01000193;
	for (unsigned int i = 0; i < len; ++i)
		h = (h ^ (uint32_t)reinterpret_cast<const uint8_t *>(data)[i]) * p;
	return h;
}

} // namespace Utils

} // namespace ZeroTier
