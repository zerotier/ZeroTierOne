#include "sc25519.h"

void sc25519_mul_shortsc(sc25519 *r, const sc25519 *x, const shortsc25519 *y)
{
  /* XXX: This wants to be faster */
  sc25519 t;
  sc25519_from_shortsc(&t, y);
  sc25519_mul(r, x, &t);
}
