#include "sc25519.h"

void sc25519_window4(signed char r[64], const sc25519 *s)
{
  char carry;
  int i;
  for(i=0;i<16;i++)
    r[i] = (s->v[0] >> (4*i)) & 15;
  for(i=0;i<16;i++)
    r[i+16] = (s->v[1] >> (4*i)) & 15;
  for(i=0;i<16;i++)
    r[i+32] = (s->v[2] >> (4*i)) & 15;
  for(i=0;i<16;i++)
    r[i+48] = (s->v[3] >> (4*i)) & 15;

  /* Making it signed */
  carry = 0;
  for(i=0;i<63;i++)
  {
    r[i] += carry;
    r[i+1] += r[i] >> 4;
    r[i] &= 15;
    carry = r[i] >> 3;
    r[i] -= carry << 4;
  }
  r[63] += carry;
}
