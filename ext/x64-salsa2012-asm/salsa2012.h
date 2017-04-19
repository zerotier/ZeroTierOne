#ifndef ZT_X64_SALSA2012_ASM
#define ZT_X64_SALSA2012_ASM

#ifdef __cplusplus
extern "C" {
#endif

// Generates Salsa20/12 key stream
// output, outlen, nonce, key (256-bit / 32-byte)
extern int zt_salsa2012_amd64_xmm6(unsigned char *, unsigned long long, const unsigned char *, const unsigned char *);

#ifdef __cplusplus
}
#endif

#endif
