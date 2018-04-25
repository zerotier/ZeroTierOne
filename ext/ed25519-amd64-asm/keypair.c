#include <string.h>
#include "crypto_sign.h"
#include "crypto_hash_sha512.h"
#include "randombytes.h"
#include "ge25519.h"

int crypto_sign_keypair(unsigned char *pk,unsigned char *sk)
{
  unsigned char az[64];
  sc25519 scsk;
  ge25519 gepk;

  randombytes(sk,32);
  crypto_hash_sha512(az,sk,32);
  az[0] &= 248;
  az[31] &= 127;
  az[31] |= 64;

  sc25519_from32bytes(&scsk,az);
  
  ge25519_scalarmult_base(&gepk, &scsk);
  ge25519_pack(pk, &gepk);
  memmove(sk + 32,pk,32);
  return 0;
}
