/*
 * Based on public domain code available at: http://cr.yp.to/snuffle.html
 *
 * This therefore is public domain.
 */

#include "Salsa20.hpp"

#define ROTATE(v,c) (((v) << (c)) | ((v) >> (32 - (c))))
#define XOR(v,w) ((v) ^ (w))
#define PLUS(v,w) ((uint32_t)((v) + (w)))
#define PLUSONE(v) ((uint32_t)((v) + 1))

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define U8TO32_LITTLE(p) (*((const uint32_t *)((const void *)(p))))
#define U32TO8_LITTLE(c,v) *((uint32_t *)((void *)(c))) = (v)
#else
#ifdef __GNUC__
#define U8TO32_LITTLE(p) __builtin_bswap32(*((const uint32_t *)((const void *)(p))))
#define U32TO8_LITTLE(c,v) *((uint32_t *)((void *)(c))) = __builtin_bswap32((v))
#else
error need be;
#endif
#endif

namespace ZeroTier {

static const char *sigma = "expand 32-byte k";
static const char *tau = "expand 16-byte k";

void Salsa20::init(const void *key,unsigned int kbits,const void *iv)
	throw()
{
	const char *constants;
	const uint8_t *k = (const uint8_t *)key;

	_state[1] = U8TO32_LITTLE(k + 0);
	_state[2] = U8TO32_LITTLE(k + 4);
	_state[3] = U8TO32_LITTLE(k + 8);
	_state[4] = U8TO32_LITTLE(k + 12);
	if (kbits == 256) { /* recommended */
		k += 16;
		constants = sigma;
	} else { /* kbits == 128 */
		constants = tau;
	}
	_state[11] = U8TO32_LITTLE(k + 0);
	_state[12] = U8TO32_LITTLE(k + 4);
	_state[13] = U8TO32_LITTLE(k + 8);
	_state[14] = U8TO32_LITTLE(k + 12);

	_state[6] = U8TO32_LITTLE(((const uint8_t *)iv) + 0);
	_state[7] = U8TO32_LITTLE(((const uint8_t *)iv) + 4);
	_state[8] = 0;
	_state[9] = 0;

	_state[0] = U8TO32_LITTLE(constants + 0);
	_state[5] = U8TO32_LITTLE(constants + 4);
	_state[10] = U8TO32_LITTLE(constants + 8);
	_state[15] = U8TO32_LITTLE(constants + 12);
}

void Salsa20::encrypt(const void *in,void *out,unsigned int bytes)
	throw()
{
	uint32_t x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15;
	uint32_t j0, j1, j2, j3, j4, j5, j6, j7, j8, j9, j10, j11, j12, j13, j14, j15;
	uint8_t tmp[64];
	const uint8_t *m = (const uint8_t *)in;
	uint8_t *c = (uint8_t *)out;
	uint8_t *ctarget = c;
	unsigned int i;

	if (!bytes) return;

	j0 = _state[0];
	j1 = _state[1];
	j2 = _state[2];
	j3 = _state[3];
	j4 = _state[4];
	j5 = _state[5];
	j6 = _state[6];
	j7 = _state[7];
	j8 = _state[8];
	j9 = _state[9];
	j10 = _state[10];
	j11 = _state[11];
	j12 = _state[12];
	j13 = _state[13];
	j14 = _state[14];
	j15 = _state[15];

	for (;;) {
		if (bytes < 64) {
			for (i = 0;i < bytes;++i) tmp[i] = m[i];
			m = tmp;
			ctarget = c;
			c = tmp;
		}
		x0 = j0;
		x1 = j1;
		x2 = j2;
		x3 = j3;
		x4 = j4;
		x5 = j5;
		x6 = j6;
		x7 = j7;
		x8 = j8;
		x9 = j9;
		x10 = j10;
		x11 = j11;
		x12 = j12;
		x13 = j13;
		x14 = j14;
		x15 = j15;
		for (i = 20;i > 0;i -= 2) {
			 x4 = XOR( x4,ROTATE(PLUS( x0,x12), 7));
			 x8 = XOR( x8,ROTATE(PLUS( x4, x0), 9));
			x12 = XOR(x12,ROTATE(PLUS( x8, x4),13));
			 x0 = XOR( x0,ROTATE(PLUS(x12, x8),18));
			 x9 = XOR( x9,ROTATE(PLUS( x5, x1), 7));
			x13 = XOR(x13,ROTATE(PLUS( x9, x5), 9));
			 x1 = XOR( x1,ROTATE(PLUS(x13, x9),13));
			 x5 = XOR( x5,ROTATE(PLUS( x1,x13),18));
			x14 = XOR(x14,ROTATE(PLUS(x10, x6), 7));
			 x2 = XOR( x2,ROTATE(PLUS(x14,x10), 9));
			 x6 = XOR( x6,ROTATE(PLUS( x2,x14),13));
			x10 = XOR(x10,ROTATE(PLUS( x6, x2),18));
			 x3 = XOR( x3,ROTATE(PLUS(x15,x11), 7));
			 x7 = XOR( x7,ROTATE(PLUS( x3,x15), 9));
			x11 = XOR(x11,ROTATE(PLUS( x7, x3),13));
			x15 = XOR(x15,ROTATE(PLUS(x11, x7),18));
			 x1 = XOR( x1,ROTATE(PLUS( x0, x3), 7));
			 x2 = XOR( x2,ROTATE(PLUS( x1, x0), 9));
			 x3 = XOR( x3,ROTATE(PLUS( x2, x1),13));
			 x0 = XOR( x0,ROTATE(PLUS( x3, x2),18));
			 x6 = XOR( x6,ROTATE(PLUS( x5, x4), 7));
			 x7 = XOR( x7,ROTATE(PLUS( x6, x5), 9));
			 x4 = XOR( x4,ROTATE(PLUS( x7, x6),13));
			 x5 = XOR( x5,ROTATE(PLUS( x4, x7),18));
			x11 = XOR(x11,ROTATE(PLUS(x10, x9), 7));
			 x8 = XOR( x8,ROTATE(PLUS(x11,x10), 9));
			 x9 = XOR( x9,ROTATE(PLUS( x8,x11),13));
			x10 = XOR(x10,ROTATE(PLUS( x9, x8),18));
			x12 = XOR(x12,ROTATE(PLUS(x15,x14), 7));
			x13 = XOR(x13,ROTATE(PLUS(x12,x15), 9));
			x14 = XOR(x14,ROTATE(PLUS(x13,x12),13));
			x15 = XOR(x15,ROTATE(PLUS(x14,x13),18));
		}
		x0 = PLUS(x0,j0);
		x1 = PLUS(x1,j1);
		x2 = PLUS(x2,j2);
		x3 = PLUS(x3,j3);
		x4 = PLUS(x4,j4);
		x5 = PLUS(x5,j5);
		x6 = PLUS(x6,j6);
		x7 = PLUS(x7,j7);
		x8 = PLUS(x8,j8);
		x9 = PLUS(x9,j9);
		x10 = PLUS(x10,j10);
		x11 = PLUS(x11,j11);
		x12 = PLUS(x12,j12);
		x13 = PLUS(x13,j13);
		x14 = PLUS(x14,j14);
		x15 = PLUS(x15,j15);

		x0 = XOR(x0,U8TO32_LITTLE(m + 0));
		x1 = XOR(x1,U8TO32_LITTLE(m + 4));
		x2 = XOR(x2,U8TO32_LITTLE(m + 8));
		x3 = XOR(x3,U8TO32_LITTLE(m + 12));
		x4 = XOR(x4,U8TO32_LITTLE(m + 16));
		x5 = XOR(x5,U8TO32_LITTLE(m + 20));
		x6 = XOR(x6,U8TO32_LITTLE(m + 24));
		x7 = XOR(x7,U8TO32_LITTLE(m + 28));
		x8 = XOR(x8,U8TO32_LITTLE(m + 32));
		x9 = XOR(x9,U8TO32_LITTLE(m + 36));
		x10 = XOR(x10,U8TO32_LITTLE(m + 40));
		x11 = XOR(x11,U8TO32_LITTLE(m + 44));
		x12 = XOR(x12,U8TO32_LITTLE(m + 48));
		x13 = XOR(x13,U8TO32_LITTLE(m + 52));
		x14 = XOR(x14,U8TO32_LITTLE(m + 56));
		x15 = XOR(x15,U8TO32_LITTLE(m + 60));

		j8 = PLUSONE(j8);
		if (!j8) {
			j9 = PLUSONE(j9);
			/* stopping at 2^70 bytes per nonce is user's responsibility */
		}

		U32TO8_LITTLE(c + 0,x0);
		U32TO8_LITTLE(c + 4,x1);
		U32TO8_LITTLE(c + 8,x2);
		U32TO8_LITTLE(c + 12,x3);
		U32TO8_LITTLE(c + 16,x4);
		U32TO8_LITTLE(c + 20,x5);
		U32TO8_LITTLE(c + 24,x6);
		U32TO8_LITTLE(c + 28,x7);
		U32TO8_LITTLE(c + 32,x8);
		U32TO8_LITTLE(c + 36,x9);
		U32TO8_LITTLE(c + 40,x10);
		U32TO8_LITTLE(c + 44,x11);
		U32TO8_LITTLE(c + 48,x12);
		U32TO8_LITTLE(c + 52,x13);
		U32TO8_LITTLE(c + 56,x14);
		U32TO8_LITTLE(c + 60,x15);

		if (bytes <= 64) {
			if (bytes < 64) {
				for (i = 0;i < bytes;++i) ctarget[i] = c[i];
			}
			_state[8] = j8;
			_state[9] = j9;
			return;
		}
		bytes -= 64;
		c += 64;
		m += 64;
	}
}

} // namespace ZeroTier
