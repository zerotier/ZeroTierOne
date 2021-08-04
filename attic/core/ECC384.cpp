/*
 * ECC384 code is based on EASY-ECC by Kenneth MacKay with only minor changes.
 *
 * It can be found in various places online such as: https://github.com/jestan/easy-ecc
 *
 * This file is under the BSD 2-clause license since that was the license under which
 * the original ECC code was distributed.
 */

#include "ECC384.hpp"

#include "Constants.hpp"
#include "Utils.hpp"

namespace ZeroTier {

namespace {

#define ECC_CURVE_BYTES             48
#define ECC_CURVE_DIGITS            (ECC_CURVE_BYTES / 8)
#define ECC_CREATE_KEY_MAX_ATTEMPTS 4096

#define vli_clear(p)       std::fill((p), (p) + ECC_CURVE_DIGITS, 0ULL)
#define vli_set(dest, src) std::copy((src), (src) + ECC_CURVE_DIGITS, (dest))
#define vli_isEven(vli)    ((vli[0] & 1ULL) == 0ULL)
#define vli_isZero(p)      std::all_of((p), (p) + ECC_CURVE_DIGITS, [](const uint64_t i) { return i == 0; })
#define vli_testBit(p, b)  ((p)[(unsigned int)(b) >> 6U] & (1ULL << ((unsigned int)(b)&63U)))

#ifndef ZT_HAVE_UINT128
struct uint128_t {
    uint64_t m_low, m_high;
};
#endif

struct EccPoint {
    uint64_t x[ECC_CURVE_DIGITS], y[ECC_CURVE_DIGITS];
};

// ECC curve NIST P-384
const uint64_t curve_p[ECC_CURVE_DIGITS] = { 0x00000000FFFFFFFF, 0xFFFFFFFF00000000, 0xFFFFFFFFFFFFFFFE, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF };
const uint64_t curve_b[ECC_CURVE_DIGITS] = { 0x2A85C8EDD3EC2AEF, 0xC656398D8A2ED19D, 0x0314088F5013875A, 0x181D9C6EFE814112, 0x988E056BE3F82D19, 0xB3312FA7E23EE7E4 };
const EccPoint curve_G                   = { { 0x3A545E3872760AB7, 0x5502F25DBF55296C, 0x59F741E082542A38, 0x6E1D3B628BA79B98, 0x8EB1C71EF320AD74, 0xAA87CA22BE8B0537 }, { 0x7A431D7C90EA0E5F, 0x0A60B1CE1D7E819D, 0xE9DA3113B5F0B8C0, 0xF8F41DBD289A147C, 0x5D9E98BF9292DC29, 0x3617DE4A96262C6F } };
const uint64_t curve_n[ECC_CURVE_DIGITS] = { 0xECEC196ACCC52973, 0x581A0DB248B0A77A, 0xC7634D81F4372DDF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF };

ZT_INLINE unsigned int vli_numBits(const uint64_t *const p_vli)
{
    int l_numDigits = ECC_CURVE_DIGITS - 1;
    for (; l_numDigits >= 0 && p_vli[l_numDigits] == 0; --l_numDigits) {}
    if (likely(l_numDigits > -1)) {
        uint64_t l_digit = p_vli[l_numDigits];
        int i            = 0;
        for (; l_digit; ++i) {
            l_digit >>= 1;
        }
        return (unsigned int)((l_numDigits * 64) + i);
    }
    return 0;
}

ZT_INLINE int vli_cmp(const uint64_t *const p_left, const uint64_t *const p_right)
{
    int comp = 0;
    for (int i = ECC_CURVE_DIGITS - 1; i >= 0; --i) {
        comp += (int)((p_left[i] > p_right[i]) && (comp == 0));   // should be constant time
        comp -= (int)((p_left[i] < p_right[i]) && (comp == 0));
    }
    return comp;
}

ZT_INLINE uint64_t vli_lshift(uint64_t *const p_result, const uint64_t *const p_in, const unsigned int p_shift)
{
    uint64_t l_carry = 0;
    for (unsigned int i = 0, p_shift2 = (64U - p_shift); i < ECC_CURVE_DIGITS; ++i) {
        uint64_t l_temp = p_in[i];
        p_result[i]     = (l_temp << p_shift) | l_carry;
        l_carry         = l_temp >> p_shift2;
    }
    return l_carry;
}

ZT_INLINE void vli_rshift1(uint64_t *p_vli)
{
    uint64_t *const l_end = p_vli, l_carry = 0;
    p_vli += ECC_CURVE_DIGITS;
    while (p_vli-- > l_end) {
        const uint64_t l_temp = *p_vli;
        *p_vli                = (l_temp >> 1U) | l_carry;
        l_carry               = l_temp << 63U;
    }
}

ZT_INLINE uint64_t vli_add(uint64_t *const p_result, const uint64_t *const p_left, const uint64_t *const p_right)
{
    uint64_t l_carry = 0;
    for (unsigned int i = 0; i < ECC_CURVE_DIGITS; ++i) {
        uint64_t l_sum = p_left[i] + p_right[i] + l_carry;
        if (l_sum != p_left[i]) {
            l_carry = (l_sum < p_left[i]);
        }
        p_result[i] = l_sum;
    }
    return l_carry;
}

ZT_INLINE uint64_t vli_sub(uint64_t *const p_result, const uint64_t *const p_left, const uint64_t *const p_right)
{
    uint64_t l_borrow = 0;
    for (unsigned int i = 0; i < ECC_CURVE_DIGITS; ++i) {
        uint64_t l_diff = p_left[i] - p_right[i] - l_borrow;
        if (l_diff != p_left[i]) {
            l_borrow = (l_diff > p_left[i]);
        }
        p_result[i] = l_diff;
    }
    return l_borrow;
}

#ifdef ZT_HAVE_UINT128

void vli_mult(uint64_t *const p_result, const uint64_t *const p_left, const uint64_t *const p_right)
{
    uint128_t r01 = 0;
    uint64_t r2   = 0;
    for (int k = 0; k < ECC_CURVE_DIGITS * 2 - 1; ++k) {
        for (int i = (k < ECC_CURVE_DIGITS ? 0 : (k + 1) - ECC_CURVE_DIGITS); i <= k && i < ECC_CURVE_DIGITS; ++i) {
            uint128_t l_product = (uint128_t)p_left[i] * p_right[k - i];
            r01 += l_product;
            r2 += (r01 < l_product);
        }
        p_result[k] = (uint64_t)r01;
        r01         = (r01 >> 64U) | (((uint128_t)r2) << 64U);
        r2          = 0;
    }
    p_result[ECC_CURVE_DIGITS * 2 - 1] = (uint64_t)r01;
}

ZT_INLINE void vli_square(uint64_t *const p_result, const uint64_t *const p_left)
{
    uint128_t r01 = 0;
    uint64_t r2   = 0;
    for (int k = 0; k < ECC_CURVE_DIGITS * 2 - 1; ++k) {
        for (int i = (k < ECC_CURVE_DIGITS ? 0 : (k + 1) - ECC_CURVE_DIGITS); i <= k && i <= k - i; ++i) {
            uint128_t l_product = (uint128_t)p_left[i] * p_left[k - i];
            if (i < (k - i)) {
                r2 += (uint64_t)(l_product >> 127U);
                l_product *= 2;
            }
            r01 += l_product;
            r2 += (r01 < l_product);
        }
        p_result[k] = (uint64_t)r01;
        r01         = (r01 >> 64U) | (((uint128_t)r2) << 64U);
        r2          = 0;
    }
    p_result[ECC_CURVE_DIGITS * 2 - 1] = (uint64_t)r01;
}

#else /* ZT_HAVE_UINT128 */

uint128_t mul_64_64(uint64_t p_left, uint64_t p_right)
{
    uint64_t a0 = p_left & 0xffffffffull, a1 = p_left >> 32, b0 = p_right & 0xffffffffull, b1 = p_right >> 32, m0 = a0 * b0, m1 = a0 * b1, m2 = a1 * b0, m3 = a1 * b1;
    uint128_t l_result;
    m2 += (m0 >> 32);
    m2 += m1;
    if (m2 < m1) {   // overflow
        m3 += 0x100000000ull;
    }
    l_result.m_low  = (m0 & 0xffffffffull) | (m2 << 32);
    l_result.m_high = m3 + (m2 >> 32);
    return l_result;
}

ZT_INLINE uint128_t add_128_128(uint128_t a, uint128_t b)
{
    uint128_t l_result;
    l_result.m_low  = a.m_low + b.m_low;
    l_result.m_high = a.m_high + b.m_high + (l_result.m_low < a.m_low);
    return l_result;
}

void vli_mult(uint64_t *const p_result, uint64_t *const p_left, const uint64_t *const p_right)
{
    uint128_t r01 = { 0, 0 };
    uint64_t r2   = 0;
    /* Compute each digit of p_result in sequence, maintaining the carries. */
    for (int k = 0; k < ECC_CURVE_DIGITS * 2 - 1; ++k) {
        for (int i = (k < ECC_CURVE_DIGITS ? 0 : (k + 1) - ECC_CURVE_DIGITS); i <= k && i < ECC_CURVE_DIGITS; ++i) {
            uint128_t l_product = mul_64_64(p_left[i], p_right[k - i]);
            r01                 = add_128_128(r01, l_product);
            r2 += (r01.m_high < l_product.m_high);
        }
        p_result[k] = r01.m_low;
        r01.m_low   = r01.m_high;
        r01.m_high  = r2;
        r2          = 0;
    }
    p_result[ECC_CURVE_DIGITS * 2 - 1] = r01.m_low;
}

ZT_INLINE void vli_square(uint64_t *const p_result, uint64_t *const p_left)
{
    uint128_t r01 = { 0, 0 };
    uint64_t r2   = 0;
    for (int k = 0; k < ECC_CURVE_DIGITS * 2 - 1; ++k) {
        for (int i = (k < ECC_CURVE_DIGITS ? 0 : (k + 1) - ECC_CURVE_DIGITS); i <= k && i <= k - i; ++i) {
            uint128_t l_product = mul_64_64(p_left[i], p_left[k - i]);
            if (i < k - i) {
                r2 += l_product.m_high >> 63;
                l_product.m_high = (l_product.m_high << 1) | (l_product.m_low >> 63);
                l_product.m_low <<= 1;
            }
            r01 = add_128_128(r01, l_product);
            r2 += (r01.m_high < l_product.m_high);
        }
        p_result[k] = r01.m_low;
        r01.m_low   = r01.m_high;
        r01.m_high  = r2;
        r2          = 0;
    }
    p_result[ECC_CURVE_DIGITS * 2 - 1] = r01.m_low;
}

#endif /* ZT_HAVE_UINT128 */

void vli_modAdd(uint64_t *const p_result, uint64_t *const p_left, const uint64_t *const p_right, const uint64_t *const p_mod)
{
    if ((vli_add(p_result, p_left, p_right) != 0ULL) || vli_cmp(p_result, p_mod) >= 0) {
        vli_sub(p_result, p_result, p_mod);
    }
}

void vli_modSub(uint64_t *const p_result, uint64_t *const p_left, const uint64_t *const p_right, const uint64_t *const p_mod)
{
    if (vli_sub(p_result, p_left, p_right) != 0ULL) {
        vli_add(p_result, p_result, p_mod);
    }
}

ZT_INLINE void omega_mult(uint64_t *const p_result, const uint64_t *const p_right)
{
    uint64_t l_tmp[ECC_CURVE_DIGITS];
    vli_set(p_result, p_right);
    uint64_t l_carry               = vli_lshift(l_tmp, p_right, 32);
    p_result[1 + ECC_CURVE_DIGITS] = l_carry + vli_add(p_result + 1, p_result + 1, l_tmp);
    p_result[2 + ECC_CURVE_DIGITS] = vli_add(p_result + 2, p_result + 2, p_right);
    uint64_t l_diff                = p_result[ECC_CURVE_DIGITS] - (l_carry + vli_sub(p_result, p_result, l_tmp));
    if (l_diff > p_result[ECC_CURVE_DIGITS]) {
        for (unsigned int i = 1 + ECC_CURVE_DIGITS;; ++i) {
            if (likely(--p_result[i] != (uint64_t)-1)) {
                break;
            }
        }
    }
    p_result[ECC_CURVE_DIGITS] = l_diff;
}

void vli_mmod_fast(uint64_t *const p_result, uint64_t *const p_product)
{
    uint64_t l_tmp[2 * ECC_CURVE_DIGITS];
    while (!vli_isZero(p_product + ECC_CURVE_DIGITS)) {
        uint64_t l_carry = 0;
        std::fill(l_tmp, l_tmp + (2 * ECC_CURVE_DIGITS), 0ULL);
        omega_mult(l_tmp, p_product + ECC_CURVE_DIGITS);
        vli_clear(p_product + ECC_CURVE_DIGITS);
        for (unsigned int i = 0; i < ECC_CURVE_DIGITS + 3; ++i) {
            uint64_t l_sum = p_product[i] + l_tmp[i] + l_carry;
            if (l_sum != p_product[i]) {
                l_carry = (l_sum < p_product[i]);
            }
            p_product[i] = l_sum;
        }
    }
    while (vli_cmp(p_product, curve_p) > 0) {
        vli_sub(p_product, p_product, curve_p);
    }
    vli_set(p_result, p_product);
}

ZT_INLINE void vli_modMult_fast(uint64_t *const p_result, uint64_t *const p_left, const uint64_t *const p_right)
{
    uint64_t l_product[2 * ECC_CURVE_DIGITS];
    vli_mult(l_product, p_left, p_right);
    vli_mmod_fast(p_result, l_product);
}

ZT_INLINE void vli_modSquare_fast(uint64_t *const p_result, uint64_t *const p_left)
{
    uint64_t l_product[2 * ECC_CURVE_DIGITS];
    vli_square(l_product, p_left);
    vli_mmod_fast(p_result, l_product);
}

void vli_modInv(uint64_t *const p_result, uint64_t *const p_input, const uint64_t *const p_mod)
{
    if (likely(!vli_isZero(p_input))) {
        uint64_t a[ECC_CURVE_DIGITS], b[ECC_CURVE_DIGITS], u[ECC_CURVE_DIGITS], v[ECC_CURVE_DIGITS], l_carry;

        vli_set(a, p_input);
        vli_set(b, p_mod);
        u[0] = 1;
        std::fill(u + 1, u + ECC_CURVE_DIGITS, 0ULL);
        vli_clear(v);

        int l_cmpResult;
        while ((l_cmpResult = vli_cmp(a, b)) != 0) {
            l_carry = 0;
            if (vli_isEven(a)) {
                vli_rshift1(a);
                if (!vli_isEven(u)) {
                    l_carry = vli_add(u, u, p_mod);
                }
                vli_rshift1(u);
                if (l_carry) {
                    u[ECC_CURVE_DIGITS - 1] |= 0x8000000000000000ULL;
                }
            }
            else if (vli_isEven(b)) {
                vli_rshift1(b);
                if (!vli_isEven(v)) {
                    l_carry = vli_add(v, v, p_mod);
                }
                vli_rshift1(v);
                if (l_carry) {
                    v[ECC_CURVE_DIGITS - 1] |= 0x8000000000000000ULL;
                }
            }
            else if (l_cmpResult > 0) {
                vli_sub(a, a, b);
                vli_rshift1(a);
                if (vli_cmp(u, v) < 0) {
                    vli_add(u, u, p_mod);
                }
                vli_sub(u, u, v);
                if (!vli_isEven(u)) {
                    l_carry = vli_add(u, u, p_mod);
                }
                vli_rshift1(u);
                if (l_carry) {
                    u[ECC_CURVE_DIGITS - 1] |= 0x8000000000000000ULL;
                }
            }
            else {
                vli_sub(b, b, a);
                vli_rshift1(b);
                if (vli_cmp(v, u) < 0) {
                    vli_add(v, v, p_mod);
                }
                vli_sub(v, v, u);
                if (!vli_isEven(v)) {
                    l_carry = vli_add(v, v, p_mod);
                }
                vli_rshift1(v);
                if (l_carry) {
                    v[ECC_CURVE_DIGITS - 1] |= 0x8000000000000000ULL;
                }
            }
        }

        vli_set(p_result, u);
    }
    else {
        vli_clear(p_result);
    }
}

ZT_INLINE bool EccPoint_isZero(const EccPoint *const p_point) { return (vli_isZero(p_point->x) && vli_isZero(p_point->y)); }

void EccPoint_double_jacobian(uint64_t *const X1, uint64_t *const Y1, uint64_t *const Z1)
{
    if (likely(!vli_isZero(Z1))) {
        uint64_t t4[ECC_CURVE_DIGITS], t5[ECC_CURVE_DIGITS];
        vli_modSquare_fast(t4, Y1);
        vli_modMult_fast(t5, X1, t4);
        vli_modSquare_fast(t4, t4);
        vli_modMult_fast(Y1, Y1, Z1);
        vli_modSquare_fast(Z1, Z1);
        vli_modAdd(X1, X1, Z1, curve_p);
        vli_modAdd(Z1, Z1, Z1, curve_p);
        vli_modSub(Z1, X1, Z1, curve_p);
        vli_modMult_fast(X1, X1, Z1);
        vli_modAdd(Z1, X1, X1, curve_p);
        vli_modAdd(X1, X1, Z1, curve_p);
        if (vli_testBit(X1, 0)) {
            const uint64_t l_carry = vli_add(X1, X1, curve_p);
            vli_rshift1(X1);
            X1[ECC_CURVE_DIGITS - 1] |= l_carry << 63U;
        }
        else {
            vli_rshift1(X1);
        }
        vli_modSquare_fast(Z1, X1);
        vli_modSub(Z1, Z1, t5, curve_p);
        vli_modSub(Z1, Z1, t5, curve_p);
        vli_modSub(t5, t5, Z1, curve_p);
        vli_modMult_fast(X1, X1, t5);
        vli_modSub(t4, X1, t4, curve_p);
        vli_set(X1, Z1);
        vli_set(Z1, Y1);
        vli_set(Y1, t4);
    }
}

ZT_INLINE void apply_z(uint64_t *const X1, uint64_t *const Y1, uint64_t *const Z)
{
    uint64_t t1[ECC_CURVE_DIGITS];
    vli_modSquare_fast(t1, Z);
    vli_modMult_fast(X1, X1, t1);
    vli_modMult_fast(t1, t1, Z);
    vli_modMult_fast(Y1, Y1, t1);
}

void XYcZ_initial_double(uint64_t *const X1, uint64_t *const Y1, uint64_t *const X2, uint64_t *const Y2, uint64_t *const p_initialZ)
{
    uint64_t z[ECC_CURVE_DIGITS];
    vli_set(X2, X1);
    vli_set(Y2, Y1);
    z[0] = 1;
    std::fill(z + 1, z + ECC_CURVE_DIGITS, 0ULL);
    z[0] = 1;
    if (p_initialZ) {
        vli_set(z, p_initialZ);
    }
    apply_z(X1, Y1, z);
    EccPoint_double_jacobian(X1, Y1, z);
    apply_z(X2, Y2, z);
}

void XYcZ_add(uint64_t *const X1, uint64_t *const Y1, uint64_t *const X2, uint64_t *const Y2)
{
    uint64_t t5[ECC_CURVE_DIGITS];
    vli_modSub(t5, X2, X1, curve_p);
    vli_modSquare_fast(t5, t5);
    vli_modMult_fast(X1, X1, t5);
    vli_modMult_fast(X2, X2, t5);
    vli_modSub(Y2, Y2, Y1, curve_p);
    vli_modSquare_fast(t5, Y2);
    vli_modSub(t5, t5, X1, curve_p);
    vli_modSub(t5, t5, X2, curve_p);
    vli_modSub(X2, X2, X1, curve_p);
    vli_modMult_fast(Y1, Y1, X2);
    vli_modSub(X2, X1, t5, curve_p);
    vli_modMult_fast(Y2, Y2, X2);
    vli_modSub(Y2, Y2, Y1, curve_p);
    vli_set(X2, t5);
}

void XYcZ_addC(uint64_t *const X1, uint64_t *const Y1, uint64_t *const X2, uint64_t *const Y2)
{
    uint64_t t5[ECC_CURVE_DIGITS], t6[ECC_CURVE_DIGITS], t7[ECC_CURVE_DIGITS];
    vli_modSub(t5, X2, X1, curve_p);
    vli_modSquare_fast(t5, t5);
    vli_modMult_fast(X1, X1, t5);
    vli_modMult_fast(X2, X2, t5);
    vli_modAdd(t5, Y2, Y1, curve_p);
    vli_modSub(Y2, Y2, Y1, curve_p);
    vli_modSub(t6, X2, X1, curve_p);
    vli_modMult_fast(Y1, Y1, t6);
    vli_modAdd(t6, X1, X2, curve_p);
    vli_modSquare_fast(X2, Y2);
    vli_modSub(X2, X2, t6, curve_p);
    vli_modSub(t7, X1, X2, curve_p);
    vli_modMult_fast(Y2, Y2, t7);
    vli_modSub(Y2, Y2, Y1, curve_p);
    vli_modSquare_fast(t7, t5);
    vli_modSub(t7, t7, t6, curve_p);
    vli_modSub(t6, t7, X1, curve_p);
    vli_modMult_fast(t6, t6, t5);
    vli_modSub(Y1, t6, Y1, curve_p);
    vli_set(X1, t7);
}

void EccPoint_mult(EccPoint *const p_result, const EccPoint *const p_point, uint64_t *const p_scalar, uint64_t *const p_initialZ)
{
    uint64_t Rx[2][ECC_CURVE_DIGITS], Ry[2][ECC_CURVE_DIGITS], z[ECC_CURVE_DIGITS];
    vli_set(Rx[1], p_point->x);
    vli_set(Ry[1], p_point->y);
    XYcZ_initial_double(Rx[1], Ry[1], Rx[0], Ry[0], p_initialZ);
    for (int i = (int)vli_numBits(p_scalar) - 2; i > 0; --i) {
        int nb = (int)!vli_testBit(p_scalar, i);
        XYcZ_addC(Rx[1 - nb], Ry[1 - nb], Rx[nb], Ry[nb]);
        XYcZ_add(Rx[nb], Ry[nb], Rx[1 - nb], Ry[1 - nb]);
    }
    int nb = (int)!vli_testBit(p_scalar, 0);
    XYcZ_addC(Rx[1 - nb], Ry[1 - nb], Rx[nb], Ry[nb]);
    vli_modSub(z, Rx[1], Rx[0], curve_p);
    vli_modMult_fast(z, z, Ry[1 - nb]);
    vli_modMult_fast(z, z, p_point->x);
    vli_modInv(z, z, curve_p);
    vli_modMult_fast(z, z, p_point->y);
    vli_modMult_fast(z, z, Rx[1 - nb]);
    XYcZ_add(Rx[nb], Ry[nb], Rx[1 - nb], Ry[1 - nb]);
    apply_z(Rx[0], Ry[0], z);
    vli_set(p_result->x, Rx[0]);
    vli_set(p_result->y, Ry[0]);
}

ZT_INLINE void ECC_CURVE_BYTES2native(uint64_t p_native[ECC_CURVE_DIGITS], const uint8_t p_bytes[ECC_CURVE_BYTES])
{
    for (unsigned int i = 0; i < ECC_CURVE_DIGITS; ++i) {
        const uint8_t *const p_digit = p_bytes + 8 * (ECC_CURVE_DIGITS - 1 - i);
        p_native[i]                  = ((uint64_t)p_digit[0] << 56) | ((uint64_t)p_digit[1] << 48) | ((uint64_t)p_digit[2] << 40) | ((uint64_t)p_digit[3] << 32) | ((uint64_t)p_digit[4] << 24) | ((uint64_t)p_digit[5] << 16) | ((uint64_t)p_digit[6] << 8) | (uint64_t)p_digit[7];
    }
}

ZT_INLINE void ecc_native2bytes(uint8_t p_bytes[ECC_CURVE_BYTES], const uint64_t p_native[ECC_CURVE_DIGITS])
{
    for (unsigned int i = 0; i < ECC_CURVE_DIGITS; ++i) {
        uint8_t *p_digit = p_bytes + 8 * (ECC_CURVE_DIGITS - 1 - i);
        p_digit[0]       = p_native[i] >> 56;
        p_digit[1]       = p_native[i] >> 48;
        p_digit[2]       = p_native[i] >> 40;
        p_digit[3]       = p_native[i] >> 32;
        p_digit[4]       = p_native[i] >> 24;
        p_digit[5]       = p_native[i] >> 16;
        p_digit[6]       = p_native[i] >> 8;
        p_digit[7]       = p_native[i];
    }
}

void mod_sqrt(uint64_t a[ECC_CURVE_DIGITS])
{
    uint64_t l_result[ECC_CURVE_DIGITS] = { 1 }, p1[ECC_CURVE_DIGITS] = { 1 };
    vli_add(p1, curve_p, p1);
    for (int i = (int)vli_numBits(p1) - 1; i > 1; --i) {
        vli_modSquare_fast(l_result, l_result);
        if (vli_testBit(p1, i)) {
            vli_modMult_fast(l_result, l_result, a);
        }
    }
    vli_set(a, l_result);
}

void ecc_point_decompress(EccPoint *p_point, const uint8_t p_compressed[ECC_CURVE_BYTES + 1])
{
    static const uint64_t _3[ECC_CURVE_DIGITS] = { 3 };
    ECC_CURVE_BYTES2native(p_point->x, p_compressed + 1);
    vli_modSquare_fast(p_point->y, p_point->x);
    vli_modSub(p_point->y, p_point->y, _3, curve_p);
    vli_modMult_fast(p_point->y, p_point->y, p_point->x);
    vli_modAdd(p_point->y, p_point->y, curve_b, curve_p);
    mod_sqrt(p_point->y);
    if ((p_point->y[0] & 0x01) != (p_compressed[0] & 0x01)) {
        vli_sub(p_point->y, curve_p, p_point->y);
    }
}

ZT_INLINE bool ecc_make_key(uint8_t p_publicKey[ECC_CURVE_BYTES + 1], uint8_t p_privateKey[ECC_CURVE_BYTES])
{
    uint64_t l_private[ECC_CURVE_DIGITS];
    EccPoint l_public;
    unsigned int l_tries = 0;
    do {
        if (unlikely(l_tries++ >= ECC_CREATE_KEY_MAX_ATTEMPTS))
            return false;
        Utils::getSecureRandom(l_private, ECC_CURVE_BYTES);
        if (likely(!vli_isZero(l_private))) {
            if (vli_cmp(curve_n, l_private) != 1)
                vli_sub(l_private, l_private, curve_n);
            EccPoint_mult(&l_public, &curve_G, l_private, NULL);
        }
    } while (EccPoint_isZero(&l_public));
    ecc_native2bytes(p_privateKey, l_private);
    ecc_native2bytes(p_publicKey + 1, l_public.x);
    p_publicKey[0] = 0x02 + (l_public.y[0] & 0x01);
    return true;
}

ZT_INLINE bool ecdh_shared_secret(const uint8_t p_publicKey[ECC_CURVE_BYTES + 1], const uint8_t p_privateKey[ECC_CURVE_BYTES], uint8_t p_secret[ECC_CURVE_BYTES])
{
    EccPoint l_public;
    uint64_t l_private[ECC_CURVE_DIGITS];
    uint64_t l_random[ECC_CURVE_DIGITS];
    Utils::getSecureRandom(l_random, ECC_CURVE_BYTES);
    ecc_point_decompress(&l_public, p_publicKey);
    ECC_CURVE_BYTES2native(l_private, p_privateKey);
    EccPoint l_product;
    EccPoint_mult(&l_product, &l_public, l_private, l_random);
    ecc_native2bytes(p_secret, l_product.x);
    return !EccPoint_isZero(&l_product);
}

void vli_modMult(uint64_t *const p_result, uint64_t *const p_left, uint64_t *const p_right, const uint64_t *const p_mod)
{
    uint64_t l_product[2 * ECC_CURVE_DIGITS], l_modMultiple[2 * ECC_CURVE_DIGITS];
    unsigned int l_digitShift, l_bitShift, l_productBits, l_modBits = vli_numBits(p_mod);
    vli_mult(l_product, p_left, p_right);
    l_productBits = vli_numBits(l_product + ECC_CURVE_DIGITS);
    if (l_productBits) {
        l_productBits += ECC_CURVE_DIGITS * 64;
    }
    else {
        l_productBits = vli_numBits(l_product);
    }

    if (l_productBits < l_modBits) {
        vli_set(p_result, l_product);
        return;
    }
    vli_clear(l_modMultiple);
    vli_clear(l_modMultiple + ECC_CURVE_DIGITS);
    l_digitShift = (l_productBits - l_modBits) / 64;
    l_bitShift   = (l_productBits - l_modBits) % 64;
    if (l_bitShift) {
        l_modMultiple[l_digitShift + ECC_CURVE_DIGITS] = vli_lshift(l_modMultiple + l_digitShift, p_mod, l_bitShift);
    }
    else {
        vli_set(l_modMultiple + l_digitShift, p_mod);
    }
    vli_clear(p_result);
    p_result[0] = 1;
    while (l_productBits > ECC_CURVE_DIGITS * 64 || vli_cmp(l_modMultiple, p_mod) >= 0) {
        int l_cmp = vli_cmp(l_modMultiple + ECC_CURVE_DIGITS, l_product + ECC_CURVE_DIGITS);
        if (l_cmp < 0 || (l_cmp == 0 && vli_cmp(l_modMultiple, l_product) <= 0)) {
            if (vli_sub(l_product, l_product, l_modMultiple)) { /* borrow */
                vli_sub(l_product + ECC_CURVE_DIGITS, l_product + ECC_CURVE_DIGITS, p_result);
            }
            vli_sub(l_product + ECC_CURVE_DIGITS, l_product + ECC_CURVE_DIGITS, l_modMultiple + ECC_CURVE_DIGITS);
        }
        uint64_t l_carry = (l_modMultiple[ECC_CURVE_DIGITS] & 0x01) << 63;
        vli_rshift1(l_modMultiple + ECC_CURVE_DIGITS);
        vli_rshift1(l_modMultiple);
        l_modMultiple[ECC_CURVE_DIGITS - 1] |= l_carry;
        --l_productBits;
    }
    vli_set(p_result, l_product);
}

ZT_INLINE bool ecdsa_sign(const uint8_t p_privateKey[ECC_CURVE_BYTES], const uint8_t p_hash[ECC_CURVE_BYTES], uint8_t p_signature[ECC_CURVE_BYTES * 2])
{
    uint64_t k[ECC_CURVE_DIGITS], l_tmp[ECC_CURVE_DIGITS], l_s[ECC_CURVE_DIGITS];
    EccPoint p;
    unsigned int l_tries = 0;
    do {
        if (unlikely(l_tries++ >= ECC_CREATE_KEY_MAX_ATTEMPTS)) {
            return false;
        }
        Utils::getSecureRandom(k, ECC_CURVE_BYTES);
        if (likely(!vli_isZero(k))) {
            if (vli_cmp(curve_n, k) != 1) {
                vli_sub(k, k, curve_n);
            }
            EccPoint_mult(&p, &curve_G, k, NULL);
            if (vli_cmp(curve_n, p.x) != 1) {
                vli_sub(p.x, p.x, curve_n);
            }
        }
    } while (vli_isZero(p.x));
    ecc_native2bytes(p_signature, p.x);
    ECC_CURVE_BYTES2native(l_tmp, p_privateKey);
    vli_modMult(l_s, p.x, l_tmp, curve_n); /* s = r*d */
    ECC_CURVE_BYTES2native(l_tmp, p_hash);
    vli_modAdd(l_s, l_tmp, l_s, curve_n); /* s = e + r*d */
    vli_modInv(k, k, curve_n);            /* k = 1 / k */
    vli_modMult(l_s, l_s, k, curve_n);    /* s = (e + r*d) / k */
    ecc_native2bytes(p_signature + ECC_CURVE_BYTES, l_s);
    return true;
}

ZT_INLINE bool ecdsa_verify(const uint8_t p_publicKey[ECC_CURVE_BYTES + 1], const uint8_t p_hash[ECC_CURVE_BYTES], const uint8_t p_signature[ECC_CURVE_BYTES * 2])
{
    uint64_t u1[ECC_CURVE_DIGITS], u2[ECC_CURVE_DIGITS], z[ECC_CURVE_DIGITS], rx[ECC_CURVE_DIGITS], ry[ECC_CURVE_DIGITS], tx[ECC_CURVE_DIGITS], ty[ECC_CURVE_DIGITS], tz[ECC_CURVE_DIGITS], l_r[ECC_CURVE_DIGITS], l_s[ECC_CURVE_DIGITS];
    EccPoint l_public, l_sum;

    ecc_point_decompress(&l_public, p_publicKey);
    ECC_CURVE_BYTES2native(l_r, p_signature);
    ECC_CURVE_BYTES2native(l_s, p_signature + ECC_CURVE_BYTES);

    if (unlikely(vli_isZero(l_r) || vli_isZero(l_s) || (vli_cmp(curve_n, l_r) != 1) || (vli_cmp(curve_n, l_s) != 1))) {
        return false;
    }

    vli_modInv(z, l_s, curve_n);
    ECC_CURVE_BYTES2native(u1, p_hash);
    vli_modMult(u1, u1, z, curve_n);
    vli_modMult(u2, l_r, z, curve_n);
    vli_set(l_sum.x, l_public.x);
    vli_set(l_sum.y, l_public.y);
    vli_set(tx, curve_G.x);
    vli_set(ty, curve_G.y);
    vli_modSub(z, l_sum.x, tx, curve_p);
    XYcZ_add(tx, ty, l_sum.x, l_sum.y);
    vli_modInv(z, z, curve_p);
    apply_z(l_sum.x, l_sum.y, z);
    const EccPoint *const l_points[4] = { NULL, &curve_G, &l_public, &l_sum };
    unsigned int l_numBits            = std::max(vli_numBits(u1), vli_numBits(u2));
    const EccPoint *const l_point     = l_points[(!!vli_testBit(u1, l_numBits - 1)) | ((!!vli_testBit(u2, l_numBits - 1)) << 1)];
    vli_set(rx, l_point->x);
    vli_set(ry, l_point->y);
    vli_clear(z);
    z[0] = 1;

    for (int i = l_numBits - 2; i >= 0; --i) {
        EccPoint_double_jacobian(rx, ry, z);
        int l_index                    = (!!vli_testBit(u1, i)) | ((!!vli_testBit(u2, i)) << 1);
        const EccPoint *const l_point2 = l_points[l_index];
        if (l_point2) {
            vli_set(tx, l_point2->x);
            vli_set(ty, l_point2->y);
            apply_z(tx, ty, z);
            vli_modSub(tz, rx, tx, curve_p);
            XYcZ_add(tx, ty, rx, ry);
            vli_modMult_fast(z, z, tz);
        }
    }

    vli_modInv(z, z, curve_p);
    apply_z(rx, ry, z);

    if (vli_cmp(curve_n, rx) != 1) {
        vli_sub(rx, rx, curve_n);
    }

    return (vli_cmp(rx, l_r) == 0);
}

}   // anonymous namespace

void ECC384GenerateKey(uint8_t pub[ZT_ECC384_PUBLIC_KEY_SIZE], uint8_t priv[ZT_ECC384_PRIVATE_KEY_SIZE])
{
    if (unlikely(!ecc_make_key(pub, priv))) {
        fprintf(stderr, "FATAL: ecdsa_make_key() failed!" ZT_EOL_S);
        abort();
    }
}

void ECC384ECDSASign(const uint8_t priv[ZT_ECC384_PRIVATE_KEY_SIZE], const uint8_t hash[ZT_ECC384_SIGNATURE_HASH_SIZE], uint8_t sig[ZT_ECC384_SIGNATURE_SIZE])
{
    if (unlikely(!ecdsa_sign(priv, hash, sig))) {
        fprintf(stderr, "FATAL: ecdsa_sign() failed!" ZT_EOL_S);
        abort();
    }
}

bool ECC384ECDSAVerify(const uint8_t pub[ZT_ECC384_PUBLIC_KEY_SIZE], const uint8_t hash[ZT_ECC384_SIGNATURE_HASH_SIZE], const uint8_t sig[ZT_ECC384_SIGNATURE_SIZE]) { return (ecdsa_verify(pub, hash, sig) != 0); }

bool ECC384ECDH(const uint8_t theirPub[ZT_ECC384_PUBLIC_KEY_SIZE], const uint8_t ourPriv[ZT_ECC384_PRIVATE_KEY_SIZE], uint8_t secret[ZT_ECC384_SHARED_SECRET_SIZE]) { return (ecdh_shared_secret(theirPub, ourPriv, secret) != 0); }

}   // namespace ZeroTier
