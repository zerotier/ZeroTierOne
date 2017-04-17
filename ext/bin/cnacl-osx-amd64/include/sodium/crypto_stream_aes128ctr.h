#ifndef crypto_stream_aes128ctr_H
#define crypto_stream_aes128ctr_H

#define crypto_stream_aes128ctr_portable_KEYBYTES 16
#define crypto_stream_aes128ctr_portable_NONCEBYTES 16
#define crypto_stream_aes128ctr_portable_BEFORENMBYTES 1408
#ifdef __cplusplus
#include <string>
extern std::string crypto_stream_aes128ctr_portable(size_t,const std::string &,const std::string &);
extern std::string crypto_stream_aes128ctr_portable_xor(const std::string &,const std::string &,const std::string &);
extern "C" {
#endif
extern int crypto_stream_aes128ctr_portable(unsigned char *,unsigned long long,const unsigned char *,const unsigned char *);
extern int crypto_stream_aes128ctr_portable_xor(unsigned char *,const unsigned char *,unsigned long long,const unsigned char *,const unsigned char *);
extern int crypto_stream_aes128ctr_portable_beforenm(unsigned char *,const unsigned char *);
extern int crypto_stream_aes128ctr_portable_afternm(unsigned char *,unsigned long long,const unsigned char *,const unsigned char *);
extern int crypto_stream_aes128ctr_portable_xor_afternm(unsigned char *,const unsigned char *,unsigned long long,const unsigned char *,const unsigned char *);
#ifdef __cplusplus
}
#endif
#define crypto_stream_aes128ctr crypto_stream_aes128ctr_portable
#define crypto_stream_aes128ctr_xor crypto_stream_aes128ctr_portable_xor
#define crypto_stream_aes128ctr_beforenm crypto_stream_aes128ctr_portable_beforenm
#define crypto_stream_aes128ctr_afternm crypto_stream_aes128ctr_portable_afternm
#define crypto_stream_aes128ctr_xor_afternm crypto_stream_aes128ctr_portable_xor_afternm
#define crypto_stream_aes128ctr_KEYBYTES crypto_stream_aes128ctr_portable_KEYBYTES
#define crypto_stream_aes128ctr_NONCEBYTES crypto_stream_aes128ctr_portable_NONCEBYTES
#define crypto_stream_aes128ctr_BEFORENMBYTES crypto_stream_aes128ctr_portable_BEFORENMBYTES
#define crypto_stream_aes128ctr_IMPLEMENTATION "crypto_stream/aes128ctr/portable"
#ifndef crypto_stream_aes128ctr_portable_VERSION
#define crypto_stream_aes128ctr_portable_VERSION "-"
#endif
#define crypto_stream_aes128ctr_VERSION crypto_stream_aes128ctr_portable_VERSION

#endif
