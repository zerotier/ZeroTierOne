#include "fe25519.h"

void fe25519_setint(fe25519 *r, unsigned int v)
{
  r->v[0] = v;
  r->v[1] = 0;
  r->v[2] = 0;
  r->v[3] = 0;
}
