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

#ifndef ZT_TRIVIALLYCOPYABLE_HPP
#define ZT_TRIVIALLYCOPYABLE_HPP

#include "Constants.hpp"
#include "Utils.hpp"

namespace ZeroTier {

/**
 * Classes inheriting from this base class are safe to abuse in C-like ways.
 *
 * It also includes some static methods to do this conveniently.
 */
struct TriviallyCopyable
{
public:
	/**
	 * Zero a TriviallyCopyable object
	 *
	 * @tparam T Automatically inferred type of object
	 * @param obj Any TriviallyCopyable object
	 */
	template<typename T>
	static ZT_INLINE void memoryZero(T *obj) noexcept
	{
		static_assert(isTriviallyCopyable(obj),"parameter is not TriviallyCopyable");
		Utils::zero<sizeof(T)>(obj);
	}

	/**
	 * Zero a TriviallyCopyable object
	 *
	 * @tparam T Automatically inferred type of object
	 * @param obj Any TriviallyCopyable object
	 */
	template<typename T>
	static ZT_INLINE void memoryZero(T &obj) noexcept
	{
		static_assert(isTriviallyCopyable(obj),"parameter is not TriviallyCopyable");
		Utils::zero<sizeof(T)>(&obj);
	}

	/**
	 * Copy a TriviallyCopyable object
	 *
	 * @tparam T Automatically inferred type of destination
	 * @param dest Destination TriviallyCopyable object
	 * @param src Source TriviallyCopyable object
	 */
	template<typename T>
	static ZT_INLINE void memoryCopy(T *dest,const T *src) noexcept
	{
		static_assert(isTriviallyCopyable(dest),"parameter is not TriviallyCopyable");
		Utils::copy<sizeof(T)>(dest,src);
	}

	/**
	 * Copy a TriviallyCopyable object
	 *
	 * @tparam T Automatically inferred type of destination
	 * @param dest Destination TriviallyCopyable object
	 * @param src Source TriviallyCopyable object
	 */
	template<typename T>
	static ZT_INLINE void memoryCopy(T *dest,const T &src) noexcept
	{
		static_assert(isTriviallyCopyable(src),"parameter is not TriviallyCopyable");
		Utils::copy<sizeof(T)>(dest,&src);
	}

	/**
	 * Copy a TriviallyCopyable object
	 *
	 * @tparam T Automatically inferred type of destination
	 * @param dest Destination TriviallyCopyable object
	 * @param src Source TriviallyCopyable object
	 */
	template<typename T>
	static ZT_INLINE void memoryCopy(T &dest,const T *src) noexcept
	{
		static_assert(isTriviallyCopyable(dest),"parameter is not TriviallyCopyable");
		Utils::copy<sizeof(T)>(&dest,src);
	}

	/**
	 * Copy a TriviallyCopyable object
	 *
	 * @tparam T Automatically inferred type of destination
	 * @param dest Destination TriviallyCopyable object
	 * @param src Source TriviallyCopyable object
	 */
	template<typename T>
	static ZT_INLINE void memoryCopy(T &dest,const T &src) noexcept
	{
		static_assert(isTriviallyCopyable(dest),"parameter is not TriviallyCopyable");
		Utils::copy<sizeof(T)>(&dest,&src);
	}
};

static constexpr bool isTriviallyCopyable(const TriviallyCopyable *) noexcept { return true; }
static constexpr bool isTriviallyCopyable(const void *) noexcept { return false; }

template<typename T>
static constexpr bool isTriviallyCopyable(const T &anything) noexcept { return isTriviallyCopyable(&anything); }

} // namespace ZeroTier

#endif
