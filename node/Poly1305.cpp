/*
20080912
D. J. Bernstein
Public domain.
*/

#include "Constants.hpp"
#include "Poly1305.hpp"

#ifdef __WINDOWS__
#pragma warning(disable: 4146)
#endif

namespace ZeroTier {

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

static void add(unsigned int h[17],const unsigned int c[17])
{
  unsigned int j;
  unsigned int u;
  u = 0;
  for (j = 0;j < 17;++j) { u += h[j] + c[j]; h[j] = u & 255; u >>= 8; }
}

static void squeeze(unsigned int h[17])
{
  unsigned int j;
  unsigned int u;
  u = 0;
  for (j = 0;j < 16;++j) { u += h[j]; h[j] = u & 255; u >>= 8; }
  u += h[16]; h[16] = u & 3;
  u = 5 * (u >> 2);
  for (j = 0;j < 16;++j) { u += h[j]; h[j] = u & 255; u >>= 8; }
  u += h[16]; h[16] = u;
}

static const unsigned int minusp[17] = {
  5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 252
} ;

static void freeze(unsigned int h[17])
{
  unsigned int horig[17];
  unsigned int j;
  unsigned int negative;
  for (j = 0;j < 17;++j) horig[j] = h[j];
  add(h,minusp);
  negative = -(h[16] >> 7);
  for (j = 0;j < 17;++j) h[j] ^= negative & (horig[j] ^ h[j]);
}

static void mulmod(unsigned int h[17],const unsigned int r[17])
{
  unsigned int hr[17];
  unsigned int i;
  unsigned int j;
  unsigned int u;

  for (i = 0;i < 17;++i) {
    u = 0;
    for (j = 0;j <= i;++j) u += h[j] * r[i - j];
    for (j = i + 1;j < 17;++j) u += 320 * h[j] * r[i + 17 - j];
    hr[i] = u;
  }
  for (i = 0;i < 17;++i) h[i] = hr[i];
  squeeze(h);
}

static inline int crypto_onetimeauth(unsigned char *out,const unsigned char *in,unsigned long long inlen,const unsigned char *k)
{
  unsigned int j;
  unsigned int r[17];
  unsigned int h[17];
  unsigned int c[17];

  r[0] = k[0];
  r[1] = k[1];
  r[2] = k[2];
  r[3] = k[3] & 15;
  r[4] = k[4] & 252;
  r[5] = k[5];
  r[6] = k[6];
  r[7] = k[7] & 15;
  r[8] = k[8] & 252;
  r[9] = k[9];
  r[10] = k[10];
  r[11] = k[11] & 15;
  r[12] = k[12] & 252;
  r[13] = k[13];
  r[14] = k[14];
  r[15] = k[15] & 15;
  r[16] = 0;

  for (j = 0;j < 17;++j) h[j] = 0;

  while (inlen > 0) {
    for (j = 0;j < 17;++j) c[j] = 0;
    for (j = 0;(j < 16) && (j < inlen);++j) c[j] = in[j];
    c[j] = 1;
    in += j; inlen -= j;
    add(h,c);
    mulmod(h,r);
  }

  freeze(h);

  for (j = 0;j < 16;++j) c[j] = k[j + 16];
  c[16] = 0;
  add(h,c);
  for (j = 0;j < 16;++j) out[j] = h[j];
  return 0;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

void Poly1305::compute(void *auth,const void *data,unsigned int len,const void *key)
	throw()
{
	crypto_onetimeauth((unsigned char *)auth,(const unsigned char *)data,len,(const unsigned char *)key);
}

} // namespace ZeroTier
