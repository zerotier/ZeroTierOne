/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2024-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include "MIMC52.hpp"
#include "SHA512.hpp"
#include "AES.hpp"
#include "Utils.hpp"

// Set this to use a much slower but portable modmul code that doesn't rely on a proper 64-bit FPU.
//#define ZT_MIMC52_NO_FPU

namespace ZeroTier {

namespace {

// These are the largest 512 primes less than 2^52 and of the form (6k - 5).
const uint64_t s_mimc52Primes[512] = { 4503599627332907ULL,4503599627332943ULL,4503599627333243ULL,4503599627333321ULL,4503599627333393ULL,4503599627333423ULL,4503599627333549ULL,4503599627333603ULL,4503599627333771ULL,4503599627333813ULL,4503599627333921ULL,4503599627333951ULL,4503599627334077ULL,4503599627334131ULL,4503599627334161ULL,4503599627334167ULL,4503599627334173ULL,4503599627334221ULL,4503599627334251ULL,4503599627334257ULL,4503599627334263ULL,4503599627334419ULL,4503599627334473ULL,4503599627334539ULL,4503599627334623ULL,4503599627334641ULL,4503599627334653ULL,4503599627334767ULL,4503599627334839ULL,4503599627334917ULL,4503599627335097ULL,4503599627335169ULL,4503599627335223ULL,4503599627335253ULL,4503599627335439ULL,4503599627335499ULL,4503599627335643ULL,4503599627335871ULL,4503599627335877ULL,4503599627335931ULL,4503599627335943ULL,4503599627335967ULL,4503599627335973ULL,4503599627335979ULL,4503599627336081ULL,4503599627336099ULL,4503599627336129ULL,4503599627336267ULL,4503599627336477ULL,4503599627336507ULL,4503599627336543ULL,4503599627336651ULL,4503599627336723ULL,4503599627336963ULL,4503599627336993ULL,4503599627337029ULL,4503599627337047ULL,4503599627337071ULL,4503599627337197ULL,4503599627337233ULL,4503599627337311ULL,4503599627337407ULL,4503599627337443ULL,4503599627337467ULL,4503599627337479ULL,4503599627337491ULL,4503599627337527ULL,4503599627337563ULL,4503599627337623ULL,4503599627337689ULL,4503599627337761ULL,4503599627338067ULL,4503599627338121ULL,4503599627338169ULL,4503599627338337ULL,4503599627338439ULL,4503599627338499ULL,4503599627338631ULL,4503599627338739ULL,4503599627338829ULL,4503599627338901ULL,4503599627338991ULL,4503599627338997ULL,4503599627339003ULL,4503599627339159ULL,4503599627339201ULL,4503599627339207ULL,4503599627339237ULL,4503599627339279ULL,4503599627339327ULL,4503599627339363ULL,4503599627339447ULL,4503599627339507ULL,4503599627339531ULL,4503599627339609ULL,4503599627339651ULL,4503599627339801ULL,4503599627339807ULL,4503599627339819ULL,4503599627339867ULL,4503599627339909ULL,4503599627339933ULL,4503599627339951ULL,4503599627339963ULL,4503599627340029ULL,4503599627340137ULL,4503599627340197ULL,4503599627340227ULL,4503599627340251ULL,4503599627340293ULL,4503599627340377ULL,4503599627340413ULL,4503599627340491ULL,4503599627340581ULL,4503599627340617ULL,4503599627340629ULL,4503599627340881ULL,4503599627340893ULL,4503599627340953ULL,4503599627340971ULL,4503599627341013ULL,4503599627341037ULL,4503599627341091ULL,4503599627341169ULL,4503599627341271ULL,4503599627341301ULL,4503599627341523ULL,4503599627341571ULL,4503599627341601ULL,4503599627341667ULL,4503599627341673ULL,4503599627341721ULL,4503599627341799ULL,4503599627341931ULL,4503599627341979ULL,4503599627342033ULL,4503599627342099ULL,4503599627342177ULL,4503599627342183ULL,4503599627342267ULL,4503599627342327ULL,4503599627342423ULL,4503599627342477ULL,4503599627342531ULL,4503599627342537ULL,4503599627342591ULL,4503599627342657ULL,4503599627342759ULL,4503599627342873ULL,4503599627342891ULL,4503599627343101ULL,4503599627343191ULL,4503599627343287ULL,4503599627343293ULL,4503599627343389ULL,4503599627343413ULL,4503599627343617ULL,4503599627343857ULL,4503599627343899ULL,4503599627343953ULL,4503599627344001ULL,4503599627344073ULL,4503599627344151ULL,4503599627344247ULL,4503599627344343ULL,4503599627344361ULL,4503599627344427ULL,4503599627344481ULL,4503599627344577ULL,4503599627344709ULL,4503599627344751ULL,4503599627344901ULL,4503599627344931ULL,4503599627344967ULL,4503599627345111ULL,4503599627345123ULL,4503599627345243ULL,4503599627345339ULL,4503599627345513ULL,4503599627345543ULL,4503599627345573ULL,4503599627345627ULL,4503599627345729ULL,4503599627345801ULL,4503599627345849ULL,4503599627345873ULL,4503599627345879ULL,4503599627345933ULL,4503599627345969ULL,4503599627346011ULL,4503599627346161ULL,4503599627346329ULL,4503599627346341ULL,4503599627346419ULL,4503599627346569ULL,4503599627346623ULL,4503599627346671ULL,4503599627346851ULL,4503599627346887ULL,4503599627346917ULL,4503599627346929ULL,4503599627346947ULL,4503599627346959ULL,4503599627347013ULL,4503599627347019ULL,4503599627347091ULL,4503599627347277ULL,4503599627347391ULL,4503599627347433ULL,4503599627347439ULL,4503599627347553ULL,4503599627347589ULL,4503599627347781ULL,4503599627347823ULL,4503599627347829ULL,4503599627347859ULL,4503599627348021ULL,4503599627348231ULL,4503599627348357ULL,4503599627348411ULL,4503599627348429ULL,4503599627348501ULL,4503599627348537ULL,4503599627348543ULL,4503599627348711ULL,4503599627348747ULL,4503599627348867ULL,4503599627348909ULL,4503599627348957ULL,4503599627349041ULL,4503599627349191ULL,4503599627349233ULL,4503599627349299ULL,4503599627349353ULL,4503599627349677ULL,4503599627349887ULL,4503599627349947ULL,4503599627350157ULL,4503599627350193ULL,4503599627350223ULL,4503599627350331ULL,4503599627350367ULL,4503599627350487ULL,4503599627350511ULL,4503599627350613ULL,4503599627350781ULL,4503599627350799ULL,4503599627350871ULL,4503599627351051ULL,4503599627351159ULL,4503599627351267ULL,4503599627351273ULL,4503599627351393ULL,4503599627351459ULL,4503599627351621ULL,4503599627351651ULL,4503599627351813ULL,4503599627351873ULL,4503599627352083ULL,4503599627352227ULL,4503599627352257ULL,4503599627352269ULL,4503599627352719ULL,4503599627352731ULL,4503599627352803ULL,4503599627352809ULL,4503599627352827ULL,4503599627352893ULL,4503599627352929ULL,4503599627352941ULL,4503599627353007ULL,4503599627353121ULL,4503599627353139ULL,4503599627353181ULL,4503599627353193ULL,4503599627353283ULL,4503599627353373ULL,4503599627353397ULL,4503599627353451ULL,4503599627353487ULL,4503599627353517ULL,4503599627353523ULL,4503599627353529ULL,4503599627353781ULL,4503599627353793ULL,4503599627353817ULL,4503599627353829ULL,4503599627353913ULL,4503599627354129ULL,4503599627354351ULL,4503599627354363ULL,4503599627354369ULL,4503599627354579ULL,4503599627354783ULL,4503599627354813ULL,4503599627355053ULL,4503599627355107ULL,4503599627355113ULL,4503599627355137ULL,4503599627355161ULL,4503599627355263ULL,4503599627355383ULL,4503599627355593ULL,4503599627355743ULL,4503599627355749ULL,4503599627355797ULL,4503599627355827ULL,4503599627355887ULL,4503599627355923ULL,4503599627355929ULL,4503599627355989ULL,4503599627356019ULL,4503599627356073ULL,4503599627356091ULL,4503599627356103ULL,4503599627356169ULL,4503599627356199ULL,4503599627356223ULL,4503599627356307ULL,4503599627356373ULL,4503599627356481ULL,4503599627356541ULL,4503599627356631ULL,4503599627356799ULL,4503599627356889ULL,4503599627356919ULL,4503599627357027ULL,4503599627357087ULL,4503599627357177ULL,4503599627357273ULL,4503599627357279ULL,4503599627357357ULL,4503599627357417ULL,4503599627357483ULL,4503599627357807ULL,4503599627357867ULL,4503599627358059ULL,4503599627358101ULL,4503599627358173ULL,4503599627358281ULL,4503599627358437ULL,4503599627358473ULL,4503599627358527ULL,4503599627358641ULL,4503599627358707ULL,4503599627358791ULL,4503599627358989ULL,4503599627359031ULL,4503599627359037ULL,4503599627359127ULL,4503599627359163ULL,4503599627359169ULL,4503599627359187ULL,4503599627359241ULL,4503599627359247ULL,4503599627359397ULL,4503599627359523ULL,4503599627359559ULL,4503599627359751ULL,4503599627359793ULL,4503599627359823ULL,4503599627359829ULL,4503599627359841ULL,4503599627359919ULL,4503599627359961ULL,4503599627359967ULL,4503599627359997ULL,4503599627360021ULL,4503599627360033ULL,4503599627360237ULL,4503599627360279ULL,4503599627360303ULL,4503599627360381ULL,4503599627360549ULL,4503599627360579ULL,4503599627360633ULL,4503599627360663ULL,4503599627360711ULL,4503599627360777ULL,4503599627360927ULL,4503599627361059ULL,4503599627361077ULL,4503599627361239ULL,4503599627361353ULL,4503599627361509ULL,4503599627361617ULL,4503599627361641ULL,4503599627361893ULL,4503599627361917ULL,4503599627361953ULL,4503599627362103ULL,4503599627362109ULL,4503599627362217ULL,4503599627362271ULL,4503599627362307ULL,4503599627362361ULL,4503599627362457ULL,4503599627362499ULL,4503599627362529ULL,4503599627362631ULL,4503599627362793ULL,4503599627362799ULL,4503599627362859ULL,4503599627363093ULL,4503599627363117ULL,4503599627363183ULL,4503599627363201ULL,4503599627363231ULL,4503599627363261ULL,4503599627363279ULL,4503599627363297ULL,4503599627363471ULL,4503599627363483ULL,4503599627363549ULL,4503599627363567ULL,4503599627363621ULL,4503599627363729ULL,4503599627363741ULL,4503599627363783ULL,4503599627363813ULL,4503599627363873ULL,4503599627363891ULL,4503599627363897ULL,4503599627363987ULL,4503599627364119ULL,4503599627364203ULL,4503599627364239ULL,4503599627364353ULL,4503599627364359ULL,4503599627364371ULL,4503599627364521ULL,4503599627364611ULL,4503599627364671ULL,4503599627364707ULL,4503599627364737ULL,4503599627364869ULL,4503599627364917ULL,4503599627364941ULL,4503599627365073ULL,4503599627365259ULL,4503599627365337ULL,4503599627365367ULL,4503599627365463ULL,4503599627365481ULL,4503599627365511ULL,4503599627365541ULL,4503599627365613ULL,4503599627365721ULL,4503599627365763ULL,4503599627365961ULL,4503599627366093ULL,4503599627366303ULL,4503599627366363ULL,4503599627366423ULL,4503599627366513ULL,4503599627366549ULL,4503599627366627ULL,4503599627366747ULL,4503599627366759ULL,4503599627366921ULL,4503599627366999ULL,4503599627367017ULL,4503599627367101ULL,4503599627367119ULL,4503599627367221ULL,4503599627367479ULL,4503599627367497ULL,4503599627367563ULL,4503599627367587ULL,4503599627367611ULL,4503599627367767ULL,4503599627367803ULL,4503599627367887ULL,4503599627367977ULL,4503599627368241ULL,4503599627368271ULL,4503599627368319ULL,4503599627368421ULL,4503599627368451ULL,4503599627368487ULL,4503599627368613ULL,4503599627368769ULL,4503599627368847ULL,4503599627368871ULL,4503599627368889ULL,4503599627368979ULL,4503599627369039ULL,4503599627369093ULL,4503599627369291ULL,4503599627369309ULL,4503599627369327ULL,4503599627369357ULL,4503599627369387ULL,4503599627369399ULL,4503599627369411ULL,4503599627369603ULL,4503599627369657ULL,4503599627369699ULL,4503599627369741ULL,4503599627369837ULL,4503599627369861ULL,4503599627369939ULL,4503599627370023ULL,4503599627370083ULL,4503599627370101ULL,4503599627370161ULL,4503599627370227ULL,4503599627370287ULL,4503599627370299ULL,4503599627370323ULL,4503599627370353ULL,4503599627370449ULL };

// This ASM code works on x64 but is slightly *slower* than the FPU trick as the
// latter avoids 128-bit divq.
#if 0
#define mulmod52(r,a,b,m) __asm__( \
	"mulq %2;" \
	"divq %3;" \
	:"=&d"(r) \
	:"a"(a),"r"(b),"r"(m));
#define cubemod52(i,m) __asm__( \
	"movq %1,%%r10;" \
	"movq %1,%%rax;" \
	"mulq %%r10;" \
	"divq %2;" \
	"mov %%rdx,%%rax;" \
	"mulq %%r10;" \
	"divq %2;" \
	:"=&d"(i) \
	:"r"(i),"r"(m) \
	:"rax","r10");
#endif

#ifdef ZT_MIMC52_NO_FPU
ZT_ALWAYS_INLINE uint64_t mulmod52(uint64_t a,uint64_t b,uint64_t m) noexcept
{
	uint64_t res = 0;
	for(;;) {
		if ((a & 1ULL))
			res = (res + b) % m;
		a >>= 1U;
		if (a) {
			b = (b << 1U) % m;
		} else {
			return res;
		}
	}
}
#else
#define mulmod52(a,b,p) (((a * b) - (((uint64_t)(((double)a * (double)b) / pp) - one) * p)) % p)
#endif

// Compute ((a ^ e) % m) with 52 bits of integer precision.
ZT_ALWAYS_INLINE uint64_t modpow52(uint64_t a,uint64_t e,const uint64_t p) noexcept
{
#ifndef ZT_MIMC52_NO_FPU
	uint64_t one = 1;
	double pp = (double)p;
#endif
	uint64_t r = 1;
	for(;;) {
		if ((e & 1ULL)) {
			r = mulmod52(r,a,p);
		}
		e >>= 1U;
		if (e) {
			a = mulmod52(a,a,p);
		} else {
			return r;
		}
	}
}

ZT_ALWAYS_INLINE void mimc52Init(uint64_t hash[6],uint64_t k[128],const void *const salt,const unsigned int saltSize,uint64_t &p,uint64_t &x) noexcept
{
	SHA384(hash,salt,saltSize);

#if __BYTE_ORDER == __LITTLE_ENDIAN
	p = s_mimc52Primes[hash[0] & 511U];
	x = hash[1] % p;
#else
	p = s_mimc52Primes[Utils::swapBytes(hash[0]) & 511U];
	x = Utils::swapBytes(hash[1]) % p;
#endif

	AES aes(hash + 2);
	uint64_t ctr[2];
	ctr[1] = 0;
	for(unsigned int i=0;i<128;i+=2) { // AES-CTR (with little-endian counter) to expand salt into 'k'
#if __BYTE_ORDER == __LITTLE_ENDIAN
		ctr[0] = (uint64_t)i;
#else
		ctr[0] = (uint64_t)i << 56U;
#endif
		aes.encrypt(ctr,k + i);
	}

#if __BYTE_ORDER != __LITTLE_ENDIAN
	for(unsigned int i=0;i<128;++i)
		k[i] = Utils::swapBytes(k[i]);
#endif
}

} // anonymous namespace

uint64_t mimc52Delay(const void *const salt,const unsigned int saltSize,const unsigned long rounds)
{
	uint64_t hash[6],k[128],x,p;
	mimc52Init(hash,k,salt,saltSize,p,x);

	const uint64_t e = ((p * 2) - 1) / 3;
	const uint64_t m52 = 0xfffffffffffffULL;
	for(unsigned long r=0,kn=rounds;r<rounds;++r) {
		x = (x - k[--kn & 127U]) & m52;
		x = modpow52(x,e,p);
	}

	return x;
}

bool mimc52Verify(const void *const salt,const unsigned int saltSize,unsigned long rounds,const uint64_t proof)
{
	uint64_t hash[6],k[128],x,p;
	mimc52Init(hash,k,salt,saltSize,p,x);

	const uint64_t m52 = 0xfffffffffffffULL;
	uint64_t y = proof & m52;
#ifndef ZT_MIMC52_NO_FPU
	double ii,of,pp = (double)p;
	uint64_t oi,one = 1;
#endif
	for(unsigned long r=0;r<rounds;++r) {
#ifdef ZT_MIMC52_NO_FPU
		y = mulmod52(mulmod52(y,y,p),y,p);
#else
		of = (double)y;
		oi = y;

		ii = of * of;
		y *= oi;
		ii /= pp;
		y -= ((uint64_t)ii - one) * p;
		//y %= p;

		ii = (double)y * of;
		y *= oi;
		ii /= pp;
		y -= ((uint64_t)ii - one) * p;
		y %= p;
#endif
		y = (y + k[r & 127U]) & m52;
	}

	return (y % p) == x;
}

} // namespace ZeroTier
