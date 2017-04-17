#ifndef crypto_onetimeauth_poly1305_H
#define crypto_onetimeauth_poly1305_H

#define crypto_onetimeauth_poly1305_53_BYTES 16
#define crypto_onetimeauth_poly1305_53_KEYBYTES 32
#ifdef __cplusplus
#include <string>
extern std::string crypto_onetimeauth_poly1305_53(const std::string &,const std::string &);
extern void crypto_onetimeauth_poly1305_53_verify(const std::string &,const std::string &,const std::string &);
extern "C" {
#endif
extern int crypto_onetimeauth_poly1305_53(unsigned char *,const unsigned char *,unsigned long long,const unsigned char *);
extern int crypto_onetimeauth_poly1305_53_verify(const unsigned char *,const unsigned char *,unsigned long long,const unsigned char *);
#ifdef __cplusplus
}
#endif
#define crypto_onetimeauth_poly1305 crypto_onetimeauth_poly1305_53
#define crypto_onetimeauth_poly1305_verify crypto_onetimeauth_poly1305_53_verify
#define crypto_onetimeauth_poly1305_BYTES crypto_onetimeauth_poly1305_53_BYTES
#define crypto_onetimeauth_poly1305_KEYBYTES crypto_onetimeauth_poly1305_53_KEYBYTES
#define crypto_onetimeauth_poly1305_IMPLEMENTATION "crypto_onetimeauth/poly1305/53"
#ifndef crypto_onetimeauth_poly1305_53_VERSION
#define crypto_onetimeauth_poly1305_53_VERSION "-"
#endif
#define crypto_onetimeauth_poly1305_VERSION crypto_onetimeauth_poly1305_53_VERSION

#endif
