#ifndef crypto_scalarmult_curve25519_H
#define crypto_scalarmult_curve25519_H

#define crypto_scalarmult_curve25519_donna_c64_BYTES 32
#define crypto_scalarmult_curve25519_donna_c64_SCALARBYTES 32
#ifdef __cplusplus
#include <string>
extern std::string crypto_scalarmult_curve25519_donna_c64(const std::string &,const std::string &);
extern std::string crypto_scalarmult_curve25519_donna_c64_base(const std::string &);
extern "C" {
#endif
extern int crypto_scalarmult_curve25519_donna_c64(unsigned char *,const unsigned char *,const unsigned char *);
extern int crypto_scalarmult_curve25519_donna_c64_base(unsigned char *,const unsigned char *);
#ifdef __cplusplus
}
#endif
#define crypto_scalarmult_curve25519 crypto_scalarmult_curve25519_donna_c64
#define crypto_scalarmult_curve25519_base crypto_scalarmult_curve25519_donna_c64_base
#define crypto_scalarmult_curve25519_BYTES crypto_scalarmult_curve25519_donna_c64_BYTES
#define crypto_scalarmult_curve25519_SCALARBYTES crypto_scalarmult_curve25519_donna_c64_SCALARBYTES
#define crypto_scalarmult_curve25519_IMPLEMENTATION "crypto_scalarmult/curve25519/donna_c64"
#ifndef crypto_scalarmult_curve25519_donna_c64_VERSION
#define crypto_scalarmult_curve25519_donna_c64_VERSION "-"
#endif
#define crypto_scalarmult_curve25519_VERSION crypto_scalarmult_curve25519_donna_c64_VERSION

#endif
