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

#include <cstring>
#include <cstdlib>

namespace ZeroTier {

/**
 * Classes inheriting from this base class are safe to abuse in C-like ways: memcpy, memset, etc.
 *
 * It also includes some static methods to do this conveniently.
 */
ZT_PACKED_STRUCT(struct TriviallyCopyable
{
	/**
	 * Be absolutely sure a TriviallyCopyable object is zeroed using Utils::burn()
	 *
	 * @tparam T Automatically inferred type of object
	 * @param obj Any TriviallyCopyable object
	 */
	template<typename T>
	static ZT_INLINE void memoryBurn(T *obj) noexcept
	{
		TriviallyCopyable *const tmp = obj;
		Utils::burn(tmp,sizeof(T));
	}

	/**
	 * Be absolutely sure a TriviallyCopyable object is zeroed using Utils::burn()
	 *
	 * @tparam T Automatically inferred type of object
	 * @param obj Any TriviallyCopyable object
	 */
	template<typename T>
	static ZT_INLINE void memoryBurn(T &obj) noexcept
	{
		TriviallyCopyable *const tmp = &obj;
		Utils::burn(tmp,sizeof(T));
	}

	/**
	 * Zero a TriviallyCopyable object
	 *
	 * @tparam T Automatically inferred type of object
	 * @param obj Any TriviallyCopyable object
	 */
	template<typename T>
	static ZT_INLINE void memoryZero(T *obj) noexcept
	{
		TriviallyCopyable *const tmp = obj;
		memset(tmp,0,sizeof(T));
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
		TriviallyCopyable *const tmp = &obj;
		memset(tmp,0,sizeof(T));
	}

	/**
	 * Copy any memory over a TriviallyCopyable object
	 *
	 * @tparam T Automatically inferred type of destination
	 * @param dest Any TriviallyCopyable object
	 * @param src Source memory of same size or less than sizeof(dest)
	 */
	template<typename T>
	static ZT_INLINE void memoryCopyUnsafe(T *dest,const void *src) noexcept
	{
		TriviallyCopyable *const tmp = dest;
		memcpy(tmp,src,sizeof(T));
	}

	/**
	 * Copy any memory over a TriviallyCopyable object
	 *
	 * @tparam T Automatically inferred type of destination
	 * @param dest Any TriviallyCopyable object
	 * @param src Source memory of same size or less than sizeof(dest)
	 */
	template<typename T>
	static ZT_INLINE void memoryCopyUnsafe(T &dest,const void *src) noexcept
	{
		TriviallyCopyable *const tmp = &dest;
		memcpy(tmp,src,sizeof(T));
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
		TriviallyCopyable *const tmp = dest;
		memcpy(tmp,src,sizeof(T));
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
		TriviallyCopyable *const tmp = dest;
		memcpy(tmp,&src,sizeof(T));
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
		TriviallyCopyable *const tmp = &dest;
		memcpy(tmp,src,sizeof(T));
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
		TriviallyCopyable *const tmp = &dest;
		memcpy(tmp,&src,sizeof(T));
	}
});

} // namespace ZeroTier

#endif
