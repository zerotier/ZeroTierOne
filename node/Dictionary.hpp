/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#ifndef _ZT_DICTIONARY_HPP
#define _ZT_DICTIONARY_HPP

#include <string>
#include <map>
#include "Constants.hpp"

namespace ZeroTier {

/**
 * Simple key/value dictionary with string serialization
 *
 * The serialization format is a flat key=value with backslash escape.
 * It does not support comments or other syntactic complexities.
 */
class Dictionary : public std::map<std::string,std::string>
{
public:
	inline std::string toString() const
	{
		std::string s;

		for(const_iterator kv(begin());kv!=end();++kv) {
			_appendEsc(kv->first.data(),kv->first.length(),s);
			s.push_back('=');
			_appendEsc(kv->second.data(),kv->second.length(),s);
			s.append(ZT_EOL_S);
		}

		return s;
	}

	inline void fromString(const char *s)
	{
		clear();
		bool escapeState = false;
		std::string keyBuf;
		std::string *element = &keyBuf;
		while (*s) {
			if (escapeState) {
				escapeState = false;
				switch(*s) {
					case '0':
						element->push_back((char)0);
						break;
					case 'r':
						element->push_back('\r');
						break;
					case 'n':
						element->push_back('\n');
						break;
					default:
						element->push_back(*s);
						break;
				}
			} else {
				if (*s == '\\') {
					escapeState = true;
				} else if (*s == '=') {
					if (element == &keyBuf)
						element = &((*this)[keyBuf]);
				} else if ((*s == '\r')||(*s == '\n')) {
					if ((element == &keyBuf)&&(keyBuf.length() > 0))
						(*this)[keyBuf];
					keyBuf = "";
					element = &keyBuf;
				} else element->push_back(*s);
			}
			++s;
		}
		if ((element == &keyBuf)&&(keyBuf.length() > 0))
			(*this)[keyBuf];
	}
	inline void fromString(const std::string &s)
	{
		fromString(s.c_str());
	}

private:
	static inline void _appendEsc(const char *data,unsigned int len,std::string &to)
	{
		for(unsigned int i=0;i<len;++i) {
			switch(data[i]) {
				case 0:
					to.append("\\0");
					break;
				case '\r':
					to.append("\\r");
					break;
				case '\n':
					to.append("\\n");
					break;
				case '\\':
					to.append("\\\\");
					break;
				case '=':
					to.append("\\=");
					break;
				default:
					to.push_back(data[i]);
					break;
			}
		}
	}
};

} // namespace ZeroTier

#endif
