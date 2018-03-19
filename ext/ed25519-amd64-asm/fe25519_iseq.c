#include "fe25519.h"

int fe25519_iseq_vartime(const fe25519 *x, const fe25519 *y)
{
  fe25519 t1 = *x;
  fe25519 t2 = *y;
  fe25519_freeze(&t1);
  fe25519_freeze(&t2);
  if(t1.v[0] != t2.v[0]) return 0;
  if(t1.v[1] != t2.v[1]) return 0;
  if(t1.v[2] != t2.v[2]) return 0;
  if(t1.v[3] != t2.v[3]) return 0;
  return 1;
}
