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

#ifndef ZT_BP_HPP
#define ZT_BP_HPP

#include "Constants.hpp"

namespace ZeroTier {

/**
 * A C-like pointer to a byte array with static bounds checking whenever possible.
 *
 * This fills the niche in between Buf and a naked C pointer and behaves like a C pointer
 * annotated with a length that is statically checked at compile time. It should have no
 * runtime overhead at all.
 *
 * - Explicit or implicit casts are supported to pointers to smaller but not larger arrays
 * - A templated slice() method (analogous to Go slices) allows getting subset arrays
 * - A templated at() method allows bounds checked access to indices known at compile time
 * - Copy and assignment from larger or equal sized arrays, but not smaller
 * - NULL pointers are explicitly not allowed, so a naked pointer must still be used in that case
 *
 * Note that the [] operator for dynamic access is NOT bounds checked as this would add
 * runtime overhead that is not desirable where this is used. The goal here is to keep
 * C performance while being safer to the extent that C++ allows.
 *
 * @tparam S Size of memory to which this pointer must point
 */
template<unsigned int S>
class BP
{
public:
	typedef uint8_t * iterator;
	typedef const uint8_t * const_iterator;

	explicit ZT_INLINE BP(void *const bytes) : p(reinterpret_cast<uint8_t *>(bytes)) {}

	template<unsigned int CS>
	ZT_INLINE BP(BP<CS> &b) noexcept :
		p(b.p)
	{
		static_assert(CS <= S,"attempt to copy byte pointer from one of smaller size");
	}

	template<unsigned int CS>
	ZT_INLINE BP &operator=(BP<CS> &b)
	{
		static_assert(CS <= S,"attempt to assign byte pointer from one of smaller size");
		p = b.p;
		return *this;
	}

	template<unsigned int CS>
	ZT_INLINE operator BP<CS>() noexcept // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)
	{
		static_assert(CS <= S,"attempt to cast byte pointer to larger size");
		return BP<CS>(p);
	}

	ZT_INLINE uint8_t &operator[](const unsigned int i) noexcept
	{
		return p[i];
	}
	ZT_INLINE uint8_t operator[](const unsigned int i) const noexcept
	{
		return p[i];
	}
	template<typename I>
	ZT_INLINE uint8_t *operator+(const I i) noexcept
	{
		return p + i;
	}
	template<typename I>
	ZT_INLINE const uint8_t *operator+(const I i) const noexcept
	{
		return p + i;
	}

	ZT_INLINE uint8_t &operator*() noexcept
	{
		static_assert(S > 0,"attempt to access empty array");
		return *p;
	}
	ZT_INLINE uint8_t operator*() const noexcept
	{
		static_assert(S > 0,"attempt to access empty array");
		return *p;
	}

	template<unsigned int I>
	ZT_INLINE uint8_t &at() noexcept
	{
		static_assert(I < S,"static access beyond end of byte pointer");
		return p[I];
	}
	template<unsigned int I>
	ZT_INLINE uint8_t at() const noexcept
	{
		static_assert(I < S,"static access beyond end of byte pointer");
		return p[I];
	}

	template<unsigned int RS,unsigned int RE>
	ZT_INLINE BP<RE - RS> &slice() noexcept
	{
		static_assert(RE > RS,"slice must end after it begins");
		static_assert(RE <= S,"slice ends after byte array end");
		return reinterpret_cast< BP<RE - RS> >(*this);
	}

	template<unsigned int RS,unsigned int RE>
	ZT_INLINE const BP<RE - RS> &slice() const noexcept
	{
		static_assert(RE > RS,"slice must end after it begins");
		static_assert(RE <= S,"slice ends after byte array end");
		return reinterpret_cast< BP<RE - RS> >(*this);
	}

	template<unsigned int F>
	ZT_INLINE BP<S - F> &from() noexcept
	{
		static_assert(F < S,"attempt to get array from beyond bounds");
		return reinterpret_cast< BP<S - F> >(*this);
	}
	template<unsigned int F>
	ZT_INLINE const BP<S - F> &from() const noexcept
	{
		static_assert(F < S,"attempt to get array from beyond bounds");
		return reinterpret_cast< BP<S - F> >(*this);
	}

	ZT_INLINE iterator begin() noexcept { return p; }
	ZT_INLINE iterator end() noexcept { return p + S; }
	ZT_INLINE const_iterator begin() const noexcept { return p; }
	ZT_INLINE const_iterator end() const noexcept { return p + S; }

	static constexpr unsigned int size() noexcept { return S; }

	template<unsigned int CS>
	ZT_INLINE bool operator==(const BP<CS> &b) const noexcept { return p == b.p; }
	template<unsigned int CS>
	ZT_INLINE bool operator!=(const BP<CS> &b) const noexcept { return p != b.p; }
	template<unsigned int CS>
	ZT_INLINE bool operator<(const BP<CS> &b) const noexcept { return p < b.p; }
	template<unsigned int CS>
	ZT_INLINE bool operator>(const BP<CS> &b) const noexcept { return p > b.p; }
	template<unsigned int CS>
	ZT_INLINE bool operator<=(const BP<CS> &b) const noexcept { return p <= b.p; }
	template<unsigned int CS>
	ZT_INLINE bool operator>=(const BP<CS> &b) const noexcept { return p >= b.p; }

	ZT_INLINE bool operator==(const void *const b) const noexcept { return p == reinterpret_cast<const uint8_t *>(b); }
	ZT_INLINE bool operator!=(const void *const b) const noexcept { return p != reinterpret_cast<const uint8_t *>(b); }
	ZT_INLINE bool operator<(const void *const b) const noexcept { return p < reinterpret_cast<const uint8_t *>(b); }
	ZT_INLINE bool operator>(const void *const b) const noexcept { return p > reinterpret_cast<const uint8_t *>(b); }
	ZT_INLINE bool operator<=(const void *const b) const noexcept { return p <= reinterpret_cast<const uint8_t *>(b); }
	ZT_INLINE bool operator>=(const void *const b) const noexcept { return p >= reinterpret_cast<const uint8_t *>(b); }

private:
	uint8_t *const p;
};

} // namespace ZeroTier

#endif
