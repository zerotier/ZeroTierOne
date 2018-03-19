#include "sc25519.h"

void sc25519_from_shortsc(sc25519 *r, const shortsc25519 *x)
{
  r->v[0] = x->v[0];
  r->v[1] = x->v[1];
  r->v[2] = 0;
  r->v[3] = 0;
}
