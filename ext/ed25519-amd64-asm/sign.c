#include <stdlib.h>
#include <string.h>
/*#include "crypto_sign.h"
#include "crypto_hash_sha512.h"*/
#include "ge25519.h"

/* Original */
#if 0
int crypto_sign(
    unsigned char *sm,unsigned long long *smlen,
    const unsigned char *m,unsigned long long mlen,
    const unsigned char *sk
    )
{
  unsigned char pk[32];
  unsigned char az[64];
  unsigned char nonce[64];
  unsigned char hram[64];
  sc25519 sck, scs, scsk;
  ge25519 ger;

  memmove(pk,sk + 32,32);
  /* pk: 32-byte public key A */

  crypto_hash_sha512(az,sk,32);
  az[0] &= 248;
  az[31] &= 127;
  az[31] |= 64;
  /* az: 32-byte scalar a, 32-byte randomizer z */

  *smlen = mlen + 64;
  memmove(sm + 64,m,mlen);
  memmove(sm + 32,az + 32,32);
  /* sm: 32-byte uninit, 32-byte z, mlen-byte m */

  crypto_hash_sha512(nonce, sm+32, mlen+32);
  /* nonce: 64-byte H(z,m) */

  sc25519_from64bytes(&sck, nonce);
  ge25519_scalarmult_base(&ger, &sck);
  ge25519_pack(sm, &ger);
  /* sm: 32-byte R, 32-byte z, mlen-byte m */

  memmove(sm + 32,pk,32);
  /* sm: 32-byte R, 32-byte A, mlen-byte m */

  crypto_hash_sha512(hram,sm,mlen + 64);
  /* hram: 64-byte H(R,A,m) */

  sc25519_from64bytes(&scs, hram);
  sc25519_from32bytes(&scsk, az);
  sc25519_mul(&scs, &scs, &scsk);
  sc25519_add(&scs, &scs, &sck);
  /* scs: S = nonce + H(R,A,m)a */

  sc25519_to32bytes(sm + 32,&scs);
  /* sm: 32-byte R, 32-byte S, mlen-byte m */

  return 0;
}
#endif

#if 0
void C25519::sign(const C25519::Private &myPrivate,const C25519::Public &myPublic,const void *msg,unsigned int len,void *signature)
{
  sc25519 sck, scs, scsk;
  ge25519 ger;
  unsigned char r[32];
  unsigned char s[32];
  unsigned char extsk[64];
  unsigned char hmg[crypto_hash_sha512_BYTES];
  unsigned char hram[crypto_hash_sha512_BYTES];
  unsigned char *sig = (unsigned char *)signature;
  unsigned char digest[64]; // we sign the first 32 bytes of SHA-512(msg)

  SHA512::hash(digest,msg,len);

  SHA512::hash(extsk,myPrivate.data + 32,32);
  extsk[0] &= 248;
  extsk[31] &= 127;
  extsk[31] |= 64;

  for(unsigned int i=0;i<32;i++)
    sig[32 + i] = extsk[32 + i];
  for(unsigned int i=0;i<32;i++)
    sig[64 + i] = digest[i];

  SHA512::hash(hmg,sig + 32,64);

  /* Computation of R */
  sc25519_from64bytes(&sck, hmg);
  ge25519_scalarmult_base(&ger, &sck);
  ge25519_pack(r, &ger);

  /* Computation of s */
  for(unsigned int i=0;i<32;i++)
    sig[i] = r[i];

  get_hram(hram,sig,myPublic.data + 32,sig,96);

  sc25519_from64bytes(&scs, hram);
  sc25519_from32bytes(&scsk, extsk);
  sc25519_mul(&scs, &scs, &scsk);

  sc25519_add(&scs, &scs, &sck);

  sc25519_to32bytes(s,&scs); /* cat s */
  for(unsigned int i=0;i<32;i++)
    sig[32 + i] = s[i];
}

void get_hram(unsigned char *hram, const unsigned char *sm, const unsigned char *pk, unsigned char *playground, unsigned long long smlen)
{
  unsigned long long i;

  for (i =  0;i < 32;++i)    playground[i] = sm[i];
  for (i = 32;i < 64;++i)    playground[i] = pk[i-32];
  for (i = 64;i < smlen;++i) playground[i] = sm[i];

  //crypto_hash_sha512(hram,playground,smlen);
  ZeroTier::SHA512::hash(hram,playground,(unsigned int)smlen);
}
#endif

extern void ZT_sha512internal(void *digest,const void *data,unsigned int len);

extern void ed25519_amd64_asm_sign(const unsigned char *sk,const unsigned char *pk,const unsigned char *digest,unsigned char *sig)
{
  unsigned char az[64];
  unsigned char nonce[64];
  unsigned char hram[64];
  sc25519 sck, scs, scsk;
  ge25519 ger;
  unsigned int i;

  ZT_sha512internal(az,sk,32);
  az[0] &= 248;
  az[31] &= 127;
  az[31] |= 64;

  for(i=0;i<32;i++)
    sig[32 + i] = az[32 + i];
  for(i=0;i<32;i++)
    sig[64 + i] = digest[i];

  ZT_sha512internal(nonce,sig + 32,64);

  sc25519_from64bytes(&sck, nonce);
  ge25519_scalarmult_base(&ger, &sck);
  ge25519_pack(sig, &ger);

  memmove(sig + 32,pk,32);

  ZT_sha512internal(hram,sig,96);

  sc25519_from64bytes(&scs, hram);
  sc25519_from32bytes(&scsk, az);
  sc25519_mul(&scs, &scs, &scsk);
  sc25519_add(&scs, &scs, &sck);

  sc25519_to32bytes(sig + 32,&scs);
}
