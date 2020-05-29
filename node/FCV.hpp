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

#ifndef ZT_FCV_HPP
#define ZT_FCV_HPP

#include "Constants.hpp"

#include <iterator>
#include <algorithm>
#include <memory>

namespace ZeroTier {

/**
 * FCV is a Fixed Capacity Vector
 *
 * This doesn't implement everything in std::vector, just what we need. It
 * also adds a few special things for use in ZT core code.
 *
 * @tparam T Type to contain
 * @tparam C Maximum capacity of vector
 */
template<typename T, unsigned int C>
class FCV
{
public:
	typedef T *iterator;
	typedef const T *const_iterator;

	ZT_INLINE FCV() noexcept: _s(0)
	{}

	ZT_INLINE FCV(const FCV &v) : _s(0)
	{ *this = v; }

	ZT_INLINE FCV(const T *const contents, const unsigned int len) :
		_s(0)
	{
		for (unsigned int i = 0;i < len;++i)
			push_back(contents[i]);
	}

	template<typename I>
	ZT_INLINE FCV(I i, I end) :
		_s(0)
	{
		while (i != end) {
			push_back(*i);
			++i;
		}
	}

	ZT_INLINE ~FCV()
	{ this->clear(); }

	ZT_INLINE FCV &operator=(const FCV &v)
	{
		if (likely(&v != this)) {
			this->clear();
			const unsigned int s = v._s;
			_s = s;
			for (unsigned int i = 0;i < s;++i)
				new(reinterpret_cast<T *>(_m) + i) T(*(reinterpret_cast<const T *>(v._m) + i));
		}
		return *this;
	}

	/**
	 * Clear this vector, destroying all content objects
	 */
	ZT_INLINE void clear()
	{
		const unsigned int s = _s;
		_s = 0;
		for (unsigned int i = 0;i < s;++i)
			(reinterpret_cast<T *>(_m) + i)->~T();
	}

	/**
	 * Move contents from this vector to another and clear this vector.
	 *
	 * @param v Target vector
	 */
	ZT_INLINE void unsafeMoveTo(FCV &v) noexcept
	{
		Utils::copy(v._m, _m, (v._s = _s) * sizeof(T));
		_s = 0;
	}

	ZT_INLINE iterator begin() noexcept
	{ return reinterpret_cast<T *>(_m); }

	ZT_INLINE iterator end() noexcept
	{ return reinterpret_cast<T *>(_m) + _s; }

	ZT_INLINE const_iterator begin() const noexcept
	{ return reinterpret_cast<const T *>(_m); }

	ZT_INLINE const_iterator end() const noexcept
	{ return reinterpret_cast<const T *>(_m) + _s; }

	ZT_INLINE T &operator[](const unsigned int i)
	{
		if (likely(i < _s))
			return reinterpret_cast<T *>(_m)[i];
		throw std::out_of_range("i > capacity");
	}

	ZT_INLINE const T &operator[](const unsigned int i) const
	{
		if (likely(i < _s))
			return reinterpret_cast<const T *>(_m)[i];
		throw std::out_of_range("i > capacity");
	}

	static constexpr unsigned int capacity() noexcept
	{ return C; }

	ZT_INLINE unsigned int size() const noexcept
	{ return _s; }

	ZT_INLINE bool empty() const noexcept
	{ return (_s == 0); }

	ZT_INLINE T *data() noexcept
	{ return reinterpret_cast<T *>(_m); }

	ZT_INLINE const T *data() const noexcept
	{ return reinterpret_cast<const T *>(_m); }

	/**
	 * Push a value onto the back of this vector
	 *
	 * If the vector is at capacity this silently fails.
	 *
	 * @param v Value to push
	 */
	ZT_INLINE void push_back(const T &v)
	{
		if (likely(_s < C))
			new(reinterpret_cast<T *>(_m) + _s++) T(v);
		else throw std::out_of_range("capacity exceeded");
	}

	/**
	 * Push new default value or return last in vector if full.
	 *
	 * @return Reference to new item
	 */
	ZT_INLINE T &push()
	{
		if (likely(_s < C)) {
			return *(new(reinterpret_cast<T *>(_m) + _s++) T());
		} else {
			return *(reinterpret_cast<T *>(_m) + (C - 1));
		}
	}

	/**
	 * Push new default value or replace and return last in vector if full.
	 *
	 * @return Reference to new item
	 */
	ZT_INLINE T &push(const T &v)
	{
		if (likely(_s < C)) {
			return *(new(reinterpret_cast<T *>(_m) + _s++) T(v));
		} else {
			T &tmp = *(reinterpret_cast<T *>(_m) + (C - 1));
			tmp = v;
			return tmp;
		}
	}

	/**
	 * Remove the last element if this vector is not empty
	 */
	ZT_INLINE void pop_back()
	{
		if (likely(_s != 0))
			(reinterpret_cast<T *>(_m) + --_s)->~T();
	}

	/**
	 * Resize vector
	 *
	 * @param ns New size (clipped to C if larger than capacity)
	 */
	ZT_INLINE void resize(unsigned int ns)
	{
		if (unlikely(ns > C))
			throw std::out_of_range("capacity exceeded");
		unsigned int s = _s;
		while (s < ns)
			new(reinterpret_cast<T *>(_m) + s++) T();
		while (s > ns)
			(reinterpret_cast<T *>(_m) + --s)->~T();
		_s = s;
	}

	/**
	 * Set the size of this vector without otherwise changing anything
	 *
	 * @param ns New size
	 */
	ZT_INLINE void unsafeSetSize(unsigned int ns)
	{ _s = ns; }

	/**
	 * This is a bounds checked auto-resizing variant of the [] operator
	 *
	 * If 'i' is out of bounds vs the current size of the vector, the vector is
	 * resized. If that size would exceed C (capacity), 'i' is clipped to C-1.
	 *
	 * @param i Index to obtain as a reference, resizing if needed
	 * @return Reference to value at this index
	 */
	ZT_INLINE T &at(unsigned int i)
	{
		if (i >= _s) {
			if (unlikely(i >= C))
				i = C - 1;
			do {
				new(reinterpret_cast<T *>(_m) + _s++) T();
			} while (i >= _s);
		}
		return *(reinterpret_cast<T *>(_m) + i);
	}

	/**
	 * Assign this vector's contents from a range of pointers or iterators
	 *
	 * If the range is larger than C it is truncated at C.
	 *
	 * @tparam X Inferred type of interators or pointers
	 * @param start Starting iterator
	 * @param end Ending iterator (must be greater than start)
	 */
	template<typename X>
	ZT_INLINE void assign(X start, const X &end)
	{
		const int l = std::min((int) std::distance(start, end), (int) C);
		if (l > 0) {
			this->resize((unsigned int) l);
			for (int i = 0;i < l;++i)
				reinterpret_cast<T *>(_m)[i] = *(start++);
		} else {
			this->clear();
		}
	}

	ZT_INLINE bool operator==(const FCV &v) const noexcept
	{
		if (_s == v._s) {
			for (unsigned int i = 0;i < _s;++i) {
				if (!(*(reinterpret_cast<const T *>(_m) + i) == *(reinterpret_cast<const T *>(v._m) + i)))
					return false;
			}
			return true;
		}
		return false;
	}

	ZT_INLINE bool operator!=(const FCV &v) const noexcept
	{ return (!(*this == v)); }

	ZT_INLINE bool operator<(const FCV &v) const noexcept
	{ return std::lexicographical_compare(begin(), end(), v.begin(), v.end()); }

	ZT_INLINE bool operator>(const FCV &v) const noexcept
	{ return (v < *this); }

	ZT_INLINE bool operator<=(const FCV &v) const noexcept
	{ return !(v < *this); }

	ZT_INLINE bool operator>=(const FCV &v) const noexcept
	{ return !(*this < v); }

private:
#ifdef _MSC_VER
	uint8_t _m[sizeof(T) * C];
#else
	__attribute__((aligned(16))) uint8_t _m[sizeof(T) * C];
#endif
	unsigned int _s;
};

} // namespace ZeroTier

#endif
