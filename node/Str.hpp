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

#define ZT_STR_CAPACITY 254

namespace ZeroTier {

/**
 * A short non-allocating replacement for std::string
 */
class Str
{
public:
	typedef char * iterator;
	typedef const char * const_iterator;

	ZT_ALWAYS_INLINE Str() { _l = 0; _s[0] = 0; }
	ZT_ALWAYS_INLINE Str(const Str &s)
	{
		_l = s._l;
		memcpy(_s,s._s,_l+1);
	}
	ZT_ALWAYS_INLINE Str(const char *s)
	{
		_l = 0;
		_s[0] = 0;
		(*this) << s;
	}
	ZT_ALWAYS_INLINE Str(const std::string &s)
	{
		*this = s;
	}

	ZT_ALWAYS_INLINE Str &operator=(const Str &s)
	{
		_l = s._l;
		memcpy(_s,s._s,_l+1);
		return *this;
	}
	ZT_ALWAYS_INLINE Str &operator=(const char *s)
	{
		_l = 0;
		_s[0] = 0;
		return ((*this) << s);
	}
	ZT_ALWAYS_INLINE Str &operator=(const std::string &s)
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

	ZT_ALWAYS_INLINE char operator[](const unsigned int i) const
	{
		if (unlikely(i >= (unsigned int)_l))
			throw ZT_EXCEPTION_OUT_OF_BOUNDS;
		return _s[i];
	}

	ZT_ALWAYS_INLINE void clear() { _l = 0; _s[0] = 0; }
	ZT_ALWAYS_INLINE const char *c_str() const { return _s; }
	ZT_ALWAYS_INLINE unsigned int length() const { return (unsigned int)_l; }
	ZT_ALWAYS_INLINE bool empty() const { return (_l == 0); }
	ZT_ALWAYS_INLINE iterator begin() { return (iterator)_s; }
	ZT_ALWAYS_INLINE iterator end() { return (iterator)(_s + (unsigned long)_l); }
	ZT_ALWAYS_INLINE const_iterator begin() const { return (const_iterator)_s; }
	ZT_ALWAYS_INLINE const_iterator end() const { return (const_iterator)(_s + (unsigned long)_l); }

	ZT_ALWAYS_INLINE Str &operator<<(const char *s)
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
	ZT_ALWAYS_INLINE Str &operator<<(const Str &s) { return ((*this) << s._s); }
	ZT_ALWAYS_INLINE Str &operator<<(const char c)
	{
		if (unlikely(_l >= ZT_STR_CAPACITY)) {
			_s[ZT_STR_CAPACITY] = 0;
			throw ZT_EXCEPTION_OUT_OF_BOUNDS;
		}
		_s[(unsigned long)(_l++)] = c;
		_s[(unsigned long)_l] = 0;
		return *this;
	}
	ZT_ALWAYS_INLINE Str &operator<<(const unsigned long n)
	{
		char tmp[32];
		Utils::decimal(n,tmp);
		return ((*this) << tmp);
	}
	ZT_ALWAYS_INLINE Str &operator<<(const unsigned int n)
	{
		char tmp[32];
		Utils::decimal((unsigned long)n,tmp);
		return ((*this) << tmp);
	}
	ZT_ALWAYS_INLINE Str &operator<<(const Address &a)
	{
		char tmp[32];
		return ((*this) << a.toString(tmp));
	}
	ZT_ALWAYS_INLINE Str &operator<<(const InetAddress &a)
	{
		char tmp[128];
		return ((*this) << a.toString(tmp));
	}
	ZT_ALWAYS_INLINE Str &operator<<(const MAC &a)
	{
		char tmp[64];
		return ((*this) << a.toString(tmp));
	}

	ZT_ALWAYS_INLINE Str &append(const char *s,const unsigned int max)
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

	ZT_ALWAYS_INLINE operator bool() const { return (_l != 0); }

	ZT_ALWAYS_INLINE bool operator==(const Str &s) const { return ((_l == s._l)&&(memcmp(_s,s._s,_l) == 0)); }
	ZT_ALWAYS_INLINE bool operator!=(const Str &s) const { return ((_l != s._l)||(memcmp(_s,s._s,_l) != 0)); }
	ZT_ALWAYS_INLINE bool operator<(const Str &s) const { return ( (_l < s._l) ? true : ((_l == s._l) ? (memcmp(_s,s._s,_l) < 0) : false) ); }
	ZT_ALWAYS_INLINE bool operator>(const Str &s) const { return (s < *this); }
	ZT_ALWAYS_INLINE bool operator<=(const Str &s) const { return !(s < *this); }
	ZT_ALWAYS_INLINE bool operator>=(const Str &s) const { return !(*this < s); }

	ZT_ALWAYS_INLINE bool operator==(const char *s) const { return (strcmp(_s,s) == 0); }
	ZT_ALWAYS_INLINE bool operator!=(const char *s) const { return (strcmp(_s,s) != 0); }
	ZT_ALWAYS_INLINE bool operator<(const char *s) const { return (strcmp(_s,s) < 0); }
	ZT_ALWAYS_INLINE bool operator>(const char *s) const { return (strcmp(_s,s) > 0); }
	ZT_ALWAYS_INLINE bool operator<=(const char *s) const { return (strcmp(_s,s) <= 0); }
	ZT_ALWAYS_INLINE bool operator>=(const char *s) const { return (strcmp(_s,s) >= 0); }

	ZT_ALWAYS_INLINE unsigned long hashCode() const
	{
		const char *p = _s;
		unsigned long h = 0;
		char c;
		while ((c = *(p++)))
			h = (31 * h) + (unsigned long)c;
		return h;
	}

	template<unsigned int C>
	inline void serialize(Buffer<C> &b,const bool forSign = false) const
	{
		b.append(_l);
		b.append(_s,(unsigned int)_l);
	}

	template<unsigned int C>
	inline unsigned int deserialize(const Buffer<C> &b,unsigned int startAt = 0)
	{
		unsigned int p = startAt;
		_l = (uint8_t)b[p++];
		memcpy(_s,b.field(p,(unsigned int)_l),(unsigned long)_l);
		p += (unsigned int)_l;
		return (p - startAt);
	}

private:
	uint8_t _l;
	char _s[ZT_STR_CAPACITY+1];
};

} // namespace ZeroTier

#endif
