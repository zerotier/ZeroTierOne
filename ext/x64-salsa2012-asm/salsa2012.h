#ifdef __cplusplus
extern "C" {
#endif

// output, outlen, nonce, key (256-bit / 32-byte)
extern int zt_salsa2012_amd64_xmm6(unsigned char *,unsigned long long,const unsigned char *,const unsigned char *);

// ciphertext, message, mlen, nonce, key
extern int zt_salsa2012_amd64_xmm6_xor(unsigned char *,const unsigned char *,unsigned long long,const unsigned char *,const unsigned char *);

#ifdef __cplusplus
}
#endif
