#ifndef ZT_SALSA2012_ARM32NEON_ASM
#define ZT_SALSA2012_ARM32NEON_ASM

#if defined(__linux__) || defined(linux) || defined(__LINUX__) || defined(__linux)
#include <sys/auxv.h>
#include <asm/hwcap.h>
#define zt_arm_has_neon() ((getauxval(AT_HWCAP) & HWCAP_NEON) != 0)
#elif defined(__ARM_NEON__) || defined(__ARM_NEON)
#define zt_arm_has_neon() (true)
#else
#define zt_arm_has_neon() (false)
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ciphertext buffer, message/NULL, length, nonce (8 bytes), key (32 bytes)
extern int zt_salsa2012_armneon3_xor(unsigned char *c,const unsigned char *m,unsigned long long len,const unsigned char *n,const unsigned char *k);

#ifdef __cplusplus
}
#endif

#endif
