/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2018  ZeroTier, Inc.  https://www.zerotier.com/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#include "Constants.hpp"

namespace ZeroTier {

/**
 * A really simple fixed capacity vector
 *
 * This class does no bounds checking, so the user must ensure that
 * no more than C elements are ever added and that accesses are in
 * bounds.
 *
 * @tparam T Type to contain
 * @tparam C Capacity of vector
 */
template<typename T,unsigned long C>
class FCV
{
public:
	FCV() : _s(0) {}
	~FCV() { clear(); }

	FCV(const FCV &v) :
		_s(v._s)
	{
		for(unsigned long i=0;i<_s;++i) {
			new (reinterpret_cast<T *>(_mem + (sizeof(T) * i))) T(reinterpret_cast<const T *>(v._mem)[i]);
		}
	}

	inline FCV &operator=(const FCV &v)
	{
		clear();
		_s = v._s;
		for(unsigned long i=0;i<_s;++i) {
			new (reinterpret_cast<T *>(_mem + (sizeof(T) * i))) T(reinterpret_cast<const T *>(v._mem)[i]);
		}
		return *this;
	}

	typedef T * iterator;
	typedef const T * const_iterator;
	typedef unsigned long size_type;

	inline iterator begin() { return (T *)_mem; }
	inline iterator end() { return (T *)(_mem + (sizeof(T) * _s)); }
	inline iterator begin() const { return (const T *)_mem; }
	inline iterator end() const { return (const T *)(_mem + (sizeof(T) * _s)); }

	inline T &operator[](const size_type i) { return reinterpret_cast<T *>(_mem)[i]; }
	inline const T &operator[](const size_type i) const { return reinterpret_cast<const T *>(_mem)[i]; }

	inline T &front() { return reinterpret_cast<T *>(_mem)[0]; }
	inline const T &front() const { return reinterpret_cast<const T *>(_mem)[0]; }
	inline T &back() { return reinterpret_cast<T *>(_mem)[_s - 1]; }
	inline const T &back() const { return reinterpret_cast<const T *>(_mem)[_s - 1]; }

	inline void push_back(const T &v) { new (reinterpret_cast<T *>(_mem + (sizeof(T) * _s++))) T(v); }
	inline void pop_back() { reinterpret_cast<T *>(_mem + (sizeof(T) * --_s))->~T(); }

	inline size_type size() const { return _s; }
	inline size_type capacity() const { return C; }

	inline void clear()
	{
		for(unsigned long i=0;i<_s;++i)
			reinterpret_cast<T *>(_mem + (sizeof(T) * i))->~T();
		_s = 0;
	}

private:
	char _mem[sizeof(T) * C];
	unsigned long _s;
};

} // namespace ZeroTier
