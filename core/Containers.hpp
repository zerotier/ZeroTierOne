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

#ifndef ZT_CONTAINERS_HPP
#define ZT_CONTAINERS_HPP

/* This defines a Map, SortedMap, Vector, etc. based on STL templates. */

#include "Constants.hpp"
#include "Utils.hpp"

#include <map>
#include <vector>
#include <list>
#include <set>
#include <string>

#ifdef __CPP11__
#include <atomic>
#include <unordered_map>
#include <forward_list>
#endif

namespace ZeroTier {

#ifdef __CPP11__

struct intl_MapHasher
{
	template< typename O >
	std::size_t operator()(const O &obj) const noexcept
	{ return (std::size_t)obj.hashCode(); }
	std::size_t operator()(const uint64_t i) const noexcept
	{ return (std::size_t)Utils::hash64(i + Utils::s_mapNonce); }
	std::size_t operator()(const int64_t i) const noexcept
	{ return (std::size_t)Utils::hash64((uint64_t)i + Utils::s_mapNonce); }
	std::size_t operator()(const uint32_t i) const noexcept
	{ return (std::size_t)Utils::hash32(i + (uint32_t)Utils::s_mapNonce); }
	std::size_t operator()(const int32_t i) const noexcept
	{ return (std::size_t)Utils::hash32((uint32_t)i + (uint32_t)Utils::s_mapNonce); }
};

template< typename K, typename V >
class Map : public std::unordered_map< K, V, intl_MapHasher >
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
	ZT_INLINE void set(const K &key, const V &value) { this->emplace(key, value); }
};

template< typename K, typename V >
class MultiMap : public std::unordered_multimap< K, V, intl_MapHasher, std::equal_to< K > >
{};

#else

template<typename K,typename V>
class Map : public std::map< K,V,std::less<K> >
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
	{ (*this)[key] = value; }
};

template<typename K,typename V>
class MultiMap : public std::multimap< K,V,std::less<K>,Utils::Mallocator< std::pair<const K,V> > >
{
};

#endif

template< typename K, typename V >
class SortedMap : public std::map< K, V >
{};

template< typename V >
class Vector : public std::vector< V >
{
public:
	ZT_INLINE Vector()
	{}

	template< typename I >
	ZT_INLINE Vector(I begin,I end) :
		std::vector< V >(begin, end)
	{}
};

template< typename V >
class List : public std::list< V >
{};

#ifdef __CPP11__

template< typename V >
class ForwardList : public std::forward_list< V >
{};

#else

template< typename V >
class ForwardList : public std::list< V >
{};

#endif

template< typename V >
class Set : public std::set< V, std::less< V > >
{};

typedef std::string String;

} // ZeroTier

#endif
