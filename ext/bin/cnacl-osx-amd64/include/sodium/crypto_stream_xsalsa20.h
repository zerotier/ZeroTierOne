#ifndef crypto_stream_xsalsa20_H
#define crypto_stream_xsalsa20_H

#define crypto_stream_xsalsa20_ref_KEYBYTES 32
#define crypto_stream_xsalsa20_ref_NONCEBYTES 24
#ifdef __cplusplus
#include <string>
extern std::string crypto_stream_xsalsa20_ref(size_t,const std::string &,const std::string &);
extern std::string crypto_stream_xsalsa20_ref_xor(const std::string &,const std::string &,const std::string &);
extern "C" {
#endif
extern int crypto_stream_xsalsa20_ref(unsigned char *,unsigned long long,const unsigned char *,const unsigned char *);
extern int crypto_stream_xsalsa20_ref_xor(unsigned char *,const unsigned char *,unsigned long long,const unsigned char *,const unsigned char *);
extern int crypto_stream_xsalsa20_ref_beforenm(unsigned char *,const unsigned char *);
extern int crypto_stream_xsalsa20_ref_afternm(unsigned char *,unsigned long long,const unsigned char *,const unsigned char *);
extern int crypto_stream_xsalsa20_ref_xor_afternm(unsigned char *,const unsigned char *,unsigned long long,const unsigned char *,const unsigned char *);
#ifdef __cplusplus
}
#endif
#define crypto_stream_xsalsa20 crypto_stream_xsalsa20_ref
#define crypto_stream_xsalsa20_xor crypto_stream_xsalsa20_ref_xor
#define crypto_stream_xsalsa20_beforenm crypto_stream_xsalsa20_ref_beforenm
#define crypto_stream_xsalsa20_afternm crypto_stream_xsalsa20_ref_afternm
#define crypto_stream_xsalsa20_xor_afternm crypto_stream_xsalsa20_ref_xor_afternm
#define crypto_stream_xsalsa20_KEYBYTES crypto_stream_xsalsa20_ref_KEYBYTES
#define crypto_stream_xsalsa20_NONCEBYTES crypto_stream_xsalsa20_ref_NONCEBYTES
#define crypto_stream_xsalsa20_BEFORENMBYTES crypto_stream_xsalsa20_ref_BEFORENMBYTES
#define crypto_stream_xsalsa20_IMPLEMENTATION "crypto_stream/xsalsa20/ref"
#ifndef crypto_stream_xsalsa20_ref_VERSION
#define crypto_stream_xsalsa20_ref_VERSION "-"
#endif
#define crypto_stream_xsalsa20_VERSION crypto_stream_xsalsa20_ref_VERSION

#endif
