/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
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
#include <intrin.h>
#endif

#include "Utils.hpp"
#include "Mutex.hpp"
#include "Salsa20.hpp"

namespace ZeroTier {

const uint64_t Utils::ZERO256[4] = {0ULL,0ULL,0ULL,0ULL};

const char Utils::HEXCHARS[16] = { '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f' };

#ifdef ZT_ARCH_ARM_HAS_NEON
Utils::ARMCapabilities::ARMCapabilities() noexcept
{
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
}

const Utils::ARMCapabilities Utils::ARMCAP;
#endif

#ifdef ZT_ARCH_X64

Utils::CPUIDRegisters::CPUIDRegisters() noexcept
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

const Utils::CPUIDRegisters Utils::CPUID;
#endif

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
	if (pos >= 22) // sanity check, should be impossible
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

} // namespace ZeroTier
