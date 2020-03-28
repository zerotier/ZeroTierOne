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

#ifndef ZT_FLATMAP_HPP
#define ZT_FLATMAP_HPP

#include "Constants.hpp"
#include "Utils.hpp"

#include <list>
#include <vector>
#include <utility>

namespace ZeroTier {

/**
 * A very simple and fast flat hash table
 *
 * This is designed to prioritize performance over memory, though with
 * endpoint ZeroTier nodes this is used in cases where the data set is not
 * that large (e.g. networks, peers) and so memory consumption should not
 * be much.
 *
 * It creates a flat hash table that maps keys to values and doubles its
 * size whenever a collision occurs. The size is always a power of two so
 * that only very fast operations like add, XOR, and bit mask can be used
 * for hash table lookup.
 *
 * It also saves actual key/value pairs in a linked list for relatively
 * fast iteration compared to a hash table where you have to iterate through
 * a bunch of empty buckets. Iteration order should be considered undefined.
 *
 * @tparam K Key type
 * @tparam V Value type
 * @tparam IC Initial capacity (must be a power of two)
 */
template<typename K,typename V,unsigned long IC = 16>
class FlatMap
{
public:
	typedef K key_type;
	typedef V mapped_type;
	typedef std::pair<K,V> value_type;
	typedef typename std::list< std::pair<K,V> >::iterator iterator;
	typedef typename std::list< std::pair<K,V> >::const_iterator const_iterator;

	ZT_INLINE FlatMap() :
		_d(),
		_null(),
		_b(new iterator[IC]),
		_s(IC),
		_m(IC - 1)
	{
	}

	ZT_INLINE FlatMap(const FlatMap &m) :
		_d(m._d),
		_null(),
		_b(new iterator[m._s]),
		_s(m._s),
		_m(m._m)
	{
		try {
			for (iterator i=_d.begin();i!=_d.end();++i)
				_b[_hc(i->first) & _m] = i;
		} catch ( ... ) {
			delete [] _b;
			throw;
		}
	}

	ZT_INLINE ~FlatMap()
	{
		delete [] _b;
	}

	ZT_INLINE FlatMap &operator=(const FlatMap &m)
	{
		_d = m._d;
		delete [] _b;
		_b = nullptr;
		_b = new iterator[m._s];
		_s = m._s;
		_m = m._m;
		for(iterator i=_d.begin();i!=_d.end();++i)
			_b[_hc(i->first) & _m] = i;
	}

	ZT_INLINE void clear()
	{
		_d.clear();
		delete [] _b;
		_b = nullptr;
		_b = new iterator[IC];
		_s = IC;
		_m = IC - 1;
	}

	ZT_INLINE V *get(const K &key) noexcept
	{
		const unsigned long hb = _hc(key) & _m;
		if ((_b[hb] != _null)&&(_b[hb]->first == key))
			return &(_b[hb]->second);
		return nullptr;
	}

	ZT_INLINE const V *get(const K &key) const noexcept
	{
		const unsigned long hb = _hc(key) & _m;
		if ((_b[hb] != _null)&&(_b[hb]->first == key))
			return &(_b[hb]->second);
		return nullptr;
	}

	ZT_INLINE iterator find(const K &key) noexcept
	{
		const unsigned long hb = _hc(key) & _m;
		if ((_b[hb] != _null)&&(_b[hb]->first == key))
			return _b[hb];
		return _d.end();
	}

	ZT_INLINE const_iterator find(const K &key) const noexcept
	{
		const unsigned long hb = _hc(key) & _m;
		if ((_b[hb] != _null)&&(_b[hb]->first == key))
			return _b[hb];
		return _d.end();
	}

	ZT_INLINE V &set(const K &key,const V &value)
	{
		unsigned long hb = _hc(key) & _m;
		if (_b[hb] == _null) {
#ifdef __CPP11__
			_d.emplace_back(key,value);
#else
			_d.push_back(std::pair<K,V>(key,value));
#endif
			return (_b[hb] = --_d.end())->second;
		} else {
			if (_b[hb]->first == key) {
				_b[hb]->second = value;
				return _b[hb]->second;
			}
#ifdef __CPP11__
			_d.emplace_back(key,value);
#else
			_d.push_back(std::pair<K,V>(key,value));
#endif
			_grow();
			return _b[_hc(key) & _m]->second;
		}
	}

	ZT_INLINE V &operator[](const K &key)
	{
		unsigned long hb = _hc(key) & _m;
		if (_b[hb] == _null) {
#ifdef __CPP11__
			_d.emplace_back(key,V());
#else
			_d.push_back(std::pair<K,V>(key,V()));
#endif
			return (_b[hb] = --_d.end())->second;
		} else {
			if (_b[hb]->first == key)
				return _b[hb]->second;
#ifdef __CPP11__
			_d.emplace_back(key,V());
#else
			_d.push_back(std::pair<K,V>(key,V()));
#endif
			_grow();
			return _b[_hc(key) & _m]->second;
		}
	}

	ZT_INLINE void erase(const iterator &i)
	{
		_b[_hc(i->first) & _m] = _null;
		_d.erase(i);
	}

	ZT_INLINE void erase(const K &key)
	{
		iterator e(find(key));
		if (e != end())
			erase(e);
	}

	ZT_INLINE iterator begin() noexcept { return _d.begin(); }
	ZT_INLINE iterator end() noexcept { return _d.end(); }
	ZT_INLINE const_iterator begin() const noexcept { return _d.begin(); }
	ZT_INLINE const_iterator end() const noexcept { return _d.end(); }

	ZT_INLINE unsigned long size() const { return (unsigned long)_d.size(); }
	ZT_INLINE bool empty() const { return _d.empty(); }
	ZT_INLINE unsigned long hashSize() const { return _s; }

private:
	template<typename O>
	static ZT_INLINE unsigned long _hc(const O &obj) { return (unsigned long)obj.hashCode(); }
	static ZT_INLINE unsigned long _hc(const uint64_t i) noexcept { return (unsigned long)(i + (i >> 32U)); }
	static ZT_INLINE unsigned long _hc(const int64_t i) noexcept { return (unsigned long)((uint64_t)i + ((uint64_t)i >> 32U)); }
	static ZT_INLINE unsigned long _hc(const uint32_t i) noexcept { return Utils::hash32(i); }

	ZT_INLINE void _grow()
	{
		unsigned long hb;
enlarge_again:
		delete [] _b;
		_b = nullptr; // in case 'new' throws
		_b = new iterator[_s <<= 1U];
		_m = _s - 1;
		for(iterator i=_d.begin();i!=_d.end();++i) {
			hb = _hc(i->first) & _m;
			if (_b[hb] == _null) {
				_b[hb] = i;
			} else {
				goto enlarge_again;
			}
		}
	}

	std::list< std::pair<K,V> > _d;
	const iterator _null; // prototype of a "null" / default iterator
	iterator *_b;
	unsigned long _s;
	unsigned long _m;
};

} // namespace ZeroTier

#endif
