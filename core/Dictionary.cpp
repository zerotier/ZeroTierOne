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
{}

Dictionary::~Dictionary()
{}

Vector< uint8_t > &Dictionary::operator[](const char *k)
{
	if (k)
		return m_entries[s_key(k)];
	else return m_entries[""];
}

const Vector< uint8_t > &Dictionary::operator[](const char *k) const
{
	static const Vector< uint8_t > s_emptyEntry;
	if (k) {
		SortedMap< String, Vector< uint8_t > >::const_iterator e(m_entries.find(s_key(k)));
		return (e == m_entries.end()) ? s_emptyEntry : e->second;
	} else {
		SortedMap< String, Vector< uint8_t > >::const_iterator e(m_entries.find(""));
		return (e == m_entries.end()) ? s_emptyEntry : e->second;
	}
}

void Dictionary::add(const char *k, bool v)
{
	Vector< uint8_t > &e = (*this)[k];
	e.resize(2);
	e[0] = (uint8_t)(v ? '1' : '0');
	e[1] = 0;
}

void Dictionary::add(const char *k, const Address &v)
{
	Vector< uint8_t > &e = (*this)[k];
	e.resize(ZT_ADDRESS_STRING_SIZE_MAX);
	v.toString((char *)e.data());
}

void Dictionary::add(const char *k, const char *v)
{
	if ((v) && (*v)) {
		Vector< uint8_t > &e = (*this)[k];
		e.clear();
		while (*v)
			e.push_back((uint8_t)*(v++));
	}
}

void Dictionary::add(const char *k, const void *data, unsigned int len)
{
	Vector< uint8_t > &e = (*this)[k];
	if (len != 0) {
		e.assign((const uint8_t *)data, (const uint8_t *)data + len);
	} else {
		e.clear();
	}
}

bool Dictionary::getB(const char *k, bool dfl) const
{
	const Vector< uint8_t > &e = (*this)[k];
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

uint64_t Dictionary::getUI(const char *k, uint64_t dfl) const
{
	uint8_t tmp[18];
	uint64_t v = dfl;
	const Vector< uint8_t > &e = (*this)[k];
	if (!e.empty()) {
		if (e.back() != 0) {
			const unsigned long sl = e.size();
			Utils::copy(tmp, e.data(), (sl > 17) ? 17 : sl);
			tmp[17] = 0;
			return Utils::unhex((const char *)tmp);
		}
		return Utils::unhex((const char *)e.data());
	}
	return v;
}

char *Dictionary::getS(const char *k, char *v, const unsigned int cap) const
{
	if (cap == 0) // sanity check
		return v;
	const Vector< uint8_t > &e = (*this)[k];
	unsigned int i = 0;
	const unsigned int last = cap - 1;
	for (;;) {
		if ((i == last) || (i >= (unsigned int)e.size()))
			break;
		v[i] = (char)e[i];
		++i;
	}
	v[i] = 0;
	return v;
}

void Dictionary::clear()
{
	m_entries.clear();
}

void Dictionary::encode(Vector< uint8_t > &out) const
{
	out.clear();
	for (SortedMap< String, Vector< uint8_t > >::const_iterator ti(m_entries.begin()); ti != m_entries.end(); ++ti) {
		s_appendKey(out, ti->first.data());
		for (Vector< uint8_t >::const_iterator i(ti->second.begin()); i != ti->second.end(); ++i)
			s_appendValueByte(out, *i);
		out.push_back((uint8_t)'\n');
	}
	out.push_back(0);
}

bool Dictionary::decode(const void *data, unsigned int len)
{
	clear();
	String k;
	Vector< uint8_t > *v = nullptr;
	bool escape = false;
	for (unsigned int di = 0; di < len; ++di) {
		uint8_t c = reinterpret_cast<const uint8_t *>(data)[di];
		if (!c) break;
		if (v) {
			if (escape) {
				escape = false;
				switch (c) {
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
				if (c == (uint8_t)'\n') {
					k.clear();
					v = nullptr;
				} else if (c == 92) { // backslash
					escape = true;
				} else {
					v->push_back(c);
				}
			}
		} else {
			if ((c < 33) || (c > 126) || (c == 92)) {
				return false;
			} else if (c == (uint8_t)'=') {
				k.push_back(0);
				v = &m_entries[k];
			} else if (k.size() < 7) {
				k.push_back(c);
			} else {
				return false;
			}
		}
	}
	return true;
}

char *Dictionary::arraySubscript(char buf[256],const char *name,const unsigned long sub) noexcept
{
	for(unsigned int i=0;i<(256 - 17);++i) {
		if ((buf[i] = name[i]) == 0) {
			buf[i++] = '#';
			Utils::hex(sub, buf + i);
			return buf;
		}
	}
	buf[0] = 0;
	return buf;
}

} // namespace ZeroTier
