#ifndef crypto_verify_16_H
#define crypto_verify_16_H

#define crypto_verify_16_ref_BYTES 16
#ifdef __cplusplus
#include <string>
extern "C" {
#endif
extern int crypto_verify_16_ref(const unsigned char *,const unsigned char *);
#ifdef __cplusplus
}
#endif
#define crypto_verify_16 crypto_verify_16_ref
#define crypto_verify_16_BYTES crypto_verify_16_ref_BYTES
#define crypto_verify_16_IMPLEMENTATION "crypto_verify/16/ref"
#ifndef crypto_verify_16_ref_VERSION
#define crypto_verify_16_ref_VERSION "-"
#endif
#define crypto_verify_16_VERSION crypto_verify_16_ref_VERSION

#endif
