/*
20080912
D. J. Bernstein
Public domain.
*/

// Small modifications have been made for ZeroTier, but this code remains in the public domain.

#include "Constants.hpp"
#include "Poly1305.hpp"
#include "Utils.hpp"

#include <cstring>

#ifdef __WINDOWS__
#pragma warning(disable: 4146)
#endif

#define U8TO64(p) Utils::loadLittleEndian<uint64_t>(p)
#define U64TO8(p,v) Utils::storeLittleEndian<uint64_t>(p,v)
#define U8TO32(p) Utils::loadLittleEndian<uint32_t>(p)
#define U32TO8(p,v) Utils::storeLittleEndian<uint32_t>(p,v)

namespace ZeroTier {

namespace {

typedef struct poly1305_context {
  size_t aligner;
  unsigned char opaque[136];
} poly1305_context;

#ifdef ZT_HAVE_UINT128

#define MUL(out, x, y) out = ((uint128_t)x * y)
#define ADD(out, in) out += in
#define ADDLO(out, in) out += in
#define SHR(in, shift) (unsigned long long)(in >> (shift))
#define LO(in) (unsigned long long)(in)

#define poly1305_block_size 16

typedef struct poly1305_state_internal_t {
  unsigned long long r[3];
  unsigned long long h[3];
  unsigned long long pad[2];
  size_t leftover;
  unsigned char buffer[poly1305_block_size];
  unsigned char final;
} poly1305_state_internal_t;

ZT_INLINE void poly1305_init(poly1305_context *ctx,const unsigned char key[32])
{
  poly1305_state_internal_t *st = (poly1305_state_internal_t *)ctx;
  unsigned long long t0,t1;

  /* r &= 0xffffffc0ffffffc0ffffffc0fffffff */
  t0 = U8TO64(&key[0]);
  t1 = U8TO64(&key[8]);

  st->r[0] = ( t0                    ) & 0xffc0fffffff;
  st->r[1] = ((t0 >> 44) | (t1 << 20)) & 0xfffffc0ffff;
  st->r[2] = ((t1 >> 24)             ) & 0x00ffffffc0f;

  /* h = 0 */
  st->h[0] = 0;
  st->h[1] = 0;
  st->h[2] = 0;

  /* save pad for later */
  st->pad[0] = U8TO64(&key[16]);
  st->pad[1] = U8TO64(&key[24]);

  st->leftover = 0;
  st->final = 0;
}

void poly1305_blocks(poly1305_state_internal_t *st, const unsigned char *m, size_t bytes)
{
  const unsigned long long hibit = (st->final) ? 0 : ((unsigned long long)1 << 40); /* 1 << 128 */
  unsigned long long r0,r1,r2;
  unsigned long long s1,s2;
  unsigned long long h0,h1,h2;
  uint128_t d0,d1,d2,d;

  r0 = st->r[0];
  r1 = st->r[1];
  r2 = st->r[2];

  h0 = st->h[0];
  h1 = st->h[1];
  h2 = st->h[2];

  s1 = r1 * (5 << 2);
  s2 = r2 * (5 << 2);

  while (bytes >= poly1305_block_size) {
    unsigned long long t0,t1;

    /* h += m[i] */
    t0 = U8TO64(&m[0]);
    t1 = U8TO64(&m[8]);

    h0 += (( t0                    ) & 0xfffffffffff);
    h1 += (((t0 >> 44) | (t1 << 20)) & 0xfffffffffff);
    h2 += (((t1 >> 24)             ) & 0x3ffffffffff) | hibit;

    /* h *= r */
    MUL(d0, h0, r0); MUL(d, h1, s2); ADD(d0, d); MUL(d, h2, s1); ADD(d0, d);
    MUL(d1, h0, r1); MUL(d, h1, r0); ADD(d1, d); MUL(d, h2, s2); ADD(d1, d);
    MUL(d2, h0, r2); MUL(d, h1, r1); ADD(d2, d); MUL(d, h2, r0); ADD(d2, d);

    /* (partial) h %= p */
    unsigned long long c = SHR(d0, 44); h0 = LO(d0) & 0xfffffffffff;
    ADDLO(d1, c); c = SHR(d1, 44); h1 = LO(d1) & 0xfffffffffff;
    ADDLO(d2, c); c = SHR(d2, 42); h2 = LO(d2) & 0x3ffffffffff;
    h0  += c * 5; c = (h0 >> 44);  h0 =    h0  & 0xfffffffffff;
    h1  += c;

    m += poly1305_block_size;
    bytes -= poly1305_block_size;
  }

  st->h[0] = h0;
  st->h[1] = h1;
  st->h[2] = h2;
}

ZT_INLINE void poly1305_finish(poly1305_context *ctx,unsigned char mac[16])
{
  poly1305_state_internal_t *st = (poly1305_state_internal_t *)ctx;
  unsigned long long h0,h1,h2,c;
  unsigned long long g0,g1,g2;
  unsigned long long t0,t1;

  /* process the remaining block */
  if (st->leftover) {
    size_t i = st->leftover;
    st->buffer[i] = 1;
    for (i = i + 1; i < poly1305_block_size; i++)
      st->buffer[i] = 0;
    st->final = 1;
    poly1305_blocks(st, st->buffer, poly1305_block_size);
  }

  /* fully carry h */
  h0 = st->h[0];
  h1 = st->h[1];
  h2 = st->h[2];

               c = (h1 >> 44); h1 &= 0xfffffffffff;
  h2 += c;     c = (h2 >> 42); h2 &= 0x3ffffffffff;
  h0 += c * 5; c = (h0 >> 44); h0 &= 0xfffffffffff;
  h1 += c;     c = (h1 >> 44); h1 &= 0xfffffffffff;
  h2 += c;     c = (h2 >> 42); h2 &= 0x3ffffffffff;
  h0 += c * 5; c = (h0 >> 44); h0 &= 0xfffffffffff;
  h1 += c;

  /* compute h + -p */
  g0 = h0 + 5; c = (g0 >> 44); g0 &= 0xfffffffffff;
  g1 = h1 + c; c = (g1 >> 44); g1 &= 0xfffffffffff;
  g2 = h2 + c - ((unsigned long long)1 << 42);

  /* select h if h < p, or h + -p if h >= p */
  c = (g2 >> ((sizeof(unsigned long long) * 8) - 1)) - 1;
  g0 &= c;
  g1 &= c;
  g2 &= c;
  c = ~c;
  h0 = (h0 & c) | g0;
  h1 = (h1 & c) | g1;
  h2 = (h2 & c) | g2;

  /* h = (h + pad) */
  t0 = st->pad[0];
  t1 = st->pad[1];

  h0 += (( t0                    ) & 0xfffffffffff)    ; c = (h0 >> 44); h0 &= 0xfffffffffff;
  h1 += (((t0 >> 44) | (t1 << 20)) & 0xfffffffffff) + c; c = (h1 >> 44); h1 &= 0xfffffffffff;
  h2 += (((t1 >> 24)             ) & 0x3ffffffffff) + c;                 h2 &= 0x3ffffffffff;

  /* mac = h % (2^128) */
  h0 = ((h0      ) | (h1 << 44));
  h1 = ((h1 >> 20) | (h2 << 24));

  U64TO8(&mac[0], h0);
  U64TO8(&mac[8], h1);

  /* zero out the state */
  st->h[0] = 0;
  st->h[1] = 0;
  st->h[2] = 0;
  st->r[0] = 0;
  st->r[1] = 0;
  st->r[2] = 0;
  st->pad[0] = 0;
  st->pad[1] = 0;
}

#else // no uint128_t

#define poly1305_block_size 16

typedef struct poly1305_state_internal_t {
  unsigned long r[5];
  unsigned long h[5];
  unsigned long pad[4];
  size_t leftover;
  unsigned char buffer[poly1305_block_size];
  unsigned char final;
} poly1305_state_internal_t;

ZT_INLINE void poly1305_init(poly1305_context *ctx, const unsigned char key[32])
{
  poly1305_state_internal_t *st = (poly1305_state_internal_t *)ctx;

  /* r &= 0xffffffc0ffffffc0ffffffc0fffffff */
  st->r[0] = (U8TO32(&key[ 0])     ) & 0x3ffffff;
  st->r[1] = (U8TO32(&key[ 3]) >> 2) & 0x3ffff03;
  st->r[2] = (U8TO32(&key[ 6]) >> 4) & 0x3ffc0ff;
  st->r[3] = (U8TO32(&key[ 9]) >> 6) & 0x3f03fff;
  st->r[4] = (U8TO32(&key[12]) >> 8) & 0x00fffff;

  /* h = 0 */
  st->h[0] = 0;
  st->h[1] = 0;
  st->h[2] = 0;
  st->h[3] = 0;
  st->h[4] = 0;

  /* save pad for later */
  st->pad[0] = U8TO32(&key[16]);
  st->pad[1] = U8TO32(&key[20]);
  st->pad[2] = U8TO32(&key[24]);
  st->pad[3] = U8TO32(&key[28]);

  st->leftover = 0;
  st->final = 0;
}

void poly1305_blocks(poly1305_state_internal_t *st, const unsigned char *m, size_t bytes)
{
  const unsigned long hibit = (st->final) ? 0 : (1 << 24); /* 1 << 128 */
  unsigned long r0,r1,r2,r3,r4;
  unsigned long s1,s2,s3,s4;
  unsigned long h0,h1,h2,h3,h4;

  r0 = st->r[0];
  r1 = st->r[1];
  r2 = st->r[2];
  r3 = st->r[3];
  r4 = st->r[4];

  s1 = r1 * 5;
  s2 = r2 * 5;
  s3 = r3 * 5;
  s4 = r4 * 5;

  h0 = st->h[0];
  h1 = st->h[1];
  h2 = st->h[2];
  h3 = st->h[3];
  h4 = st->h[4];

  while (bytes >= poly1305_block_size) {
    /* h += m[i] */
    h0 += (U8TO32(m+ 0)     ) & 0x3ffffff;
    h1 += (U8TO32(m+ 3) >> 2) & 0x3ffffff;
    h2 += (U8TO32(m+ 6) >> 4) & 0x3ffffff;
    h3 += (U8TO32(m+ 9) >> 6) & 0x3ffffff;
    h4 += (U8TO32(m+12) >> 8) | hibit;

    /* h *= r */
    unsigned long long d0 = ((unsigned long long)h0 * r0) + ((unsigned long long)h1 * s4) + ((unsigned long long)h2 * s3) + ((unsigned long long)h3 * s2) + ((unsigned long long)h4 * s1);
    unsigned long long d1 = ((unsigned long long)h0 * r1) + ((unsigned long long)h1 * r0) + ((unsigned long long)h2 * s4) + ((unsigned long long)h3 * s3) + ((unsigned long long)h4 * s2);
    unsigned long long d2 = ((unsigned long long)h0 * r2) + ((unsigned long long)h1 * r1) + ((unsigned long long)h2 * r0) + ((unsigned long long)h3 * s4) + ((unsigned long long)h4 * s3);
    unsigned long long d3 = ((unsigned long long)h0 * r3) + ((unsigned long long)h1 * r2) + ((unsigned long long)h2 * r1) + ((unsigned long long)h3 * r0) + ((unsigned long long)h4 * s4);
    unsigned long long d4 = ((unsigned long long)h0 * r4) + ((unsigned long long)h1 * r3) + ((unsigned long long)h2 * r2) + ((unsigned long long)h3 * r1) + ((unsigned long long)h4 * r0);

    /* (partial) h %= p */
    unsigned long c = (unsigned long)(d0 >> 26); h0 = (unsigned long)d0 & 0x3ffffff;
    d1 += c;      c = (unsigned long)(d1 >> 26); h1 = (unsigned long)d1 & 0x3ffffff;
    d2 += c;      c = (unsigned long)(d2 >> 26); h2 = (unsigned long)d2 & 0x3ffffff;
    d3 += c;      c = (unsigned long)(d3 >> 26); h3 = (unsigned long)d3 & 0x3ffffff;
    d4 += c;      c = (unsigned long)(d4 >> 26); h4 = (unsigned long)d4 & 0x3ffffff;
    h0 += c * 5;  c =                (h0 >> 26); h0 =                h0 & 0x3ffffff;
    h1 += c;

    m += poly1305_block_size;
    bytes -= poly1305_block_size;
  }

  st->h[0] = h0;
  st->h[1] = h1;
  st->h[2] = h2;
  st->h[3] = h3;
  st->h[4] = h4;
}

ZT_INLINE void poly1305_finish(poly1305_context *ctx, unsigned char mac[16])
{
  poly1305_state_internal_t *st = (poly1305_state_internal_t *)ctx;
  unsigned long h0,h1,h2,h3,h4,c;
  unsigned long g0,g1,g2,g3,g4;
  unsigned long long f;
  unsigned long mask;

  /* process the remaining block */
  if (st->leftover) {
    size_t i = st->leftover;
    st->buffer[i++] = 1;
    for (; i < poly1305_block_size; i++)
      st->buffer[i] = 0;
    st->final = 1;
    poly1305_blocks(st, st->buffer, poly1305_block_size);
  }

  /* fully carry h */
  h0 = st->h[0];
  h1 = st->h[1];
  h2 = st->h[2];
  h3 = st->h[3];
  h4 = st->h[4];

               c = h1 >> 26; h1 = h1 & 0x3ffffff;
  h2 +=     c; c = h2 >> 26; h2 = h2 & 0x3ffffff;
  h3 +=     c; c = h3 >> 26; h3 = h3 & 0x3ffffff;
  h4 +=     c; c = h4 >> 26; h4 = h4 & 0x3ffffff;
  h0 += c * 5; c = h0 >> 26; h0 = h0 & 0x3ffffff;
  h1 +=     c;

  /* compute h + -p */
  g0 = h0 + 5; c = g0 >> 26; g0 &= 0x3ffffff;
  g1 = h1 + c; c = g1 >> 26; g1 &= 0x3ffffff;
  g2 = h2 + c; c = g2 >> 26; g2 &= 0x3ffffff;
  g3 = h3 + c; c = g3 >> 26; g3 &= 0x3ffffff;
  g4 = h4 + c - (1 << 26);

  /* select h if h < p, or h + -p if h >= p */
  mask = (g4 >> ((sizeof(unsigned long) * 8) - 1)) - 1;
  g0 &= mask;
  g1 &= mask;
  g2 &= mask;
  g3 &= mask;
  g4 &= mask;
  mask = ~mask;
  h0 = (h0 & mask) | g0;
  h1 = (h1 & mask) | g1;
  h2 = (h2 & mask) | g2;
  h3 = (h3 & mask) | g3;
  h4 = (h4 & mask) | g4;

  /* h = h % (2^128) */
  h0 = ((h0      ) | (h1 << 26)) & 0xffffffff;
  h1 = ((h1 >>  6) | (h2 << 20)) & 0xffffffff;
  h2 = ((h2 >> 12) | (h3 << 14)) & 0xffffffff;
  h3 = ((h3 >> 18) | (h4 <<  8)) & 0xffffffff;

  /* mac = (h + pad) % (2^128) */
  f = (unsigned long long)h0 + st->pad[0]            ; h0 = (unsigned long)f;
  f = (unsigned long long)h1 + st->pad[1] + (f >> 32); h1 = (unsigned long)f;
  f = (unsigned long long)h2 + st->pad[2] + (f >> 32); h2 = (unsigned long)f;
  f = (unsigned long long)h3 + st->pad[3] + (f >> 32); h3 = (unsigned long)f;

  U32TO8(mac +  0, h0);
  U32TO8(mac +  4, h1);
  U32TO8(mac +  8, h2);
  U32TO8(mac + 12, h3);

  /* zero out the state */
  st->h[0] = 0;
  st->h[1] = 0;
  st->h[2] = 0;
  st->h[3] = 0;
  st->h[4] = 0;
  st->r[0] = 0;
  st->r[1] = 0;
  st->r[2] = 0;
  st->r[3] = 0;
  st->r[4] = 0;
  st->pad[0] = 0;
  st->pad[1] = 0;
  st->pad[2] = 0;
  st->pad[3] = 0;
}

#endif // uint128_t or portable version?

ZT_INLINE void poly1305_update(poly1305_context *ctx,const unsigned char *m,size_t bytes) noexcept
{
  poly1305_state_internal_t *st = (poly1305_state_internal_t *)ctx;
  size_t i;

  /* handle leftover */
  if (st->leftover) {
    size_t want = (poly1305_block_size - st->leftover);
    if (want > bytes)
      want = bytes;
    for (i = 0; i < want; i++)
      st->buffer[st->leftover + i] = m[i];
    bytes -= want;
    m += want;
    st->leftover += want;
    if (st->leftover < poly1305_block_size)
      return;
    poly1305_blocks(st, st->buffer, poly1305_block_size);
    st->leftover = 0;
  }

  /* process full blocks */
  if (bytes >= poly1305_block_size) {
    size_t want = (bytes & ~(poly1305_block_size - 1));
    poly1305_blocks(st, m, want);
    m += want;
    bytes -= want;
  }

  /* store leftover */
  if (bytes) {
    for (i = 0; i < bytes; i++)
      st->buffer[st->leftover + i] = m[i];
    st->leftover += bytes;
  }
}

} // anonymous namespace

void Poly1305::init(const void *key) noexcept
{
  static_assert(sizeof(ctx) >= sizeof(poly1305_context),"buffer in class smaller than required structure size");
  poly1305_init(reinterpret_cast<poly1305_context *>(&ctx),reinterpret_cast<const unsigned char *>(key));
}

void Poly1305::update(const void *data,unsigned int len) noexcept
{
  poly1305_update(reinterpret_cast<poly1305_context *>(&ctx),reinterpret_cast<const unsigned char *>(data),(size_t)len);
}

void Poly1305::finish(void *auth) noexcept
{
  poly1305_finish(reinterpret_cast<poly1305_context *>(&ctx),reinterpret_cast<unsigned char *>(auth));
}

} // namespace ZeroTier
