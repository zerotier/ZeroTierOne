#include "sc25519.h"

/*Arithmetic modulo the group order n = 2^252 + 27742317777372353535851937790883648493 
 *                                    = 7237005577332262213973186563042994240857116359379907606001950938285454250989
 */

/* Contains order, 2*order, 4*order, 8*order, each represented in 4 consecutive unsigned long long */
static const unsigned long long order[16] = {0x5812631A5CF5D3EDULL, 0x14DEF9DEA2F79CD6ULL, 
                                             0x0000000000000000ULL, 0x1000000000000000ULL,
                                             0xB024C634B9EBA7DAULL, 0x29BDF3BD45EF39ACULL,
                                             0x0000000000000000ULL, 0x2000000000000000ULL,
                                             0x60498C6973D74FB4ULL, 0x537BE77A8BDE7359ULL,
                                             0x0000000000000000ULL, 0x4000000000000000ULL,
                                             0xC09318D2E7AE9F68ULL, 0xA6F7CEF517BCE6B2ULL,
                                             0x0000000000000000ULL, 0x8000000000000000ULL};

static unsigned long long smaller(unsigned long long a,unsigned long long b)
{
  unsigned long long atop = a >> 32;
  unsigned long long abot = a & 4294967295;
  unsigned long long btop = b >> 32;
  unsigned long long bbot = b & 4294967295;
  unsigned long long atopbelowbtop = (atop - btop) >> 63;
  unsigned long long atopeqbtop = ((atop ^ btop) - 1) >> 63;
  unsigned long long abotbelowbbot = (abot - bbot) >> 63;
  return atopbelowbtop | (atopeqbtop & abotbelowbbot);
}

void sc25519_from32bytes(sc25519 *r, const unsigned char x[32])
{
  unsigned long long t[4];
  unsigned long long b;
  unsigned long long mask;
  int i, j;

  /* assuming little-endian */
  r->v[0] = *(unsigned long long *)x;
  r->v[1] = *(((unsigned long long *)x)+1);
  r->v[2] = *(((unsigned long long *)x)+2);
  r->v[3] = *(((unsigned long long *)x)+3);
  
  for(j=3;j>=0;j--)
  {
    b=0;
    for(i=0;i<4;i++) 
    {
      b += order[4*j+i]; /* no overflow for this particular order */
      t[i] = r->v[i] - b;
      b = smaller(r->v[i],b);
    }
    mask = b - 1;
    for(i=0;i<4;i++) 
      r->v[i] ^= mask & (r->v[i] ^ t[i]);
  }
}
