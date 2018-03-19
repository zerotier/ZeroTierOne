#ifndef SC25519_H
#define SC25519_H

#define sc25519                  crypto_sign_ed25519_amd64_64_sc25519
#define shortsc25519             crypto_sign_ed25519_amd64_64_shortsc25519
#define sc25519_from32bytes      crypto_sign_ed25519_amd64_64_sc25519_from32bytes
#define shortsc25519_from16bytes crypto_sign_ed25519_amd64_64_shortsc25519_from16bytes
#define sc25519_from64bytes      crypto_sign_ed25519_amd64_64_sc25519_from64bytes
#define sc25519_from_shortsc     crypto_sign_ed25519_amd64_64_sc25519_from_shortsc
#define sc25519_to32bytes        crypto_sign_ed25519_amd64_64_sc25519_to32bytes
#define sc25519_iszero_vartime   crypto_sign_ed25519_amd64_64_sc25519_iszero_vartime
#define sc25519_isshort_vartime  crypto_sign_ed25519_amd64_64_sc25519_isshort_vartime
#define sc25519_lt               crypto_sign_ed25519_amd64_64_sc25519_lt
#define sc25519_add              crypto_sign_ed25519_amd64_64_sc25519_add
#define sc25519_sub_nored        crypto_sign_ed25519_amd64_64_sc25519_sub_nored
#define sc25519_mul              crypto_sign_ed25519_amd64_64_sc25519_mul
#define sc25519_mul_shortsc      crypto_sign_ed25519_amd64_64_sc25519_mul_shortsc
#define sc25519_window4          crypto_sign_ed25519_amd64_64_sc25519_window4
#define sc25519_slide          crypto_sign_ed25519_amd64_64_sc25519_slide
#define sc25519_2interleave2     crypto_sign_ed25519_amd64_64_sc25519_2interleave2
#define sc25519_barrett crypto_sign_ed25519_amd64_64_sc25519_barrett

typedef struct 
{
  unsigned long long v[4]; 
}
sc25519;

typedef struct 
{
  unsigned long long v[2]; 
}
shortsc25519;

void sc25519_from32bytes(sc25519 *r, const unsigned char x[32]);

void sc25519_from64bytes(sc25519 *r, const unsigned char x[64]);

void sc25519_from_shortsc(sc25519 *r, const shortsc25519 *x);

void sc25519_to32bytes(unsigned char r[32], const sc25519 *x);

int sc25519_iszero_vartime(const sc25519 *x);

int sc25519_lt(const sc25519 *x, const sc25519 *y);

void sc25519_add(sc25519 *r, const sc25519 *x, const sc25519 *y);

void sc25519_sub_nored(sc25519 *r, const sc25519 *x, const sc25519 *y);

void sc25519_mul(sc25519 *r, const sc25519 *x, const sc25519 *y);

void sc25519_mul_shortsc(sc25519 *r, const sc25519 *x, const shortsc25519 *y);

/* Convert s into a representation of the form \sum_{i=0}^{63}r[i]2^(4*i)
 * with r[i] in {-8,...,7}
 */
void sc25519_window4(signed char r[85], const sc25519 *s);

void sc25519_slide(signed char r[256], const sc25519 *s, int swindowsize);

void sc25519_2interleave2(unsigned char r[127], const sc25519 *s1, const sc25519 *s2);

void sc25519_barrett(sc25519 *r, unsigned long long x[8]);

#endif
