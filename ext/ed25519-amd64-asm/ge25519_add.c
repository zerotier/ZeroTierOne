#include "ge25519.h"

void ge25519_add(ge25519_p3 *r, const ge25519_p3 *p, const ge25519_p3 *q)
{
  ge25519_p1p1 grp1p1;
  ge25519_add_p1p1(&grp1p1, p, q);
  ge25519_p1p1_to_p3(r, &grp1p1);
}
