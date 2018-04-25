#include "fe25519.h"

/* Assumes input x being reduced below 2^255 */
void fe25519_pack(unsigned char r[32], const fe25519 *x)
{
  int i;
  fe25519 t;
  t = *x;
  fe25519_freeze(&t);
  /* assuming little-endian */
  for(i=0;i<32;i++) r[i] = i[(unsigned char *)&t.v]; 
}

