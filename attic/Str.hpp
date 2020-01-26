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

#ifndef ZT_STR_HPP
#define ZT_STR_HPP

#include "Constants.hpp"
#include "Utils.hpp"
#include "Address.hpp"
#include "MAC.hpp"
#include "InetAddress.hpp"

namespace ZeroTier {

/**
 * A short non-allocating replacement for std::string
 *
 * @tparam C Maximum capacity (default: 1021 to make total size 1024)
 */
template<unsigned long C = 1021>
class Str
{
public:
	typedef char * iterator;
	typedef const char * const_iterator;

	ZT_ALWAYS_INLINE Str() { memset(reinterpret_cast<void *>(this),0,sizeof(Str)); }
	explicit ZT_ALWAYS_INLINE Str(const char *s) { *this = s; }

	ZT_ALWAYS_INLINE Str &operator=(const char *s)
	{
		if (s) {
			unsigned int l = 0;
			while (l < C) {
				char c = s[l];
				if (!c) break;
				_s[l++] = c;
			}
			_s[l] = 0;
			_l = (uint16_t)l;
		} else {
			_l = 0;
			_s[0] = 0;
		}
	}

	ZT_ALWAYS_INLINE char operator[](const unsigned int i) const
	{
		if (i >= (unsigned int)_l)
			return 0;
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
		if (s) {
			unsigned int l = _l;
			while (l < C) {
				char c = s[l];
				if (!c) break;
				_s[l++] = c;
			}
			_s[l] = 0;
			_l = (uint16_t)l;
		}
	}
	ZT_ALWAYS_INLINE Str &operator<<(const Str &s) { return ((*this) << s._s); }
	ZT_ALWAYS_INLINE Str &operator<<(const char c)
	{
		if (_l < C) {
			_s[_l++] = c;
			_s[_l] = 0;
		}
		return *this;
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

	ZT_ALWAYS_INLINE unsigned long hashCode() const { return Utils::hashString(_s,_l); }

private:
	uint16_t _l;
	char _s[C+1];
};

} // namespace ZeroTier

#endif
