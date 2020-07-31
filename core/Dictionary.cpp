/*
 * Copyright (c)2013-2020 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
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

Vector< uint8_t > &Dictionary::operator[](const char *const k)
{ return m_entries[k]; }

const Vector< uint8_t > &Dictionary::operator[](const char *const k) const
{
	static const Vector< uint8_t > s_emptyEntry;
	const SortedMap< String, Vector< uint8_t > >::const_iterator e(m_entries.find(k));
	return (e == m_entries.end()) ? s_emptyEntry : e->second;
}

void Dictionary::add(const char *k, const Address &v)
{
	char tmp[ZT_ADDRESS_STRING_SIZE_MAX];
	v.toString(tmp);
	add(k, tmp);
}

void Dictionary::add(const char *k, const char *v)
{
	Vector< uint8_t > &e = (*this)[k];
	e.clear();
	if (v) {
		while (*v)
			e.push_back((uint8_t)*(v++));
	}
}

void Dictionary::add(const char *k, const void *data, unsigned int len)
{
	Vector< uint8_t > &e = (*this)[k];
	if (likely(len != 0)) {
		e.assign((const uint8_t *)data, (const uint8_t *)data + len);
	} else {
		e.clear();
	}
}

uint64_t Dictionary::getUI(const char *k, uint64_t dfl) const
{
	char tmp[32];
	getS(k, tmp, sizeof(tmp));
	if (tmp[0])
		return Utils::unhex(tmp);
	return dfl;
}

char *Dictionary::getS(const char *k, char *v, const unsigned int cap) const
{
	if (cap == 0) // sanity check
		return v;

	const Vector< uint8_t > &e = (*this)[k];
	if (e.empty()) {
		v[0] = 0;
		return v;
	}

	for (unsigned int i = 0, last = (cap - 1);; ++i) {
		if ((i >= last) || (i >= (unsigned int)e.size())) {
			v[i] = 0;
			break;
		}
		if ((v[i] = (char)e[i]) == 0)
			break;
	}

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
}

bool Dictionary::decode(const void *data, unsigned int len)
{
	clear();
	String k;
	Vector< uint8_t > *v = nullptr;
	bool escape = false;
	for (unsigned int di = 0; di < len; ++di) {
		const uint8_t c = reinterpret_cast<const uint8_t *>(data)[di];
		if (c) {
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
				if (c == (uint8_t)'=') {
					v = &m_entries[k];
				} else {
					k.push_back(c);
				}
			}
		} else {
			break;
		}
	}
	return true;
}

char *Dictionary::arraySubscript(char *buf, unsigned int bufSize, const char *name, const unsigned long sub) noexcept
{
	if (bufSize < 17) { // sanity check
		buf[0] = 0;
		return buf;
	}
	for (unsigned int i = 0; i < (bufSize - 17); ++i) {
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
