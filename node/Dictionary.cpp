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

#include "Dictionary.hpp"

#ifdef ZT_SUPPORT_OLD_STYLE_NETCONF

#include "C25519.hpp"
#include "Identity.hpp"
#include "Utils.hpp"

namespace ZeroTier {

Dictionary::iterator Dictionary::find(const std::string &key)
{
	for(iterator i(begin());i!=end();++i) {
		if (i->first == key)
			return i;
	}
	return end();
}
Dictionary::const_iterator Dictionary::find(const std::string &key) const
{
	for(const_iterator i(begin());i!=end();++i) {
		if (i->first == key)
			return i;
	}
	return end();
}

bool Dictionary::getBoolean(const std::string &key,bool dfl) const
{
	const_iterator e(find(key));
	if (e == end())
		return dfl;
	if (e->second.length() < 1)
		return dfl;
	switch(e->second[0]) {
		case '1':
		case 't':
		case 'T':
		case 'y':
		case 'Y':
			return true;
	}
	return false;
}

std::string &Dictionary::operator[](const std::string &key)
{
	for(iterator i(begin());i!=end();++i) {
		if (i->first == key)
			return i->second;
	}
	push_back(std::pair<std::string,std::string>(key,std::string()));
	std::sort(begin(),end());
	for(iterator i(begin());i!=end();++i) {
		if (i->first == key)
			return i->second;
	}
	return front().second; // should be unreachable!
}

std::string Dictionary::toString() const
{
	std::string s;
	for(const_iterator kv(begin());kv!=end();++kv) {
		_appendEsc(kv->first.data(),(unsigned int)kv->first.length(),s);
		s.push_back('=');
		_appendEsc(kv->second.data(),(unsigned int)kv->second.length(),s);
		s.append(ZT_EOL_S);
	}
	return s;
}

void Dictionary::updateFromString(const char *s,unsigned int maxlen)
{
	bool escapeState = false;
	std::string keyBuf;
	std::string *element = &keyBuf;
	const char *end = s + maxlen;
	while ((*s)&&(s < end)) {
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

void Dictionary::fromString(const char *s,unsigned int maxlen)
{
	clear();
	updateFromString(s,maxlen);
}

void Dictionary::eraseKey(const std::string &key)
{
	for(iterator i(begin());i!=end();++i) {
		if (i->first == key) {
			this->erase(i);
			return;
		}
	}
}

bool Dictionary::sign(const Identity &id,uint64_t now)
{
	try {
		// Sign identity and timestamp fields too. If there's an existing
		// signature, _mkSigBuf() ignores it.
		char nows[32];
		Utils::snprintf(nows,sizeof(nows),"%llx",(unsigned long long)now);
		(*this)[ZT_DICTIONARY_SIGNATURE_IDENTITY] = id.toString(false);
		(*this)[ZT_DICTIONARY_SIGNATURE_TIMESTAMP] = nows;

		// Create a blob to hash and sign from fields in sorted order
		std::string buf;
		_mkSigBuf(buf);

		// Add signature field
		C25519::Signature sig(id.sign(buf.data(),(unsigned int)buf.length()));
		(*this)[ZT_DICTIONARY_SIGNATURE] = Utils::hex(sig.data,(unsigned int)sig.size());

		return true;
	} catch ( ... ) {
		// Probably means identity has no secret key field
		removeSignature();
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
		return id.verify(buf.data(),(unsigned int)buf.length(),sigbin.data(),(unsigned int)sigbin.length());
	} catch ( ... ) {
		return false;
	}
}

uint64_t Dictionary::signatureTimestamp() const
{
	const_iterator ts(find(ZT_DICTIONARY_SIGNATURE_TIMESTAMP));
	if (ts == end())
		return 0;
	return Utils::hexStrToU64(ts->second.c_str());
}

void Dictionary::_mkSigBuf(std::string &buf) const
{
	unsigned long pairs = 0;
	for(const_iterator i(begin());i!=end();++i) {
		if (i->first != ZT_DICTIONARY_SIGNATURE) {
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

#endif // ZT_SUPPORT_OLD_STYLE_NETCONF
