#include "sc25519.h"

void sc25519_from64bytes(sc25519 *r, const unsigned char x[64])
{
  /* assuming little-endian representation of unsigned long long */
  sc25519_barrett(r, (unsigned long long *)x); 
}
