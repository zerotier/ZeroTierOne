#include "fe25519.h"
#include "sc25519.h"
#include "ge25519.h"
#include "index_heap.h"

static void setneutral(ge25519 *r)
{
  fe25519_setint(&r->x,0);
  fe25519_setint(&r->y,1);
  fe25519_setint(&r->z,1);
  fe25519_setint(&r->t,0);
}

static void ge25519_scalarmult_vartime_2limbs(ge25519 *r, ge25519 *p, sc25519 *s)
{
  if (s->v[1] == 0 && s->v[0] == 1) /* This will happen most of the time after Bos-Coster */
    *r = *p; 
  else if (s->v[1] == 0 && s->v[0] == 0) /* This won't ever happen, except for all scalars == 0 in Bos-Coster */
    setneutral(r);
  else
  {
    ge25519 d;
    unsigned long long mask = (1ULL << 63);
    int i = 1;
    while(!(mask & s->v[1]) && mask != 0)
      mask >>= 1;
    if(mask == 0)
    {
      mask = (1ULL << 63);
      i = 0;
      while(!(mask & s->v[0]) && mask != 0)
        mask >>= 1;
    }
    d = *p;
    mask >>= 1;
    for(;mask != 0;mask >>= 1)
    {
      ge25519_double(&d,&d);
      if(s->v[i] & mask)
        ge25519_add(&d,&d,p);
    }
    if(i==1)
    {
      mask = (1ULL << 63);
      for(;mask != 0;mask >>= 1)
      {
        ge25519_double(&d,&d);
        if(s->v[0] & mask)
          ge25519_add(&d,&d,p);
      }
    }
    *r = d;
  }
}

/* caller's responsibility to ensure npoints >= 5 */
void ge25519_multi_scalarmult_vartime(ge25519_p3 *r, ge25519_p3 *p, sc25519 *s, const unsigned long long npoints)
{
  unsigned long long pos[npoints];
  unsigned long long hlen=((npoints+1)/2)|1;
  unsigned long long max1, max2,i;

  heap_init(pos, hlen, s);
  
  for(i=0;;i++)
  {
    heap_get2max(pos, &max1, &max2, s);
    if((s[max1].v[3] == 0) || (sc25519_iszero_vartime(&s[max2]))) break;
    sc25519_sub_nored(&s[max1],&s[max1],&s[max2]);
    ge25519_add(&p[max2],&p[max2],&p[max1]);
    heap_rootreplaced(pos, hlen, s);
  }
  for(;;i++)
  {
    heap_get2max(pos, &max1, &max2, s);
    if((s[max1].v[2] == 0) || (sc25519_iszero_vartime(&s[max2]))) break;
    sc25519_sub_nored(&s[max1],&s[max1],&s[max2]);
    ge25519_add(&p[max2],&p[max2],&p[max1]);
    heap_rootreplaced_3limbs(pos, hlen, s);
  }
  /* We know that (npoints-1)/2 scalars are only 128-bit scalars */
  heap_extend(pos, hlen, npoints, s);
  hlen = npoints;
  for(;;i++)
  {
    heap_get2max(pos, &max1, &max2, s);
    if((s[max1].v[1] == 0) || (sc25519_iszero_vartime(&s[max2]))) break;
    sc25519_sub_nored(&s[max1],&s[max1],&s[max2]);
    ge25519_add(&p[max2],&p[max2],&p[max1]);
    heap_rootreplaced_2limbs(pos, hlen, s);
  }
  for(;;i++)
  {
    heap_get2max(pos, &max1, &max2, s);
    if(sc25519_iszero_vartime(&s[max2])) break;
    sc25519_sub_nored(&s[max1],&s[max1],&s[max2]);
    ge25519_add(&p[max2],&p[max2],&p[max1]);
    heap_rootreplaced_1limb(pos, hlen, s);
  }

  ge25519_scalarmult_vartime_2limbs(r, &p[max1], &s[max1]);
}
