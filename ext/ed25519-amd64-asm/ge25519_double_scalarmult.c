#include "fe25519.h"
#include "sc25519.h"
#include "ge25519.h"

#define S1_SWINDOWSIZE 5
#define PRE1_SIZE (1<<(S1_SWINDOWSIZE-2))
#define S2_SWINDOWSIZE 7
#define PRE2_SIZE (1<<(S2_SWINDOWSIZE-2))

ge25519_niels pre2[PRE2_SIZE] = {
#include "ge25519_base_slide_multiples.data"
};

static const fe25519 ec2d = {{0xEBD69B9426B2F146, 0x00E0149A8283B156, 0x198E80F2EEF3D130, 0xA406D9DC56DFFCE7}};

static void setneutral(ge25519 *r)
{
  fe25519_setint(&r->x,0);
  fe25519_setint(&r->y,1);
  fe25519_setint(&r->z,1);
  fe25519_setint(&r->t,0);
}

/* computes [s1]p1 + [s2]p2 */
void ge25519_double_scalarmult_vartime(ge25519_p3 *r, const ge25519_p3 *p1, const sc25519 *s1, const sc25519 *s2)
{
  signed char slide1[256], slide2[256];
  ge25519_pniels pre1[PRE1_SIZE], neg;
  ge25519_p3 d1;
  ge25519_p1p1 t;
  ge25519_niels nneg;
  fe25519 d;
  int i;
  
  sc25519_slide(slide1, s1, S1_SWINDOWSIZE);
  sc25519_slide(slide2, s2, S2_SWINDOWSIZE);

  /* precomputation */
  pre1[0] = *(ge25519_pniels *)p1;                                                                         
  ge25519_dbl_p1p1(&t,(ge25519_p2 *)pre1);      ge25519_p1p1_to_p3(&d1, &t);
  /* Convert pre[0] to projective Niels representation */
  d = pre1[0].ysubx;
  fe25519_sub(&pre1[0].ysubx, &pre1[0].xaddy, &pre1[0].ysubx);
  fe25519_add(&pre1[0].xaddy, &pre1[0].xaddy, &d);
  fe25519_mul(&pre1[0].t2d, &pre1[0].t2d, &ec2d);

  for(i=0;i<PRE1_SIZE-1;i++)
  {
    ge25519_pnielsadd_p1p1(&t, &d1, &pre1[i]);  ge25519_p1p1_to_p3((ge25519_p3 *)&pre1[i+1], &t);
    /* Convert pre1[i+1] to projective Niels representation */
    d = pre1[i+1].ysubx;
    fe25519_sub(&pre1[i+1].ysubx, &pre1[i+1].xaddy, &pre1[i+1].ysubx);
    fe25519_add(&pre1[i+1].xaddy, &pre1[i+1].xaddy, &d);
    fe25519_mul(&pre1[i+1].t2d, &pre1[i+1].t2d, &ec2d);
  }

  setneutral(r);
  for (i = 255;i >= 0;--i) {
    if (slide1[i] || slide2[i]) goto firstbit;
  }

  for(;i>=0;i--)
  {
    firstbit:

    ge25519_dbl_p1p1(&t, (ge25519_p2 *)r);

    if(slide1[i]>0)
    {
      ge25519_p1p1_to_p3(r, &t);
      ge25519_pnielsadd_p1p1(&t, r, &pre1[slide1[i]/2]);
    }
    else if(slide1[i]<0)
    {
      ge25519_p1p1_to_p3(r, &t);
      neg = pre1[-slide1[i]/2];
      d = neg.ysubx;
      neg.ysubx = neg.xaddy;
      neg.xaddy = d;
      fe25519_neg(&neg.t2d, &neg.t2d);
      ge25519_pnielsadd_p1p1(&t, r, &neg);
    }

    if(slide2[i]>0)
    {
      ge25519_p1p1_to_p3(r, &t);
      ge25519_nielsadd_p1p1(&t, r, &pre2[slide2[i]/2]);
    }
    else if(slide2[i]<0)
    {
      ge25519_p1p1_to_p3(r, &t);
      nneg = pre2[-slide2[i]/2];
      d = nneg.ysubx;
      nneg.ysubx = nneg.xaddy;
      nneg.xaddy = d;
      fe25519_neg(&nneg.t2d, &nneg.t2d);
      ge25519_nielsadd_p1p1(&t, r, &nneg);
    }

    ge25519_p1p1_to_p2((ge25519_p2 *)r, &t);
  }
}
