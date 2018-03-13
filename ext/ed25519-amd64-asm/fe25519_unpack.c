#include "fe25519.h"

void fe25519_unpack(fe25519 *r, const unsigned char x[32])
{
  /* assuming little-endian */
  r->v[0] = *(unsigned long long *)x;
  r->v[1] = *(((unsigned long long *)x)+1);
  r->v[2] = *(((unsigned long long *)x)+2);
  r->v[3] = *(((unsigned long long *)x)+3);
  r->v[3] &= 0x7fffffffffffffffULL;
}
