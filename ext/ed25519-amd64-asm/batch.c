#include "crypto_sign.h"

#include "crypto_verify_32.h"
#include "crypto_hash_sha512.h"
#include "randombytes.h"

#include "ge25519.h"
#include "hram.h"

#define MAXBATCH 64

int crypto_sign_open_batch(
    unsigned char* const m[],unsigned long long mlen[],
    unsigned char* const sm[],const unsigned long long smlen[],
    unsigned char* const pk[], 
    unsigned long long num
    )
{
  int ret = 0;
  unsigned long long i, j;
  shortsc25519 r[MAXBATCH];
  sc25519 scalars[2*MAXBATCH+1];
  ge25519 points[2*MAXBATCH+1];
  unsigned char hram[crypto_hash_sha512_BYTES];
  unsigned long long batchsize;

  for (i = 0;i < num;++i) mlen[i] = -1;

  while (num >= 3) {
    batchsize = num;
    if (batchsize > MAXBATCH) batchsize = MAXBATCH;

    for (i = 0;i < batchsize;++i)
      if (smlen[i] < 64) goto fallback;

    randombytes((unsigned char*)r,sizeof(shortsc25519) * batchsize);

    /* Computing scalars[0] = ((r1s1 + r2s2 + ...)) */
    for(i=0;i<batchsize;i++)
    {
      sc25519_from32bytes(&scalars[i], sm[i]+32); 
      sc25519_mul_shortsc(&scalars[i], &scalars[i], &r[i]);
    }
    for(i=1;i<batchsize;i++)
      sc25519_add(&scalars[0], &scalars[0], &scalars[i]);
    
    /* Computing scalars[1] ... scalars[batchsize] as r[i]*H(R[i],A[i],m[i]) */
    for(i=0;i<batchsize;i++)
    {
      get_hram(hram, sm[i], pk[i], m[i], smlen[i]);
      sc25519_from64bytes(&scalars[i+1],hram);
      sc25519_mul_shortsc(&scalars[i+1],&scalars[i+1],&r[i]);
    }
    /* Setting scalars[batchsize+1] ... scalars[2*batchsize] to r[i] */
    for(i=0;i<batchsize;i++)
      sc25519_from_shortsc(&scalars[batchsize+i+1],&r[i]);
  
    /* Computing points */
    points[0] = ge25519_base;
  
    for(i=0;i<batchsize;i++)
      if (ge25519_unpackneg_vartime(&points[i+1], pk[i])) goto fallback;
    for(i=0;i<batchsize;i++)
      if (ge25519_unpackneg_vartime(&points[batchsize+i+1], sm[i])) goto fallback;
  
    ge25519_multi_scalarmult_vartime(points, points, scalars, 2*batchsize+1);
  
    if (ge25519_isneutral_vartime(points)) {
      for(i=0;i<batchsize;i++)
      {
        for(j=0;j<smlen[i]-64;j++)
          m[i][j] = sm[i][j + 64];
        mlen[i] = smlen[i]-64;
      }
    } else {
      fallback:

      for (i = 0;i < batchsize;++i)
        ret |= crypto_sign_open(m[i], &mlen[i], sm[i], smlen[i], pk[i]);
    }

    m += batchsize;
    mlen += batchsize;
    sm += batchsize;
    smlen += batchsize;
    pk += batchsize;
    num -= batchsize;
  }

  for (i = 0;i < num;++i)
    ret |= crypto_sign_open(m[i], &mlen[i], sm[i], smlen[i], pk[i]);

  return ret;
}
