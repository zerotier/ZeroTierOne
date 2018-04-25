#include "fe25519.h"

int fe25519_iszero_vartime(const fe25519 *x)
{
  fe25519 t = *x;
  fe25519_freeze(&t);
  if (t.v[0]) return 0;
  if (t.v[1]) return 0;
  if (t.v[2]) return 0;
  if (t.v[3]) return 0;
  return 1;
}
