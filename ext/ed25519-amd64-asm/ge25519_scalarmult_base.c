#include "fe25519.h"
#include "sc25519.h"
#include "ge25519.h"

/* Multiples of the base point in Niels' representation */
static const ge25519_niels ge25519_base_multiples_niels[] = {
#ifdef SMALLTABLES
#include "ge25519_base_niels_smalltables.data"
#else
#include "ge25519_base_niels.data"
#endif
};

/* d */
/*static const fe25519 ecd = {{0x75EB4DCA135978A3, 0x00700A4D4141D8AB, 0x8CC740797779E898, 0x52036CEE2B6FFE73}};*/

void ge25519_scalarmult_base(ge25519_p3 *r, const sc25519 *s)
{
  signed char b[64];
  int i;
  ge25519_niels t;
  fe25519 d;

  sc25519_window4(b,s);

#ifdef SMALLTABLES
  ge25519_p1p1 tp1p1;
  choose_t((ge25519_niels *)r, 0, (signed long long) b[1], ge25519_base_multiples_niels);
  fe25519_sub(&d, &r->y, &r->x);
  fe25519_add(&r->y, &r->y, &r->x);
  r->x = d;
  r->t = r->z;
  fe25519_setint(&r->z,2);
  for(i=3;i<64;i+=2)
  {
    choose_t(&t, (unsigned long long) i/2, (signed long long) b[i], ge25519_base_multiples_niels);
    ge25519_nielsadd2(r, &t);
  }
  ge25519_dbl_p1p1(&tp1p1,(ge25519_p2 *)r);
  ge25519_p1p1_to_p2((ge25519_p2 *)r, &tp1p1);
  ge25519_dbl_p1p1(&tp1p1,(ge25519_p2 *)r);
  ge25519_p1p1_to_p2((ge25519_p2 *)r, &tp1p1);
  ge25519_dbl_p1p1(&tp1p1,(ge25519_p2 *)r);
  ge25519_p1p1_to_p2((ge25519_p2 *)r, &tp1p1);
  ge25519_dbl_p1p1(&tp1p1,(ge25519_p2 *)r);
  ge25519_p1p1_to_p3(r, &tp1p1);
  choose_t(&t, (unsigned long long) 0, (signed long long) b[0], ge25519_base_multiples_niels);
  fe25519_mul(&t.t2d, &t.t2d, &ecd);
  ge25519_nielsadd2(r, &t);
  for(i=2;i<64;i+=2)
  {
    choose_t(&t, (unsigned long long) i/2, (signed long long) b[i], ge25519_base_multiples_niels);
    ge25519_nielsadd2(r, &t);
  }
#else 
  choose_t((ge25519_niels *)r, 0, (signed long long) b[0], ge25519_base_multiples_niels);
  fe25519_sub(&d, &r->y, &r->x);
  fe25519_add(&r->y, &r->y, &r->x);
  r->x = d;
  r->t = r->z;
  fe25519_setint(&r->z,2);
  for(i=1;i<64;i++)
  {
    choose_t(&t, (unsigned long long) i, (signed long long) b[i], ge25519_base_multiples_niels);
    ge25519_nielsadd2(r, &t);
  }
#endif 
}
