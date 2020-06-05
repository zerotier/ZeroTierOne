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
#include "Identity.hpp"

namespace ZeroTier {

static const FCV<char, 8> s_signatureFingerprint("@Si", 4);
static const FCV<char, 8> s_signatureData("@Ss", 4);

Dictionary::Dictionary()
{
}

Vector<uint8_t> &Dictionary::operator[](const char *k)
{
	FCV<char, 8> key;
	return m_entries[s_key(key, k)];
}

const Vector<uint8_t> &Dictionary::operator[](const char *k) const
{
	static const Vector<uint8_t> s_emptyEntry;
	FCV<char, 8> key;
	SortedMap<FCV<char, 8>, Vector<uint8_t> >::const_iterator e(m_entries.find(s_key(key, k)));
	return (e == m_entries.end()) ? s_emptyEntry : e->second;
}

void Dictionary::add(const char *k, bool v)
{
	Vector<uint8_t> &e = (*this)[k];
	e.resize(2);
	e[0] = (uint8_t) (v ? '1' : '0');
	e[1] = 0;
}

void Dictionary::add(const char *k, const Address &v)
{
	Vector<uint8_t> &e = (*this)[k];
	e.resize(ZT_ADDRESS_STRING_SIZE_MAX);
	v.toString((char *) e.data());
}

void Dictionary::add(const char *k, const char *v)
{
	if ((v) && (*v)) {
		Vector<uint8_t> &e = (*this)[k];
		e.clear();
		while (*v)
			e.push_back((uint8_t) *(v++));
	}
}

void Dictionary::add(const char *k, const void *data, unsigned int len)
{
	Vector<uint8_t> &e = (*this)[k];
	if (len != 0) {
		e.assign((const uint8_t *) data, (const uint8_t *) data + len);
	} else {
		e.clear();
	}
}

bool Dictionary::getB(const char *k, bool dfl) const
{
	const Vector<uint8_t> &e = (*this)[k];
	if (!e.empty()) {
		switch ((char) e[0]) {
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
	const Vector<uint8_t> &e = (*this)[k];
	if (!e.empty()) {
		if (e.back() != 0) {
			const unsigned long sl = e.size();
			Utils::copy(tmp, e.data(), (sl > 17) ? 17 : sl);
			tmp[17] = 0;
			return Utils::unhex((const char *) tmp);
		}
		return Utils::unhex((const char *) e.data());
	}
	return v;
}

char *Dictionary::getS(const char *k, char *v, const unsigned int cap) const
{
	if (cap == 0) // sanity check
		return v;
	const Vector<uint8_t> &e = (*this)[k];
	unsigned int i = 0;
	const unsigned int last = cap - 1;
	for (;;) {
		if ((i == last) || (i >= (unsigned int)e.size()))
			break;
		v[i] = (char) e[i];
		++i;
	}
	v[i] = 0;
	return v;
}

bool Dictionary::sign(const Identity &signer)
{
	Vector<uint8_t> data;
	encode(data, true);
	uint8_t sig[ZT_SIGNATURE_BUFFER_SIZE];
	const unsigned int siglen = signer.sign(data.data(), (unsigned int) data.size(), sig, ZT_SIGNATURE_BUFFER_SIZE);
	if (siglen == 0)
		return false;

	uint8_t fp[ZT_ADDRESS_LENGTH + ZT_FINGERPRINT_HASH_SIZE];
	Address(signer.fingerprint().address).copyTo(fp);
	Utils::copy<ZT_FINGERPRINT_HASH_SIZE>(fp + ZT_ADDRESS_LENGTH, signer.fingerprint().hash);

	m_entries[s_signatureFingerprint].assign(fp, fp + ZT_ADDRESS_LENGTH + ZT_FINGERPRINT_HASH_SIZE);
	m_entries[s_signatureData].assign(sig, sig + siglen);

	return true;
}

Fingerprint Dictionary::signer() const
{
	SortedMap<FCV<char, 8>, Vector<uint8_t> >::const_iterator sigfp(m_entries.find(s_signatureFingerprint));
	Fingerprint fp;
	if ((sigfp != m_entries.end()) && (sigfp->second.size() == (ZT_ADDRESS_LENGTH + ZT_FINGERPRINT_HASH_SIZE))) {
		fp.address = Address(sigfp->second.data());
		Utils::copy<ZT_FINGERPRINT_HASH_SIZE>(fp.hash, sigfp->second.data() + ZT_ADDRESS_LENGTH);
	}
	return fp;
}

bool Dictionary::verify(const Identity &signer) const
{
	SortedMap< FCV<char, 8>, Vector<uint8_t> >::const_iterator sigfp(m_entries.find(s_signatureFingerprint));
	if (
		(sigfp == m_entries.end()) ||
		(sigfp->second.size() != (ZT_ADDRESS_LENGTH + ZT_FINGERPRINT_HASH_SIZE)) ||
		(Address(sigfp->second.data()) != signer.address()) ||
		(memcmp(sigfp->second.data() + ZT_ADDRESS_LENGTH,signer.fingerprint().hash,ZT_FINGERPRINT_HASH_SIZE) != 0))
		return false;

	SortedMap< FCV<char, 8>, Vector<uint8_t> >::const_iterator sig(m_entries.find(s_signatureData));
	if ((sig == m_entries.end()) || (sig->second.empty()))
		return false;

	Vector<uint8_t> data;
	encode(data, true);
	return signer.verify(data.data(),(unsigned int)data.size(),sig->second.data(),(unsigned int)sig->second.size());
}

void Dictionary::clear()
{
	m_entries.clear();
}

void Dictionary::encode(Vector<uint8_t> &out, const bool omitSignatureFields) const
{
	out.clear();
	for (SortedMap<FCV<char, 8>, Vector<uint8_t> >::const_iterator ti(m_entries.begin());ti != m_entries.end();++ti) {
		if ((!omitSignatureFields) || ((ti->first != s_signatureFingerprint) && (ti->first != s_signatureData))) {
			s_appendKey(out, ti->first.data());
			for (Vector<uint8_t>::const_iterator i(ti->second.begin());i != ti->second.end();++i)
				s_appendValueByte(out, *i);
			out.push_back((uint8_t) '\n');
		}
	}
	out.push_back(0);
}

bool Dictionary::decode(const void *data, unsigned int len)
{
	clear();
	FCV<char, 8> k;
	Vector<uint8_t> *v = nullptr;
	bool escape = false;
	for (unsigned int di = 0;di < len;++di) {
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
				if (c == (uint8_t) '\n') {
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
			} else if (c == (uint8_t) '=') {
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

} // namespace ZeroTier
