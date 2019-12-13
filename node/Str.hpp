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

#ifndef ZT_STR_HPP
#define ZT_STR_HPP

#include "Constants.hpp"
#include "Utils.hpp"
#include "Address.hpp"
#include "MAC.hpp"
#include "InetAddress.hpp"

#include <string>

#define ZT_STR_CAPACITY 1021

namespace ZeroTier {

/**
 * A short non-allocating replacement for std::string
 */
class Str
{
public:
	typedef char * iterator;
	typedef const char * const_iterator;

	inline Str() { _l = 0; _s[0] = 0; }
	inline Str(const Str &s)
	{
		_l = s._l;
		memcpy(_s,s._s,_l+1);
	}
	inline Str(const char *s)
	{
		_l = 0;
		_s[0] = 0;
		(*this) << s;
	}
	inline Str(const std::string &s)
	{
		*this = s;
	}

	inline Str &operator=(const Str &s)
	{
		_l = s._l;
		memcpy(_s,s._s,_l+1);
		return *this;
	}
	inline Str &operator=(const char *s)
	{
		_l = 0;
		_s[0] = 0;
		return ((*this) << s);
	}
	inline Str &operator=(const std::string &s)
	{
		if (s.length() > ZT_STR_CAPACITY) {
			_l = 0;
			_s[0] = 0;
			throw ZT_EXCEPTION_OUT_OF_BOUNDS;
		} else {
			_l = (uint8_t)s.length();
			memcpy(_s,s.data(),s.length());
			_s[s.length()] = 0;
		}
		return *this;
	}

	inline char operator[](const unsigned int i) const
	{
		if (unlikely(i >= (unsigned int)_l))
			throw ZT_EXCEPTION_OUT_OF_BOUNDS;
		return _s[i];
	}

	inline void clear() { _l = 0; _s[0] = 0; }
	inline const char *c_str() const { return _s; }
	inline unsigned int length() const { return (unsigned int)_l; }
	inline bool empty() const { return (_l == 0); }
	inline iterator begin() { return (iterator)_s; }
	inline iterator end() { return (iterator)(_s + (unsigned long)_l); }
	inline const_iterator begin() const { return (const_iterator)_s; }
	inline const_iterator end() const { return (const_iterator)(_s + (unsigned long)_l); }

	inline Str &operator<<(const char *s)
	{
		if (likely(s != (const char *)0)) {
			unsigned long l = _l;
			while (*s) {
				if (unlikely(l >= ZT_STR_CAPACITY)) {
					_s[ZT_STR_CAPACITY] = 0;
					_l = ZT_STR_CAPACITY;
					throw ZT_EXCEPTION_OUT_OF_BOUNDS;
				}
				_s[l++] = *(s++);
			}
			_s[l] = 0;
			_l = (uint8_t)l;
		}
		return *this;
	}
	inline Str &operator<<(const Str &s) { return ((*this) << s._s); }
	inline Str &operator<<(const char c)
	{
		if (unlikely(_l >= ZT_STR_CAPACITY)) {
			_s[ZT_STR_CAPACITY] = 0;
			throw ZT_EXCEPTION_OUT_OF_BOUNDS;
		}
		_s[(unsigned long)(_l++)] = c;
		_s[(unsigned long)_l] = 0;
		return *this;
	}
	inline Str &operator<<(const unsigned long n)
	{
		char tmp[32];
		Utils::decimal(n,tmp);
		return ((*this) << tmp);
	}
	inline Str &operator<<(const unsigned int n)
	{
		char tmp[32];
		Utils::decimal((unsigned long)n,tmp);
		return ((*this) << tmp);
	}
	inline Str &operator<<(const Address &a)
	{
		char tmp[32];
		return ((*this) << a.toString(tmp));
	}
	inline Str &operator<<(const InetAddress &a)
	{
		char tmp[128];
		return ((*this) << a.toString(tmp));
	}
	inline Str &operator<<(const MAC &a)
	{
		char tmp[64];
		return ((*this) << a.toString(tmp));
	}

	inline Str &append(const char *s,const unsigned int max)
	{
		if (likely(s != (const char *)0)) {
			unsigned long l = _l;
			unsigned int c = 0;
			while (*s) {
				if (c++ >= max) break;
				if (unlikely(l >= ZT_STR_CAPACITY)) {
					_s[ZT_STR_CAPACITY] = 0;
					_l = ZT_STR_CAPACITY;
					throw ZT_EXCEPTION_OUT_OF_BOUNDS;
				}
				_s[l++] = *s;
				++s;
			}
			_s[l] = 0;
			_l = (uint8_t)l;
		}
		return *this;
	}

	inline operator bool() const { return (_l != 0); }

	inline bool operator==(const Str &s) const { return ((_l == s._l)&&(memcmp(_s,s._s,_l) == 0)); }
	inline bool operator!=(const Str &s) const { return ((_l != s._l)||(memcmp(_s,s._s,_l) != 0)); }
	inline bool operator<(const Str &s) const { return ( (_l < s._l) ? true : ((_l == s._l) ? (memcmp(_s,s._s,_l) < 0) : false) ); }
	inline bool operator>(const Str &s) const { return (s < *this); }
	inline bool operator<=(const Str &s) const { return !(s < *this); }
	inline bool operator>=(const Str &s) const { return !(*this < s); }

	inline bool operator==(const char *s) const { return (strcmp(_s,s) == 0); }
	inline bool operator!=(const char *s) const { return (strcmp(_s,s) != 0); }
	inline bool operator<(const char *s) const { return (strcmp(_s,s) < 0); }
	inline bool operator>(const char *s) const { return (strcmp(_s,s) > 0); }
	inline bool operator<=(const char *s) const { return (strcmp(_s,s) <= 0); }
	inline bool operator>=(const char *s) const { return (strcmp(_s,s) >= 0); }

	inline unsigned long hashCode() const
	{
		const char *p = _s;
		unsigned long h = 0;
		char c;
		while ((c = *(p++)))
			h = (31 * h) + (unsigned long)c;
		return h;
	}

private:
	uint16_t _l;
	char _s[ZT_STR_CAPACITY+1];
};

} // namespace ZeroTier

#endif
