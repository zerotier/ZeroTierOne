/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
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
 */

#ifndef ZT_DICTIONARY_HPP
#define ZT_DICTIONARY_HPP

#include "Constants.hpp"
#include "Utils.hpp"

#include <stdint.h>

#include <string>

#define ZT_DICTIONARY_MAX_SIZE 16384

namespace ZeroTier {

/**
 * A small key=value store
 *
 * This stores data in the form of a blob of max size ZT_DICTIONARY_MAX_SIZE.
 * It's *technically* human-readable to be backward compatible with old format
 * netconfs, but it can store binary data and doing this will negatively impact
 * its human-readability.
 *
 * In any case nulls are always escaped, making the serialized form of this
 * object a valid null-terminated C-string. Appending it to a buffer appends
 * it as such.
 *
 * Keys cannot contain binary data, CR/LF, nulls, or the equals (=) sign.
 * Adding such a key will result in an invalid entry (but isn't dangerous).
 *
 * There is code to test and fuzz this in selftest.cpp.
 */
class Dictionary
{
public:
	Dictionary()
	{
		_d[0] = (char)0;
	}

	Dictionary(const char *s)
	{
		Utils::scopy(_d,sizeof(_d),s);
	}

	inline void load(const char *s)
	{
		Utils::scopy(_d,sizeof(_d),s);
	}

	/**
	 * Delete all entries
	 */
	inline void clear()
	{
		_d[0] = (char)0;
	}

	/**
	 * @return Size of dictionary in bytes not including terminating NULL
	 */
	inline unsigned int sizeBytes() const
	{
		for(unsigned int i=0;i<ZT_DICTIONARY_MAX_SIZE;++i) {
			if (!_d[i])
				return i;
		}
		return ZT_DICTIONARY_MAX_SIZE;
	}

	/**
	 * Get an entry
	 *
	 * Note that to get binary values, dest[] should be at least one more than
	 * the maximum size of the value being retrieved. That's because even if
	 * the data is binary a terminating 0 is appended to dest[].
	 *
	 * If the key is not found, dest[0] is set to 0 to make dest[] an empty
	 * C string in that case. The dest[] array will *never* be unterminated.
	 *
	 * @param key Key to look up
	 * @param dest Destination buffer
	 * @param destlen Size of destination buffer
	 * @return -1 if not found, or actual number of bytes stored in dest[] minus trailing 0
	 */
	inline int get(const char *key,char *dest,unsigned int destlen) const
	{
		const char *p = _d;
		const char *const eof = p + ZT_DICTIONARY_MAX_SIZE;
		const char *k,*s;
		unsigned int dptr = 0;
		bool esc;
		int j;

		for(;;) {
			s = p;
			for(;;) {
				if ((*p == '\r')||(*p == '\n')||(*p == '=')||(!*p)) {
					k = key;
					while ((*k)&&(s != p)) {
						if (*(k++) != *(s++))
							break;
					}
					if (*k) {
						esc = false;
						for(;;) {
							if (!*p) {
								dest[0] = (char)0;
								return -1;
							} else if (esc) {
								esc = false;
							} else if (*p == '\\') {
								esc = true;
							} else if ((*p == '\r')||(*p == '\n')) {
								++p;
								break;
							}
							++p;
						}
						break;
					} else {
						if (*p == '=') ++p;
						esc = false;
						j = 0;
						for(;;) {
							if (esc) {
								esc = false;
								if (j >= destlen) {
									dest[destlen-1] = (char)0;
									return (int)(destlen-1);
								}
								switch(*p) {
									case 'r':
										dest[j++] = '\r';
										break;
									case 'n':
										dest[j++] = '\n';
										break;
									case 't':
										dest[j++] = '\t';
										break;
									case '0':
										dest[j++] = (char)0;
										break;
									default:
										dest[j++] = *p;
								}
							} else if (*p == '\\') {
								esc = true;
							} else if ((*p == '\r')||(*p == '\n')||(!*p)) {
								dest[j] = (char)0;
								return j;
							} else {
								if (j >= destlen) {
									dest[destlen-1] = (char)0;
									return (int)(destlen-1);
								}
								dest[j++] = *p;
							}
							++p;
						}
					}
				} else {
					++p;
				}
			}
		}
	}

	/**
	 * @param key Key to look up
	 * @param dfl Default value if not found in dictionary (a key with an empty value is considered not found)
	 * @return Boolean value of key or 'dfl' if not found
	 */
	bool getBoolean(const char *key,bool dfl = false) const
	{
		char tmp[128];
		if (this->get(key,tmp,sizeof(tmp)) >= 1) {
			switch(tmp[0]) {
				case '1':
				case 't':
				case 'T':
				case 'y':
				case 'Y':
					return true;
				default:
					return false;
			}
		}
		return dfl;
	}

	/**
	 * @param key Key to look up
	 * @param dfl Default value or 0 if unspecified
	 * @return Decoded hex UInt value or 'dfl' if not found
	 */
	inline uint64_t getHexUInt(const char *key,uint64_t dfl = 0) const
	{
		char tmp[128];
		if (this->get(key,tmp,sizeof(tmp)) >= 1)
			return Utils::hexStrToU64(tmp);
		return dfl;
	}

	/**
	 * Add a new key=value pair
	 *
	 * If the key is already present this will append another, but the first
	 * will always be returned by get(). There is no erase(). This is designed
	 * to be generated and shipped, not as an editable data structure.
	 *
	 * @param key Key -- nulls, CR/LF, and equals (=) are illegal characters
	 * @param value Value to set
	 * @param vlen Length of value in bytes or -1 to treat value[] as a C-string and look for terminating 0
	 * @return True if there was enough room to add this key=value pair
	 */
	inline bool add(const char *key,const char *value,int vlen = -1)
	{
		for(unsigned int i=0;i<ZT_DICTIONARY_MAX_SIZE;++i) {
			if (!_d[i]) {
				unsigned int j = i;
				const char *p = key;
				while (*p) {
					_d[j++] = *(p++);
					if (j == ZT_DICTIONARY_MAX_SIZE) {
						_d[i] = (char)0;
						return false;
					}
				}
				p = value;
				int k = 0;
				while ((*p)&&((vlen < 0)||(k < vlen))) {
					switch(*p) {
						case '\r':
						case '\n':
						case '\0':
						case '\t':
							_d[j++] = '\\';
							if (j == ZT_DICTIONARY_MAX_SIZE) {
								_d[i] = (char)0;
								return false;
							}
							switch(*p) {
								case '\r': _d[j++] = 'r'; break;
								case '\n': _d[j++] = 'n'; break;
								case '\0': _d[j++] = '0'; break;
								case '\t': _d[j++] = 't'; break;
							}
							if (j == ZT_DICTIONARY_MAX_SIZE) {
								_d[i] = (char)0;
								return false;
							}
							break;
						default:
							_d[j++] = *p;
							if (j == ZT_DICTIONARY_MAX_SIZE) {
								_d[i] = (char)0;
								return false;
							}
							break;
					}
					++p;
					++k;
				}
				_d[j++] = (char)0;
				return true;
			}
		}
		return false;
	}

	/**
	 * Add a boolean as a '1' or a '0'
	 */
	inline void add(const char *key,bool value)
	{
		this->add(key,(value) ? "1" : "0",1);
	}

	/** 
	 * Add a 64-bit integer (unsigned) as a hex value
	 */
	inline void add(const char *key,uint64_t value)
	{
		char tmp[128];
		Utils::snprintf(tmp,sizeof(tmp),"%llx",(unsigned long long)value);
		this->add(key,tmp,-1);
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
	 * @return Dictionary data as a 0-terminated C-string
	 */
	inline const char *data() const { return _d; }

private:
	char _d[ZT_DICTIONARY_MAX_SIZE];
};

} // namespace ZeroTier

#endif
