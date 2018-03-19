#include "fe25519.h"

unsigned char fe25519_getparity(const fe25519 *x)
{
  fe25519 t = *x;
  fe25519_freeze(&t);
  return (unsigned char)t.v[0] & 1;
}
