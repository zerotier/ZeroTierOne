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

#ifndef ZT_MAP_HPP
#define ZT_MAP_HPP

/*
 * This wraps std::unordered_map (or std::map if that is not available) and gives
 * it a few extra methods. It also uses the built-in hashCode methods in key objects
 * in ZeroTier instead of requiring hashers all over the place.
 */

#include "Constants.hpp"
#include "Utils.hpp"

#ifdef __CPP11__
#include <unordered_map>
#else
#include <map>
#endif

namespace ZeroTier {

#ifdef __CPP11__

struct _MapHasher
{
	template<typename O>
	std::size_t operator()(const O &obj) const noexcept { return (std::size_t)obj.hashCode() ^ (std::size_t)Utils::s_mapNonce; }

	std::size_t operator()(const uint64_t i) const noexcept { return (std::size_t)Utils::hash64(i ^ Utils::s_mapNonce); }
	std::size_t operator()(const int64_t i) const noexcept { return (std::size_t)Utils::hash64((uint64_t)i ^ Utils::s_mapNonce); }
	std::size_t operator()(const uint32_t i) const noexcept { return (std::size_t)Utils::hash32(i ^ (uint32_t)Utils::s_mapNonce); }
	std::size_t operator()(const int32_t i) const noexcept { return (std::size_t)Utils::hash32((uint32_t)i ^ (uint32_t)Utils::s_mapNonce); }
};

template<typename K,typename V>
class Map : public std::unordered_map<K,V,_MapHasher>
{
public:
	ZT_INLINE V *get(const K &key) noexcept
	{
		typename Map::iterator i(this->find(key));
		if (i == this->end())
			return nullptr;
		return &(i->second);
	}

	ZT_INLINE const V *get(const K &key) const noexcept
	{
		typename Map::const_iterator i(this->find(key));
		if (i == this->end())
			return nullptr;
		return &(i->second);
	}

	ZT_INLINE void set(const K &key,const V &value)
	{
		this->emplace(key,value);
	}
};

#else

template<typename K,typename V>
class Map : public std::map<K,V>
{
public:
	ZT_INLINE V *get(const K &key) noexcept
	{
		typename Map::iterator i(this->find(key));
		if (i == this->end())
			return nullptr;
		return &(i->second);
	}

	ZT_INLINE const V *get(const K &key) const noexcept
	{
		typename Map::const_iterator i(this->find(key));
		if (i == this->end())
			return nullptr;
		return &(i->second);
	}

	ZT_INLINE void set(const K &key,const V &value)
	{
		(*this)[key] = value;
	}
};

#endif

} // ZeroTier

#endif
