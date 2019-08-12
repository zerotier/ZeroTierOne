/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifndef ZT_DICTIONARY_HPP
#define ZT_DICTIONARY_HPP

#include "Constants.hpp"
#include "Utils.hpp"
#include "Buffer.hpp"
#include "Address.hpp"

#include <stdint.h>

namespace ZeroTier {

/**
 * A small (in code and data) packed key=value store
 *
 * This stores data in the form of a compact blob that is sort of human
 * readable (depending on whether you put binary data in it) and is backward
 * compatible with older versions. Binary data is escaped such that the
 * serialized form of a Dictionary is always a valid null-terminated C string.
 *
 * Keys are restricted: no binary data, no CR/LF, and no equals (=). If a key
 * contains these characters it may not be retrievable. This is not checked.
 *
 * Lookup is via linear search and will be slow with a lot of keys. It's
 * designed for small things.
 *
 * There is code to test and fuzz this in selftest.cpp. Fuzzing a blob of
 * pointer tricks like this is important after any modifications.
 *
 * This is used for network configurations and for saving some things on disk
 * in the ZeroTier One service code.
 *
 * @tparam C Dictionary max capacity in bytes
 */
template<unsigned int C>
class Dictionary
{
public:
	Dictionary() { memset(_d,0,sizeof(_d)); }
	Dictionary(const char *s) { this->load(s); }
	Dictionary(const char *s,unsigned int len)
	{
		for(unsigned int i=0;i<C;++i) {
			if ((s)&&(i < len)) {
				if (!(_d[i] = *s))
					s = (const char *)0;
				else ++s;
			} else _d[i] = (char)0;
		}
		_d[C - 1] = (char)0;
	}
	Dictionary(const Dictionary &d) { memcpy(_d,d._d,C); }

	inline Dictionary &operator=(const Dictionary &d)
	{
		memcpy(_d,d._d,C);
		return *this;
	}

	inline operator bool() const { return (_d[0] != 0); }

	/**
	 * Load a dictionary from a C-string
	 *
	 * @param s Dictionary in string form
	 * @return False if 's' was longer than our capacity
	 */
	inline bool load(const char *s)
	{
		for(unsigned int i=0;i<C;++i) {
			if (s) {
				if (!(_d[i] = *s))
					s = (const char *)0;
				else ++s;
			} else _d[i] = (char)0;
		}
		_d[C - 1] = (char)0;
		return (!s);
	}

	/**
	 * Delete all entries
	 */
	inline void clear()
	{
		memset(_d,0,sizeof(_d));
	}

	/**
	 * @return Size of dictionary in bytes not including terminating NULL
	 */
	inline unsigned int sizeBytes() const
	{
		for(unsigned int i=0;i<C;++i) {
			if (!_d[i])
				return i;
		}
		return C-1;
	}

	/**
	 * Get an entry
	 *
	 * Note that to get binary values, dest[] should be at least one more than
	 * the maximum size of the value being retrieved. That's because even if
	 * the data is binary a terminating 0 is still appended to dest[] after it.
	 *
	 * If the key is not found, dest[0] is set to 0 to make dest[] an empty
	 * C string in that case. The dest[] array will *never* be unterminated
	 * after this call.
	 *
	 * Security note: if 'key' is ever directly based on anything that is not
	 * a hard-code or internally-generated name, it must be checked to ensure
	 * that the buffer is NULL-terminated since key[] does not take a secondary
	 * size parameter. In NetworkConfig all keys are hard-coded strings so this
	 * isn't a problem in the core.
	 *
	 * @param key Key to look up
	 * @param dest Destination buffer
	 * @param destlen Size of destination buffer
	 * @return -1 if not found, or actual number of bytes stored in dest[] minus trailing 0
	 */
	inline int get(const char *key,char *dest,unsigned int destlen) const
	{
		const char *p = _d;
		const char *const eof = p + C;
		const char *k;
		bool esc;
		int j;

		if (!destlen) // sanity check
			return -1;

		while (*p) {
			k = key;
			while ((*k)&&(*p)) {
				if (*p != *k)
					break;
				++k;
				if (++p == eof) {
					dest[0] = (char)0;
					return -1;
				}
			}

			if ((!*k)&&(*p == '=')) {
				j = 0;
				esc = false;
				++p;
				while ((*p != 0)&&(*p != 13)&&(*p != 10)) {
					if (esc) {
						esc = false;
						switch(*p) {
							case 'r': dest[j++] = 13; break;
							case 'n': dest[j++] = 10; break;
							case '0': dest[j++] = (char)0; break;
							case 'e': dest[j++] = '='; break;
							default: dest[j++] = *p; break;
						}
						if (j == (int)destlen) {
							dest[j-1] = (char)0;
							return j-1;
						}
					} else if (*p == '\\') {
						esc = true;
					} else {
						dest[j++] = *p;
						if (j == (int)destlen) {
							dest[j-1] = (char)0;
							return j-1;
						}
					}
					if (++p == eof) {
						dest[0] = (char)0;
						return -1;
					}
				}
				dest[j] = (char)0;
				return j;
			} else {
				while ((*p)&&(*p != 13)&&(*p != 10)) {
					if (++p == eof) {
						dest[0] = (char)0;
						return -1;
					}
				}
				if (*p) {
					if (++p == eof) {
						dest[0] = (char)0;
						return -1;
					}
				}
				else break;
			}
		}

		dest[0] = (char)0;
		return -1;
	}

	/**
	 * Get the contents of a key into a buffer
	 *
	 * @param key Key to get
	 * @param dest Destination buffer
	 * @return True if key was found (if false, dest will be empty)
	 * @tparam BC Buffer capacity (usually inferred)
	 */
	template<unsigned int BC>
	inline bool get(const char *key,Buffer<BC> &dest) const
	{
		const int r = this->get(key,const_cast<char *>(reinterpret_cast<const char *>(dest.data())),BC);
		if (r >= 0) {
			dest.setSize((unsigned int)r);
			return true;
		} else {
			dest.clear();
			return false;
		}
	}

	/**
	 * Get a boolean value
	 *
	 * @param key Key to look up
	 * @param dfl Default value if not found in dictionary
	 * @return Boolean value of key or 'dfl' if not found
	 */
	bool getB(const char *key,bool dfl = false) const
	{
		char tmp[4];
		if (this->get(key,tmp,sizeof(tmp)) >= 0)
			return ((*tmp == '1')||(*tmp == 't')||(*tmp == 'T'));
		return dfl;
	}

	/**
	 * Get an unsigned int64 stored as hex in the dictionary
	 *
	 * @param key Key to look up
	 * @param dfl Default value or 0 if unspecified
	 * @return Decoded hex UInt value or 'dfl' if not found
	 */
	inline uint64_t getUI(const char *key,uint64_t dfl = 0) const
	{
		char tmp[128];
		if (this->get(key,tmp,sizeof(tmp)) >= 1)
			return Utils::hexStrToU64(tmp);
		return dfl;
	}

	/**
	 * Get an unsigned int64 stored as hex in the dictionary
	 *
	 * @param key Key to look up
	 * @param dfl Default value or 0 if unspecified
	 * @return Decoded hex UInt value or 'dfl' if not found
	 */
	inline int64_t getI(const char *key,int64_t dfl = 0) const
	{
		char tmp[128];
		if (this->get(key,tmp,sizeof(tmp)) >= 1)
			return Utils::hexStrTo64(tmp);
		return dfl;
	}

	/**
	 * Add a new key=value pair
	 *
	 * If the key is already present this will append another, but the first
	 * will always be returned by get(). This is not checked. If you want to
	 * ensure a key is not present use erase() first.
	 *
	 * Use the vlen parameter to add binary values. Nulls will be escaped.
	 *
	 * @param key Key -- nulls, CR/LF, and equals (=) are illegal characters
	 * @param value Value to set
	 * @param vlen Length of value in bytes or -1 to treat value[] as a C-string and look for terminating 0
	 * @return True if there was enough room to add this key=value pair
	 */
	inline bool add(const char *key,const char *value,int vlen = -1)
	{
		for(unsigned int i=0;i<C;++i) {
			if (!_d[i]) {
				unsigned int j = i;

				if (j > 0) {
					_d[j++] = (char)10;
					if (j == C) {
						_d[i] = (char)0;
						return false;
					}
				}

				const char *p = key;
				while (*p) {
					_d[j++] = *(p++);
					if (j == C) {
						_d[i] = (char)0;
						return false;
					}
				}

				_d[j++] = '=';
				if (j == C) {
					_d[i] = (char)0;
					return false;
				}

				p = value;
				int k = 0;
				while ( ((vlen < 0)&&(*p)) || (k < vlen) ) {
					switch(*p) {
						case 0:
						case 13:
						case 10:
						case '\\':
						case '=':
							_d[j++] = '\\';
							if (j == C) {
								_d[i] = (char)0;
								return false;
							}
							switch(*p) {
								case 0: _d[j++] = '0'; break;
								case 13: _d[j++] = 'r'; break;
								case 10: _d[j++] = 'n'; break;
								case '\\': _d[j++] = '\\'; break;
								case '=': _d[j++] = 'e'; break;
							}
							if (j == C) {
								_d[i] = (char)0;
								return false;
							}
							break;
						default:
							_d[j++] = *p;
							if (j == C) {
								_d[i] = (char)0;
								return false;
							}
							break;
					}
					++p;
					++k;
				}

				_d[j] = (char)0;

				return true;
			}
		}
		return false;
	}

	/**
	 * Add a boolean as a '1' or a '0'
	 */
	inline bool add(const char *key,bool value)
	{
		return this->add(key,(value) ? "1" : "0",1);
	}

	/** 
	 * Add a 64-bit integer (unsigned) as a hex value
	 */
	inline bool add(const char *key,uint64_t value)
	{
		char tmp[32];
		return this->add(key,Utils::hex(value,tmp),-1);
	}

	/** 
	 * Add a 64-bit integer (unsigned) as a hex value
	 */
	inline bool add(const char *key,int64_t value)
	{
		char tmp[32];
		if (value >= 0) {
			return this->add(key,Utils::hex((uint64_t)value,tmp),-1);
		} else {
			tmp[0] = '-';
			return this->add(key,Utils::hex((uint64_t)(value * -1),tmp+1),-1);
		}
	}

	/** 
	 * Add a 64-bit integer (unsigned) as a hex value
	 */
	inline bool add(const char *key,const Address &a)
	{
		char tmp[32];
		return this->add(key,Utils::hex(a.toInt(),tmp),-1);
	}

	/**
	 * Add a binary buffer's contents as a value
	 *
	 * @tparam BC Buffer capacity (usually inferred)
	 */
	template<unsigned int BC>
	inline bool add(const char *key,const Buffer<BC> &value)
	{
		return this->add(key,(const char *)value.data(),(int)value.size());
	}

	/**
	 * @param key Key to check
	 * @return True if key is present
	 */
	inline bool contains(const char *key) const
	{
		char tmp[2];
		return (this->get(key,tmp,2) >= 0);
	}

	/**
	 * @return Value of C template parameter
	 */
	inline unsigned int capacity() const { return C; }

	inline const char *data() const { return _d; }
	inline char *unsafeData() { return _d; }

private:
	char _d[C];
};

} // namespace ZeroTier

#endif
