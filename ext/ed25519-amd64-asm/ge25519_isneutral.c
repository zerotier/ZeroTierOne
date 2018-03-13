#include "fe25519.h"
#include "ge25519.h"

int ge25519_isneutral_vartime(const ge25519_p3 *p)
{
  if(!fe25519_iszero_vartime(&p->x)) return 0;
  if(!fe25519_iseq_vartime(&p->y, &p->z)) return 0;
  return 1;
}
