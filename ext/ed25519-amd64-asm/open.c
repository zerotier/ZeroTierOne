#include <string.h>
#include "crypto_sign.h"
#include "crypto_verify_32.h"
#include "crypto_hash_sha512.h"
#include "ge25519.h"

int crypto_sign_open(
    unsigned char *m,unsigned long long *mlen,
    const unsigned char *sm,unsigned long long smlen,
    const unsigned char *pk
    )
{
  unsigned char pkcopy[32];
  unsigned char rcopy[32];
  unsigned char hram[64];
  unsigned char rcheck[32];
  ge25519 get1, get2;
  sc25519 schram, scs;

  if (smlen < 64) goto badsig;
  if (sm[63] & 224) goto badsig;
  if (ge25519_unpackneg_vartime(&get1,pk)) goto badsig;

  memmove(pkcopy,pk,32);
  memmove(rcopy,sm,32);

  sc25519_from32bytes(&scs, sm+32);

  memmove(m,sm,smlen);
  memmove(m + 32,pkcopy,32);
  crypto_hash_sha512(hram,m,smlen);

  sc25519_from64bytes(&schram, hram);

  ge25519_double_scalarmult_vartime(&get2, &get1, &schram, &scs);
  ge25519_pack(rcheck, &get2);

  if (crypto_verify_32(rcopy,rcheck) == 0) {
    memmove(m,m + 64,smlen - 64);
    memset(m + smlen - 64,0,64);
    *mlen = smlen - 64;
    return 0;
  }

badsig:
  *mlen = (unsigned long long) -1;
  memset(m,0,smlen);
  return -1;
}
