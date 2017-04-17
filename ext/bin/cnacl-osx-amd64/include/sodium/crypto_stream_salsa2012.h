#ifndef crypto_stream_salsa2012_H
#define crypto_stream_salsa2012_H

#define crypto_stream_salsa2012_amd64_xmm6_KEYBYTES 32
#define crypto_stream_salsa2012_amd64_xmm6_NONCEBYTES 8
#ifdef __cplusplus
#include <string>
extern std::string crypto_stream_salsa2012_amd64_xmm6(size_t,const std::string &,const std::string &);
extern std::string crypto_stream_salsa2012_amd64_xmm6_xor(const std::string &,const std::string &,const std::string &);
extern "C" {
#endif
extern int crypto_stream_salsa2012_amd64_xmm6(unsigned char *,unsigned long long,const unsigned char *,const unsigned char *);
extern int crypto_stream_salsa2012_amd64_xmm6_xor(unsigned char *,const unsigned char *,unsigned long long,const unsigned char *,const unsigned char *);
extern int crypto_stream_salsa2012_amd64_xmm6_beforenm(unsigned char *,const unsigned char *);
extern int crypto_stream_salsa2012_amd64_xmm6_afternm(unsigned char *,unsigned long long,const unsigned char *,const unsigned char *);
extern int crypto_stream_salsa2012_amd64_xmm6_xor_afternm(unsigned char *,const unsigned char *,unsigned long long,const unsigned char *,const unsigned char *);
#ifdef __cplusplus
}
#endif
#define crypto_stream_salsa2012 crypto_stream_salsa2012_amd64_xmm6
#define crypto_stream_salsa2012_xor crypto_stream_salsa2012_amd64_xmm6_xor
#define crypto_stream_salsa2012_beforenm crypto_stream_salsa2012_amd64_xmm6_beforenm
#define crypto_stream_salsa2012_afternm crypto_stream_salsa2012_amd64_xmm6_afternm
#define crypto_stream_salsa2012_xor_afternm crypto_stream_salsa2012_amd64_xmm6_xor_afternm
#define crypto_stream_salsa2012_KEYBYTES crypto_stream_salsa2012_amd64_xmm6_KEYBYTES
#define crypto_stream_salsa2012_NONCEBYTES crypto_stream_salsa2012_amd64_xmm6_NONCEBYTES
#define crypto_stream_salsa2012_BEFORENMBYTES crypto_stream_salsa2012_amd64_xmm6_BEFORENMBYTES
#define crypto_stream_salsa2012_IMPLEMENTATION "crypto_stream/salsa2012/amd64_xmm6"
#ifndef crypto_stream_salsa2012_amd64_xmm6_VERSION
#define crypto_stream_salsa2012_amd64_xmm6_VERSION "-"
#endif
#define crypto_stream_salsa2012_VERSION crypto_stream_salsa2012_amd64_xmm6_VERSION

#endif
