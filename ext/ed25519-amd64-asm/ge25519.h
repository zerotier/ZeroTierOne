#ifndef GE25519_H
#define GE25519_H

#include "fe25519.h"
#include "sc25519.h"

#define ge25519                           crypto_sign_ed25519_amd64_64_ge25519
#define ge25519_base                      crypto_sign_ed25519_amd64_64_ge25519_base
#define ge25519_unpackneg_vartime         crypto_sign_ed25519_amd64_64_unpackneg_vartime
#define ge25519_pack                      crypto_sign_ed25519_amd64_64_pack
#define ge25519_isneutral_vartime         crypto_sign_ed25519_amd64_64_isneutral_vartime
#define ge25519_add                       crypto_sign_ed25519_amd64_64_ge25519_add
#define ge25519_double                    crypto_sign_ed25519_amd64_64_ge25519_double
#define ge25519_double_scalarmult_vartime crypto_sign_ed25519_amd64_64_double_scalarmult_vartime
#define ge25519_multi_scalarmult_vartime  crypto_sign_ed25519_amd64_64_ge25519_multi_scalarmult_vartime
#define ge25519_scalarmult_base           crypto_sign_ed25519_amd64_64_scalarmult_base
#define ge25519_p1p1_to_p2                crypto_sign_ed25519_amd64_64_ge25519_p1p1_to_p2
#define ge25519_p1p1_to_p3                crypto_sign_ed25519_amd64_64_ge25519_p1p1_to_p3
#define ge25519_add_p1p1                  crypto_sign_ed25519_amd64_64_ge25519_add_p1p1
#define ge25519_dbl_p1p1                  crypto_sign_ed25519_amd64_64_ge25519_dbl_p1p1
#define choose_t                          crypto_sign_ed25519_amd64_64_choose_t
#define ge25519_nielsadd2                 crypto_sign_ed25519_amd64_64_ge25519_nielsadd2
#define ge25519_nielsadd_p1p1             crypto_sign_ed25519_amd64_64_ge25519_nielsadd_p1p1
#define ge25519_pnielsadd_p1p1            crypto_sign_ed25519_amd64_64_ge25519_pnielsadd_p1p1


#define ge25519_p3 ge25519

typedef struct
{
  fe25519 x;
  fe25519 y;
  fe25519 z;
  fe25519 t;
} ge25519;

typedef struct
{
  fe25519 x;
  fe25519 z;
  fe25519 y;
  fe25519 t;
} ge25519_p1p1;

typedef struct
{
  fe25519 x;
  fe25519 y;
  fe25519 z;
} ge25519_p2;

typedef struct
{
  fe25519 ysubx;
  fe25519 xaddy;
  fe25519 t2d;
} ge25519_niels;

typedef struct
{
  fe25519 ysubx;
  fe25519 xaddy;
  fe25519 z;
  fe25519 t2d;
} ge25519_pniels;

extern void ge25519_p1p1_to_p2(ge25519_p2 *r, const ge25519_p1p1 *p);
extern void ge25519_p1p1_to_p3(ge25519_p3 *r, const ge25519_p1p1 *p);
extern void ge25519_add_p1p1(ge25519_p1p1 *r, const ge25519_p3 *p, const ge25519_p3 *q);
extern void ge25519_dbl_p1p1(ge25519_p1p1 *r, const ge25519_p2 *p);
extern void choose_t(ge25519_niels *t, unsigned long long pos, signed long long b, const ge25519_niels *base_multiples);
extern void ge25519_nielsadd2(ge25519_p3 *r, const ge25519_niels *q);
extern void ge25519_nielsadd_p1p1(ge25519_p1p1 *r, const ge25519_p3 *p, const ge25519_niels *q);
extern void ge25519_pnielsadd_p1p1(ge25519_p1p1 *r, const ge25519_p3 *p, const ge25519_pniels *q);

extern const ge25519 ge25519_base;

extern int ge25519_unpackneg_vartime(ge25519 *r, const unsigned char p[32]);

extern void ge25519_pack(unsigned char r[32], const ge25519 *p);

extern int ge25519_isneutral_vartime(const ge25519 *p);

extern void ge25519_add(ge25519 *r, const ge25519 *p, const ge25519 *q);

extern void ge25519_double(ge25519 *r, const ge25519 *p);

/* computes [s1]p1 + [s2]ge25519_base */
extern void ge25519_double_scalarmult_vartime(ge25519 *r, const ge25519 *p1, const sc25519 *s1, const sc25519 *s2);

extern void ge25519_multi_scalarmult_vartime(ge25519 *r, ge25519 *p, sc25519 *s, const unsigned long long npoints);

extern void ge25519_scalarmult_base(ge25519 *r, const sc25519 *s);

#endif
