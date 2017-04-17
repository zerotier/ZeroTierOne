#ifndef crypto_core_salsa2012_H
#define crypto_core_salsa2012_H

#define crypto_core_salsa2012_ref_OUTPUTBYTES 64
#define crypto_core_salsa2012_ref_INPUTBYTES 16
#define crypto_core_salsa2012_ref_KEYBYTES 32
#define crypto_core_salsa2012_ref_CONSTBYTES 16
#ifdef __cplusplus
#include <string>
extern "C" {
#endif
extern int crypto_core_salsa2012_ref(unsigned char *,const unsigned char *,const unsigned char *,const unsigned char *);
#ifdef __cplusplus
}
#endif
#define crypto_core_salsa2012 crypto_core_salsa2012_ref
#define crypto_core_salsa2012_OUTPUTBYTES crypto_core_salsa2012_ref_OUTPUTBYTES
#define crypto_core_salsa2012_INPUTBYTES crypto_core_salsa2012_ref_INPUTBYTES
#define crypto_core_salsa2012_KEYBYTES crypto_core_salsa2012_ref_KEYBYTES
#define crypto_core_salsa2012_CONSTBYTES crypto_core_salsa2012_ref_CONSTBYTES
#define crypto_core_salsa2012_IMPLEMENTATION "crypto_core/salsa2012/ref"
#ifndef crypto_core_salsa2012_ref_VERSION
#define crypto_core_salsa2012_ref_VERSION "-"
#endif
#define crypto_core_salsa2012_VERSION crypto_core_salsa2012_ref_VERSION

#endif
