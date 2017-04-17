#ifndef crypto_hash_sha256_H
#define crypto_hash_sha256_H

#define crypto_hash_sha256_ref_BYTES 32
#ifdef __cplusplus
#include <string>
extern std::string crypto_hash_sha256_ref(const std::string &);
extern "C" {
#endif
extern int crypto_hash_sha256_ref(unsigned char *,const unsigned char *,unsigned long long);
#ifdef __cplusplus
}
#endif
#define crypto_hash_sha256 crypto_hash_sha256_ref
#define crypto_hash_sha256_BYTES crypto_hash_sha256_ref_BYTES
#define crypto_hash_sha256_IMPLEMENTATION "crypto_hash/sha256/ref"
#ifndef crypto_hash_sha256_ref_VERSION
#define crypto_hash_sha256_ref_VERSION "-"
#endif
#define crypto_hash_sha256_VERSION crypto_hash_sha256_ref_VERSION

#endif
