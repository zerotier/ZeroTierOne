#include "ge25519.h"

void ge25519_double(ge25519_p3 *r, const ge25519_p3 *p)
{
  ge25519_p1p1 grp1p1;
  ge25519_dbl_p1p1(&grp1p1, (ge25519_p2 *)p);
  ge25519_p1p1_to_p3(r, &grp1p1);
}
