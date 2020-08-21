// This code is public domain, taken from a PD crypto source file on GitHub.

#include "SHA512.hpp"
#include "Utils.hpp"

namespace ZeroTier {

#ifndef ZT_HAVE_NATIVE_SHA512

namespace {

struct sha512_state {
	uint64_t length,state[8];
	unsigned long curlen;
	uint8_t buf[128];
};

static const uint64_t K[80] = {
	0x428a2f98d728ae22ULL,0x7137449123ef65cdULL,0xb5c0fbcfec4d3b2fULL,0xe9b5dba58189dbbcULL,
	0x3956c25bf348b538ULL,0x59f111f1b605d019ULL,0x923f82a4af194f9bULL,0xab1c5ed5da6d8118ULL,
	0xd807aa98a3030242ULL,0x12835b0145706fbeULL,0x243185be4ee4b28cULL,0x550c7dc3d5ffb4e2ULL,
	0x72be5d74f27b896fULL,0x80deb1fe3b1696b1ULL,0x9bdc06a725c71235ULL,0xc19bf174cf692694ULL,
	0xe49b69c19ef14ad2ULL,0xefbe4786384f25e3ULL,0x0fc19dc68b8cd5b5ULL,0x240ca1cc77ac9c65ULL,
	0x2de92c6f592b0275ULL,0x4a7484aa6ea6e483ULL,0x5cb0a9dcbd41fbd4ULL,0x76f988da831153b5ULL,
	0x983e5152ee66dfabULL,0xa831c66d2db43210ULL,0xb00327c898fb213fULL,0xbf597fc7beef0ee4ULL,
	0xc6e00bf33da88fc2ULL,0xd5a79147930aa725ULL,0x06ca6351e003826fULL,0x142929670a0e6e70ULL,
	0x27b70a8546d22ffcULL,0x2e1b21385c26c926ULL,0x4d2c6dfc5ac42aedULL,0x53380d139d95b3dfULL,
	0x650a73548baf63deULL,0x766a0abb3c77b2a8ULL,0x81c2c92e47edaee6ULL,0x92722c851482353bULL,
	0xa2bfe8a14cf10364ULL,0xa81a664bbc423001ULL,0xc24b8b70d0f89791ULL,0xc76c51a30654be30ULL,
	0xd192e819d6ef5218ULL,0xd69906245565a910ULL,0xf40e35855771202aULL,0x106aa07032bbd1b8ULL,
	0x19a4c116b8d2d0c8ULL,0x1e376c085141ab53ULL,0x2748774cdf8eeb99ULL,0x34b0bcb5e19b48a8ULL,
	0x391c0cb3c5c95a63ULL,0x4ed8aa4ae3418acbULL,0x5b9cca4f7763e373ULL,0x682e6ff3d6b2b8a3ULL,
	0x748f82ee5defb2fcULL,0x78a5636f43172f60ULL,0x84c87814a1f0ab72ULL,0x8cc702081a6439ecULL,
	0x90befffa23631e28ULL,0xa4506cebde82bde9ULL,0xbef9a3f7b2c67915ULL,0xc67178f2e372532bULL,
	0xca273eceea26619cULL,0xd186b8c721c0c207ULL,0xeada7dd6cde0eb1eULL,0xf57d4f7fee6ed178ULL,
	0x06f067aa72176fbaULL,0x0a637dc5a2c898a6ULL,0x113f9804bef90daeULL,0x1b710b35131c471bULL,
	0x28db77f523047d84ULL,0x32caab7b40c72493ULL,0x3c9ebe0a15c9bebcULL,0x431d67c49c100d4cULL,
	0x4cc5d4becb3e42b6ULL,0x597f299cfc657e2aULL,0x5fcb6fab3ad6faecULL,0x6c44198c4a475817ULL
};

#define STORE64H(x, y) Utils::storeBigEndian<uint64_t>(y,x)
#define LOAD64H(x, y) x = Utils::loadBigEndian<uint64_t>(y)
#define ROL64c(x,y) (((x)<<(y)) | ((x)>>(64-(y))))
#define ROR64c(x,y) (((x)>>(y)) | ((x)<<(64-(y))))
#define Ch(x,y,z)       (z ^ (x & (y ^ z)))
#define Maj(x,y,z)      (((x | y) & z) | (x & y))
#define S(x, n)         ROR64c(x, n)
#define R(x, n)         ((x)>>(n))
#define Sigma0(x)       (S(x, 28) ^ S(x, 34) ^ S(x, 39))
#define Sigma1(x)       (S(x, 14) ^ S(x, 18) ^ S(x, 41))
#define Gamma0(x)       (S(x, 1) ^ S(x, 8) ^ R(x, 7))
#define Gamma1(x)       (S(x, 19) ^ S(x, 61) ^ R(x, 6))

static ZT_INLINE void sha512_compress(sha512_state *const md,uint8_t *const buf)
{
	uint64_t S[8], W[80], t0, t1;
	int i;

	for (i = 0; i < 8; i++)
		S[i] = md->state[i];
	for (i = 0; i < 16; i++)
		LOAD64H(W[i], buf + (8*i));
	for (i = 16; i < 80; i++)
		W[i] = Gamma1(W[i - 2]) + W[i - 7] + Gamma0(W[i - 15]) + W[i - 16];

#define RND(a,b,c,d,e,f,g,h,i) \
	t0 = h + Sigma1(e) + Ch(e, f, g) + K[i] + W[i]; \
	t1 = Sigma0(a) + Maj(a, b, c); \
	d += t0; \
	h  = t0 + t1;

	for (i = 0; i < 80; i += 8) {
		RND(S[0],S[1],S[2],S[3],S[4],S[5],S[6],S[7],i+0);
		RND(S[7],S[0],S[1],S[2],S[3],S[4],S[5],S[6],i+1);
		RND(S[6],S[7],S[0],S[1],S[2],S[3],S[4],S[5],i+2);
		RND(S[5],S[6],S[7],S[0],S[1],S[2],S[3],S[4],i+3);
		RND(S[4],S[5],S[6],S[7],S[0],S[1],S[2],S[3],i+4);
		RND(S[3],S[4],S[5],S[6],S[7],S[0],S[1],S[2],i+5);
		RND(S[2],S[3],S[4],S[5],S[6],S[7],S[0],S[1],i+6);
		RND(S[1],S[2],S[3],S[4],S[5],S[6],S[7],S[0],i+7);
	}

	for (i = 0; i < 8; i++)
		md->state[i] = md->state[i] + S[i];
}

static ZT_INLINE void sha384_init(sha512_state *const md)
{
	md->curlen = 0;
	md->length = 0;
	md->state[0] = 0xcbbb9d5dc1059ed8ULL;
	md->state[1] = 0x629a292a367cd507ULL;
	md->state[2] = 0x9159015a3070dd17ULL;
	md->state[3] = 0x152fecd8f70e5939ULL;
	md->state[4] = 0x67332667ffc00b31ULL;
	md->state[5] = 0x8eb44a8768581511ULL;
	md->state[6] = 0xdb0c2e0d64f98fa7ULL;
	md->state[7] = 0x47b5481dbefa4fa4ULL;
}

static ZT_INLINE void sha512_init(sha512_state *const md)
{
	md->curlen = 0;
	md->length = 0;
	md->state[0] = 0x6a09e667f3bcc908ULL;
	md->state[1] = 0xbb67ae8584caa73bULL;
	md->state[2] = 0x3c6ef372fe94f82bULL;
	md->state[3] = 0xa54ff53a5f1d36f1ULL;
	md->state[4] = 0x510e527fade682d1ULL;
	md->state[5] = 0x9b05688c2b3e6c1fULL;
	md->state[6] = 0x1f83d9abfb41bd6bULL;
	md->state[7] = 0x5be0cd19137e2179ULL;
}

static void sha512_process(sha512_state *const md,const uint8_t *in,unsigned long inlen)
{
	while (inlen > 0) {
		if (md->curlen == 0 && inlen >= 128) {
			sha512_compress(md,(uint8_t *)in);
			md->length     += 128 * 8;
			in             += 128;
			inlen          -= 128;
		} else {
			unsigned long n = std::min(inlen,(128 - md->curlen));
			Utils::copy(md->buf + md->curlen,in,n);
			md->curlen += n;
			in             += n;
			inlen          -= n;
			if (md->curlen == 128) {
				sha512_compress(md,md->buf);
				md->length += 8*128;
				md->curlen = 0;
			}
		}
	}
}

static ZT_INLINE void sha512_done(sha512_state *const md,uint8_t *out)
{
	int i;

	md->length += md->curlen * 8ULL;
	md->buf[md->curlen++] = (uint8_t)0x80;

	if (md->curlen > 112) {
		while (md->curlen < 128) {
			md->buf[md->curlen++] = (uint8_t)0;
		}
		sha512_compress(md, md->buf);
		md->curlen = 0;
	}

	while (md->curlen < 120) {
		md->buf[md->curlen++] = (uint8_t)0;
	}

	STORE64H(md->length, md->buf+120);
	sha512_compress(md, md->buf);

	for (i = 0; i < 8; i++) {
		STORE64H(md->state[i], out+(8*i));
	}
}

} // anonymous namespace

void SHA512(void *digest,const void *data,unsigned int len)
{
	sha512_state state;
	sha512_init(&state);
	sha512_process(&state,(uint8_t *)data,(unsigned long)len);
	sha512_done(&state,(uint8_t *)digest);
}

void SHA384(void *digest,const void *data,unsigned int len)
{
	uint8_t tmp[64];
	sha512_state state;
	sha384_init(&state);
	sha512_process(&state,(uint8_t *)data,(unsigned long)len);
	sha512_done(&state,tmp);
	Utils::copy<48>(digest,tmp);
}

void SHA384(void *digest,const void *data0,unsigned int len0,const void *data1,unsigned int len1)
{
	uint8_t tmp[64];
	sha512_state state;
	sha384_init(&state);
	sha512_process(&state,(uint8_t *)data0,(unsigned long)len0);
	sha512_process(&state,(uint8_t *)data1,(unsigned long)len1);
	sha512_done(&state,tmp);
	Utils::copy<48>(digest,tmp);
}

#endif // !ZT_HAVE_NATIVE_SHA512

void HMACSHA384(const uint8_t key[ZT_SYMMETRIC_KEY_SIZE],const void *msg,const unsigned int msglen,uint8_t mac[48])
{
	uint64_t kInPadded[16]; // input padded key
	uint64_t outer[22]; // output padded key | H(input padded key | msg)

	const uint64_t k0 = Utils::loadMachineEndian< uint64_t >(key);
	const uint64_t k1 = Utils::loadMachineEndian< uint64_t >(key + 8);
	const uint64_t k2 = Utils::loadMachineEndian< uint64_t >(key + 16);
	const uint64_t k3 = Utils::loadMachineEndian< uint64_t >(key + 24);
	const uint64_t k4 = Utils::loadMachineEndian< uint64_t >(key + 32);
	const uint64_t k5 = Utils::loadMachineEndian< uint64_t >(key + 40);

	const uint64_t ipad = 0x3636363636363636ULL;
	kInPadded[0] = k0 ^ ipad;
	kInPadded[1] = k1 ^ ipad;
	kInPadded[2] = k2 ^ ipad;
	kInPadded[3] = k3 ^ ipad;
	kInPadded[4] = k4 ^ ipad;
	kInPadded[5] = k5 ^ ipad;
	kInPadded[6] = ipad;
	kInPadded[7] = ipad;
	kInPadded[8] = ipad;
	kInPadded[9] = ipad;
	kInPadded[10] = ipad;
	kInPadded[11] = ipad;
	kInPadded[12] = ipad;
	kInPadded[13] = ipad;
	kInPadded[14] = ipad;
	kInPadded[15] = ipad;

	const uint64_t opad = 0x5c5c5c5c5c5c5c5cULL;
	outer[0] = k0 ^ opad;
	outer[1] = k1 ^ opad;
	outer[2] = k2 ^ opad;
	outer[3] = k3 ^ opad;
	outer[4] = k4 ^ opad;
	outer[5] = k5 ^ opad;
	outer[6] = opad;
	outer[7] = opad;
	outer[8] = opad;
	outer[9] = opad;
	outer[10] = opad;
	outer[11] = opad;
	outer[12] = opad;
	outer[13] = opad;
	outer[14] = opad;
	outer[15] = opad;

	// H(output padded key | H(input padded key | msg))
	SHA384(reinterpret_cast<uint8_t *>(outer) + 128,kInPadded,128,msg,msglen);
	SHA384(mac,outer,176);
}

void KBKDFHMACSHA384(const uint8_t key[ZT_SYMMETRIC_KEY_SIZE],const char label,const char context,const uint32_t iter,uint8_t out[ZT_SYMMETRIC_KEY_SIZE])
{
	uint8_t kbkdfMsg[13];

	Utils::storeBigEndian<uint32_t>(kbkdfMsg,(uint32_t)iter);

	kbkdfMsg[4] = (uint8_t)'Z';
	kbkdfMsg[5] = (uint8_t)'T'; // preface our labels with something ZT-specific
	kbkdfMsg[6] = (uint8_t)label;
	kbkdfMsg[7] = 0;

	kbkdfMsg[8] = (uint8_t)context;

	// Output key length: 384 bits (as 32-bit big-endian value)
	kbkdfMsg[9] = 0;
	kbkdfMsg[10] = 0;
	kbkdfMsg[11] = 0x01;
	kbkdfMsg[12] = 0x80;

	static_assert(ZT_SYMMETRIC_KEY_SIZE == ZT_SHA384_DIGEST_SIZE,"sizeof(out) != ZT_SHA384_DIGEST_SIZE");
	HMACSHA384(key,&kbkdfMsg,sizeof(kbkdfMsg),out);
}

} // namespace ZeroTier
