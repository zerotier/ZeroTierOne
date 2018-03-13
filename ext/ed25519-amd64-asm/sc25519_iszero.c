#include "sc25519.h"

int sc25519_iszero_vartime(const sc25519 *x)
{
  if(x->v[0] != 0) return 0;
  if(x->v[1] != 0) return 0;
  if(x->v[2] != 0) return 0;
  if(x->v[3] != 0) return 0;
  return 1;
}
