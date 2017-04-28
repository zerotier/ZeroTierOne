/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2017  ZeroTier, Inc.  https://www.zerotier.com/
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

#ifndef ZT_ARRAY_HPP
#define ZT_ARRAY_HPP

#include <string>
#include <algorithm>

namespace ZeroTier {

/**
 * Static array -- a simple thing that's belonged in STL since the time of the dinosaurs
 */
template<typename T,std::size_t S>
class Array
{
public:
	Array() throw() {}

	Array(const Array &a)
	{
		for(std::size_t i=0;i<S;++i)
			data[i] = a.data[i];
	}

	Array(const T *ptr)
	{
		for(std::size_t i=0;i<S;++i)
			data[i] = ptr[i];
	}

	inline Array &operator=(const Array &a)
	{
		for(std::size_t i=0;i<S;++i)
			data[i] = a.data[i];
		return *this;
	}

	typedef T value_type;
	typedef T* pointer;
	typedef const T* const_pointer;
	typedef T& reference;
	typedef const T& const_reference;
	typedef T* iterator;
	typedef const T* const_iterator;
	typedef std::size_t size_type;
	typedef std::ptrdiff_t difference_type;
	typedef std::reverse_iterator<iterator> reverse_iterator;
	typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

	inline iterator begin() throw() { return data; }
	inline iterator end() throw() { return &(data[S]); }
	inline const_iterator begin() const throw() { return data; }
	inline const_iterator end() const throw() { return &(data[S]); }

	inline reverse_iterator rbegin() throw() { return reverse_iterator(begin()); }
	inline reverse_iterator rend() throw() { return reverse_iterator(end()); }
	inline const_reverse_iterator rbegin() const throw() { return const_reverse_iterator(begin()); }
	inline const_reverse_iterator rend() const throw() { return const_reverse_iterator(end()); }

	inline std::size_t size() const throw() { return S; }
	inline std::size_t max_size() const throw() { return S; }

	inline reference operator[](const std::size_t n) throw() { return data[n]; }
	inline const_reference operator[](const std::size_t n) const throw() { return data[n]; }

	inline reference front() throw() { return data[0]; }
	inline const_reference front() const throw() { return data[0]; }
	inline reference back() throw() { return data[S-1]; }
	inline const_reference back() const throw() { return data[S-1]; }

	inline bool operator==(const Array &k) const throw()
	{
		for(unsigned long i=0;i<S;++i) {
			if (data[i] != k.data[i])
				return false;
		}
		return true;
	}
	inline bool operator<(const Array &k) const throw() { return std::lexicographical_compare(begin(),end(),k.begin(),k.end()); }
	inline bool operator!=(const Array &k) const throw() { return !(*this == k); }
	inline bool operator>(const Array &k) const throw() { return (k < *this); }
	inline bool operator<=(const Array &k) const throw() { return !(k < *this); }
	inline bool operator>=(const Array &k) const throw() { return !(*this < k); }

	T data[S];
};

} // namespace ZeroTier

#endif
