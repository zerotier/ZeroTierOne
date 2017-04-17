#ifndef crypto_core_salsa20_H
#define crypto_core_salsa20_H

#define crypto_core_salsa20_ref_OUTPUTBYTES 64
#define crypto_core_salsa20_ref_INPUTBYTES 16
#define crypto_core_salsa20_ref_KEYBYTES 32
#define crypto_core_salsa20_ref_CONSTBYTES 16
#ifdef __cplusplus
#include <string>
extern "C" {
#endif
extern int crypto_core_salsa20_ref(unsigned char *,const unsigned char *,const unsigned char *,const unsigned char *);
#ifdef __cplusplus
}
#endif
#define crypto_core_salsa20 crypto_core_salsa20_ref
#define crypto_core_salsa20_OUTPUTBYTES crypto_core_salsa20_ref_OUTPUTBYTES
#define crypto_core_salsa20_INPUTBYTES crypto_core_salsa20_ref_INPUTBYTES
#define crypto_core_salsa20_KEYBYTES crypto_core_salsa20_ref_KEYBYTES
#define crypto_core_salsa20_CONSTBYTES crypto_core_salsa20_ref_CONSTBYTES
#define crypto_core_salsa20_IMPLEMENTATION "crypto_core/salsa20/ref"
#ifndef crypto_core_salsa20_ref_VERSION
#define crypto_core_salsa20_ref_VERSION "-"
#endif
#define crypto_core_salsa20_VERSION crypto_core_salsa20_ref_VERSION

#endif
