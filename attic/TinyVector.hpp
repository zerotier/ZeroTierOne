/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_TINYVECTOR_HPP
#define ZT_TINYVECTOR_HPP

#include "Constants.hpp"
#include "Utils.hpp"

#include <utility>
#include <stdexcept>
#include <algorithm>

namespace ZeroTier {

/**
 * Tiny vector with a static base capacity for allocation-free operation at small sizes
 *
 * This doesn't support all of std::vector, uses low-level memcpy to relocate things, and
 * lacks bounds checking. It's only intended for uses where a minimal subset of the vector
 * container is needed, the objects are primitive or safe to handle in this way, and the
 * number of items is typically less than or equal to some statically definable value.
 *
 * Examples of safe objects for this include primitive types, Str, SharedPtr, InetAddress,
 * Address, MAC, etc.
 *
 * @tparam T Type to encapsulate
 * @tparam BASE Base number of items to allocate storage inside the object itself (default: 4)
 */
template<typename T,unsigned long BASE = 4>
class TinyVector
{
public:
	typedef unsigned long size_t;
	typedef T * iterator;
	typedef const T * const_iterator;
	typedef T & reference;
	typedef const T & const_reference;

	ZT_ALWAYS_INLINE TinyVector() :
		_v((void *)_baseMem),
		_c(BASE),
		_l(0)
	{
	}

	ZT_ALWAYS_INLINE TinyVector(const TinyVector &vec) :
		_v((void *)_baseMem),
		_c(BASE),
		_l(0)
	{
		*this = vec;
	}

	ZT_ALWAYS_INLINE ~TinyVector()
	{
		clear();
		if (_v != (void *)_baseMem)
			free(_v);
	}

	ZT_ALWAYS_INLINE TinyVector &operator=(const TinyVector &vec)
	{
		unsigned long i = 0;
		if (_l < vec._l) {
			while (i < _l) {
				reinterpret_cast<T *>(_v)[i] = reinterpret_cast<const T *>(vec._v)[i];
				++i;
			}

			if (vec._l > _c) {
				unsigned long nc = vec._c;
				void *nv;
				if (_v == (void *)_baseMem) {
					nv = malloc(nc);
					memcpy(nv,_v,sizeof(T) * _l);
				} else {
					nv = realloc(_v,nc);
					if (!nv)
						throw std::bad_alloc();
				}
				_v = nv;
				_c = nc;
			}

			while (i < vec._l) {
				new (reinterpret_cast<T *>(_v) + i) T(reinterpret_cast<const T *>(vec._v)[i]);
				++i;
			}
		} else {
			while (i < vec._l) {
				reinterpret_cast<T *>(_v)[i] = reinterpret_cast<const T *>(vec._v)[i];
				++i;
			}
			if (!Utils::isPrimitiveType<T>()) {
				while (i < _l)
					reinterpret_cast<T *>(_v)[i++]->~T();
			}
		}
		_l = vec._l;
	}

	ZT_ALWAYS_INLINE void clear()
	{
		if (!Utils::isPrimitiveType<T>()) {
			for (unsigned long i = 0; i < _l; ++i)
				reinterpret_cast<T *>(_v)[i]->~T();
		}
		_l = 0;
	}

	ZT_ALWAYS_INLINE void push_back(const T &v)
	{
		if (_l >= _c) {
			unsigned long nc = _c << 1U;
			void *nv;
			if (_v == (void *)_baseMem) {
				nv = malloc(sizeof(T) * nc);
				memcpy(nv,_v,sizeof(T) * _l);
			} else {
				nv = realloc(_v,sizeof(T) * nc);
				if (!nv)
					throw std::bad_alloc();
			}
			_v = nv;
			_c = nc;
		}
		new (reinterpret_cast<T *>(_v) + _l++) T(v);
	}

	ZT_ALWAYS_INLINE void pop_back()
	{
		if (!Utils::isPrimitiveType<T>())
			reinterpret_cast<T *>(_v)[_l]->~T();
		--_l;
	}

	ZT_ALWAYS_INLINE reference front() { reinterpret_cast<T *>(_v)[0]; }
	ZT_ALWAYS_INLINE const_reference front() const { reinterpret_cast<T *>(_v)[0]; }
	ZT_ALWAYS_INLINE reference back() { reinterpret_cast<T *>(_v)[_l - 1]; }
	ZT_ALWAYS_INLINE const_reference back() const { reinterpret_cast<T *>(_v)[_l - 1]; }

	ZT_ALWAYS_INLINE unsigned long size() const { return _l; }
	ZT_ALWAYS_INLINE bool empty() const { return (_l == 0); }

	ZT_ALWAYS_INLINE iterator begin() { return reinterpret_cast<T *>(_v); }
	ZT_ALWAYS_INLINE iterator end() { return (reinterpret_cast<T *>(_v) + _l); }
	ZT_ALWAYS_INLINE const_iterator begin() const { return reinterpret_cast<T *>(_v); }
	ZT_ALWAYS_INLINE const_iterator end() const { return (reinterpret_cast<T *>(_v) + _l); }

	ZT_ALWAYS_INLINE T *data() { return reinterpret_cast<T *>(_v); }
	ZT_ALWAYS_INLINE const T *data() const { return reinterpret_cast<T *>(_v); }

	ZT_ALWAYS_INLINE reference operator[](const unsigned long i) { return reinterpret_cast<T *>(_v)[i]; }
	ZT_ALWAYS_INLINE const_reference operator[](const unsigned long i) const { return reinterpret_cast<T *>(_v)[i]; }
	ZT_ALWAYS_INLINE reference at(const unsigned long i) { return reinterpret_cast<T *>(_v)[i]; }
	ZT_ALWAYS_INLINE const_reference at(const unsigned long i) const { return reinterpret_cast<T *>(_v)[i]; }

private:
	uint8_t _baseMem[BASE * sizeof(T)];
	void *_v;
	unsigned long _c;
	unsigned long _l;
};

} // namespace ZeroTier

#endif
