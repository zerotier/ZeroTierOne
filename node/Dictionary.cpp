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

#include "Dictionary.hpp"

namespace ZeroTier {

Dictionary::Dictionary(const char *s,unsigned int len)
{
	for(unsigned int i=0;i<ZT_DICTIONARY_MAX_CAPACITY;++i) {
		if ((s)&&(i < len)) {
			if (!(_d[i] = *s))
				s = (const char *)0;
			else ++s;
		} else _d[i] = (char)0;
	}
	_d[ZT_DICTIONARY_MAX_CAPACITY-1] = (char)0;
}

bool Dictionary::load(const char *s)
{
	for(unsigned int i=0;i<ZT_DICTIONARY_MAX_CAPACITY;++i) {
		if (s) {
			if (!(_d[i] = *s))
				s = (const char *)0;
			else ++s;
		} else _d[i] = (char)0;
	}
	_d[ZT_DICTIONARY_MAX_CAPACITY - 1] = (char)0;
	return (!s);
}

int Dictionary::get(const char *key,char *dest,unsigned int destlen) const
{
	const char *p = _d;
	const char *const eof = p + ZT_DICTIONARY_MAX_CAPACITY;
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

bool Dictionary::add(const char *key,const char *value,int vlen)
{
	for(unsigned int i=0;i<ZT_DICTIONARY_MAX_CAPACITY;++i) {
		if (!_d[i]) {
			unsigned int j = i;

			if (j > 0) {
				_d[j++] = (char)10;
				if (j == ZT_DICTIONARY_MAX_CAPACITY) {
					_d[i] = (char)0;
					return false;
				}
			}

			const char *p = key;
			while (*p) {
				_d[j++] = *(p++);
				if (j == ZT_DICTIONARY_MAX_CAPACITY) {
					_d[i] = (char)0;
					return false;
				}
			}

			_d[j++] = '=';
			if (j == ZT_DICTIONARY_MAX_CAPACITY) {
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
						if (j == ZT_DICTIONARY_MAX_CAPACITY) {
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
						if (j == ZT_DICTIONARY_MAX_CAPACITY) {
							_d[i] = (char)0;
							return false;
						}
						break;
					default:
						_d[j++] = *p;
						if (j == ZT_DICTIONARY_MAX_CAPACITY) {
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

bool Dictionary::add(const char *key,bool value)
{
	return this->add(key,(value) ? "1" : "0",1);
}

bool Dictionary::add(const char *key,uint64_t value)
{
	char tmp[32];
	return this->add(key,Utils::hex(value,tmp),-1);
}

bool Dictionary::add(const char *key,int64_t value)
{
	char tmp[32];
	if (value >= 0) {
		return this->add(key,Utils::hex((uint64_t)value,tmp),-1);
	} else {
		tmp[0] = '-';
		return this->add(key,Utils::hex((uint64_t)(value * -1),tmp+1),-1);
	}
}

bool Dictionary::add(const char *key,const Address &a)
{
	char tmp[32];
	return this->add(key,Utils::hex(a.toInt(),tmp),-1);
}

} // namespace ZeroTier
