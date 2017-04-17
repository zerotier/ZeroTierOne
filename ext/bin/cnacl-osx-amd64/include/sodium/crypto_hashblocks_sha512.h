#ifndef crypto_hashblocks_sha512_H
#define crypto_hashblocks_sha512_H

#define crypto_hashblocks_sha512_ref_STATEBYTES 64
#define crypto_hashblocks_sha512_ref_BLOCKBYTES 128
#ifdef __cplusplus
#include <string>
extern "C" {
#endif
extern int crypto_hashblocks_sha512_ref(unsigned char *,const unsigned char *,unsigned long long);
#ifdef __cplusplus
}
#endif
#define crypto_hashblocks_sha512 crypto_hashblocks_sha512_ref
#define crypto_hashblocks_sha512_STATEBYTES crypto_hashblocks_sha512_ref_STATEBYTES
#define crypto_hashblocks_sha512_BLOCKBYTES crypto_hashblocks_sha512_ref_BLOCKBYTES
#define crypto_hashblocks_sha512_IMPLEMENTATION "crypto_hashblocks/sha512/ref"
#ifndef crypto_hashblocks_sha512_ref_VERSION
#define crypto_hashblocks_sha512_ref_VERSION "-"
#endif
#define crypto_hashblocks_sha512_VERSION crypto_hashblocks_sha512_ref_VERSION

#endif
