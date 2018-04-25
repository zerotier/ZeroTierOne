#include "sc25519.h"

void sc25519_to32bytes(unsigned char r[32], const sc25519 *x)
{
  /* assuming little-endian */
  int i;
  for(i=0;i<32;i++) r[i] = i[(unsigned char *)x->v]; 
}
