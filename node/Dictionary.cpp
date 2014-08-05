/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2014  ZeroTier Networks LLC
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

#include "Dictionary.hpp"
#include "C25519.hpp"
#include "Identity.hpp"
#include "Utils.hpp"

namespace ZeroTier {

void Dictionary::fromString(const char *s)
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

bool Dictionary::sign(const Identity &id)
{
	try {
		std::string buf;
		_mkSigBuf(buf);
		char nows[32];
		Utils::snprintf(nows,sizeof(nows),"%llx",(unsigned long long)Utils::now());
		C25519::Signature sig(id.sign(buf.data(),buf.length()));
		(*this)[ZT_DICTIONARY_SIGNATURE] = Utils::hex(sig.data,sig.size());
		(*this)[ZT_DICTIONARY_SIGNATURE_IDENTITY] = id.toString(false);
		(*this)[ZT_DICTIONARY_SIGNATURE_TIMESTAMP] = nows;
		return true;
	} catch ( ... ) {
		return false;
	}
}

bool Dictionary::verify(const Identity &id) const
{
	try {
		std::string buf;
		_mkSigBuf(buf);
		const_iterator sig(find(ZT_DICTIONARY_SIGNATURE));
		if (sig == end())
			return false;
		std::string sigbin(Utils::unhex(sig->second));
		return id.verify(buf.data(),buf.length(),sigbin.data(),sigbin.length());
	} catch ( ... ) {
		return false;
	}
}

void Dictionary::_mkSigBuf(std::string &buf) const
{
	unsigned long pairs = 0;
	for(const_iterator i(begin());i!=end();++i) {
		if ((i->first.length() < 2)||( (i->first[0] != '~')&&(i->first[1] != '!') )) {
			buf.append(i->first);
			buf.push_back('=');
			buf.append(i->second);
			buf.push_back('\0');
			++pairs;
		}
	}
	buf.push_back((char)0xff);
	buf.push_back((char)((pairs >> 24) & 0xff)); // pad with number of key/value pairs at end
	buf.push_back((char)((pairs >> 16) & 0xff));
	buf.push_back((char)((pairs >> 8) & 0xff));
	buf.push_back((char)(pairs & 0xff));
}

void Dictionary::_appendEsc(const char *data,unsigned int len,std::string &to)
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

} // namespace ZeroTier
