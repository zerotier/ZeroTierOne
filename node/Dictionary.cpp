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

Dictionary::Dictionary()
{
}

Dictionary::~Dictionary()
{
}

std::vector<uint8_t> &Dictionary::operator[](const char *k)
{
	return _t[_toKey(k)];
}

const std::vector<uint8_t> &Dictionary::operator[](const char *k) const
{
	static const std::vector<uint8_t> emptyEntry;
	std::map< uint64_t,std::vector<uint8_t> >::const_iterator e(_t.find(_toKey(k)));
	return (e == _t.end()) ? emptyEntry : e->second;
}

void Dictionary::add(const char *k,bool v)
{
	std::vector<uint8_t> &e = (*this)[k];
	e.resize(2);
	e[0] = (uint8_t)(v ? '1' : '0');
	e[1] = 0;
}

void Dictionary::add(const char *k,uint16_t v)
{
	std::vector<uint8_t> &e = (*this)[k];
	e.resize(5);
	Utils::hex(v,(char *)e.data());
}

void Dictionary::add(const char *k,uint32_t v)
{
	std::vector<uint8_t> &e = (*this)[k];
	e.resize(9);
	Utils::hex(v,(char *)e.data());
}

void Dictionary::add(const char *k,uint64_t v)
{
	std::vector<uint8_t> &e = (*this)[k];
	e.resize(17);
	Utils::hex(v,(char *)e.data());
}

void Dictionary::add(const char *k,const Address &v)
{
	std::vector<uint8_t> &e = (*this)[k];
	e.resize(ZT_ADDRESS_STRING_SIZE_MAX);
	v.toString((char *)e.data());
}

void Dictionary::add(const char *k,const char *v)
{
	std::vector<uint8_t> &e = (*this)[k];
	e.clear();
	if (v) {
		for(;;) {
			const uint8_t c = (uint8_t)*(v++);
			e.push_back(c);
			if (!c) break;
		}
	}
}

void Dictionary::add(const char *k,const void *data,unsigned int len)
{
	std::vector<uint8_t> &e = (*this)[k];
	if (len != 0) {
		e.assign((const uint8_t *)data,(const uint8_t *)data + len);
	} else {
		e.clear();
	}
}

bool Dictionary::getB(const char *k,bool dfl) const
{
	const std::vector<uint8_t> &e = (*this)[k];
	if (!e.empty()) {
		switch ((char)e[0]) {
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

uint64_t Dictionary::getUI(const char *k,uint64_t dfl) const
{
	uint8_t tmp[18];
	uint64_t v = dfl;
	const std::vector<uint8_t> &e = (*this)[k];
	if (!e.empty()) {
		if (e.back() != 0) {
			const unsigned long sl = e.size();
			Utils::copy(tmp,e.data(),(sl > 17) ? 17 : sl);
			tmp[17] = 0;
			return Utils::unhex((const char *)tmp);
		}
		return Utils::unhex((const char *)e.data());
	}
	return v;
}

void Dictionary::getS(const char *k,char *v,unsigned int cap) const
{
	if (cap == 0) // sanity check
		return;
	const std::vector<uint8_t> &e = (*this)[k];
	unsigned int i = 0;
	const unsigned int last = cap - 1;
	for(;;) {
		if ((i == last)||(i >= (unsigned int)e.size()))
			break;
		v[i] = (char)e[i];
		++i;
	}
	v[i] = 0;
}

void Dictionary::clear()
{
	_t.clear();
}

void Dictionary::encode(std::vector<uint8_t> &out) const
{
	uint64_t str[2] = { 0,0 }; // second entry causes all strings to be null-terminated even if 8 chars in length

	out.clear();

	for(std::map< uint64_t,std::vector<uint8_t> >::const_iterator ti(_t.begin());ti!=_t.end();++ti) {
		str[0] = ti->first;
		const char *k = (const char *)str;
		for(;;) {
			char kc = *(k++);
			if (!kc) break;
			if ((kc >= 33)&&(kc <= 126)&&(kc != 61)&&(kc != 92)) // printable ASCII with no spaces, equals, or backslash
				out.push_back((uint8_t)kc);
		}

		out.push_back(61); // =

		for(std::vector<uint8_t>::const_iterator i(ti->second.begin());i!=ti->second.end();++i) {
			uint8_t c = *i;
			switch(c) {
				case 0:
					out.push_back(92);
					out.push_back(48);
					break;
				case 10:
					out.push_back(92);
					out.push_back(110);
					break;
				case 13:
					out.push_back(92);
					out.push_back(114);
					break;
				case 61:
					out.push_back(92);
					out.push_back(101);
					break;
				case 92:
					out.push_back(92);
					out.push_back(92);
					break;
				default:
					out.push_back(c);
					break;
			}
		}

		out.push_back(10);
	}
}

bool Dictionary::decode(const void *data,unsigned int len)
{
	clear();

	uint64_t k = 0;
	unsigned int ki = 0;
	std::vector<uint8_t> *v = nullptr;
	bool escape = false;
	for(unsigned int di=0;di<len;++di) {
		uint8_t c = reinterpret_cast<const uint8_t *>(data)[di];
		if (!c) break;
		if (v) {
			if (escape) {
				escape = false;
				switch(c) {
					case 48:
						v->push_back(0);
						break;
					case 101:
						v->push_back(61);
						break;
					case 110:
						v->push_back(10);
						break;
					case 114:
						v->push_back(13);
						break;
					default:
						v->push_back(c);
						break;
				}
			} else {
				if (c == 10) {
					k = 0;
					ki = 0;
					v = nullptr;
				} else if (c == 92) {
					escape = true;
				} else {
					v->push_back(c);
				}
			}
		} else {
			if ((c < 33)||(c > 126)||(c == 92)) {
				return false;
			} else if (c == 61) {
				v = &_t[k];
			} else {
				reinterpret_cast<uint8_t *>(&k)[ki & 7U] ^= c;
			}
		}
	}

	return true;
}

} // namespace ZeroTier
