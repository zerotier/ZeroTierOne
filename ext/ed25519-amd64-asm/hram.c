/*#include "crypto_hash_sha512.h"*/
#include "hram.h"

extern void ZT_sha512internal(void *digest,const void *data,unsigned int len);

void get_hram(unsigned char *hram, const unsigned char *sm, const unsigned char *pk, unsigned char *playground, unsigned long long smlen)
{
  unsigned long long i;

  for (i =  0;i < 32;++i)    playground[i] = sm[i];
  for (i = 32;i < 64;++i)    playground[i] = pk[i-32];
  for (i = 64;i < smlen;++i) playground[i] = sm[i];

  /*crypto_hash_sha512(hram,playground,smlen);*/
  ZT_sha512internal(hram,playground,smlen);
}
