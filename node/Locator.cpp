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

#include "Locator.hpp"
#include "Identity.hpp"

#include <algorithm>

namespace ZeroTier {

bool Locator::add(const Endpoint &ep)
{
	if (m_endpoints.size() < ZT_LOCATOR_MAX_ENDPOINTS) {
		if (std::find(m_endpoints.begin(), m_endpoints.end(), ep) == m_endpoints.end())
			m_endpoints.push_back(ep);
		return true;
	}
	return false;
}

bool Locator::sign(const int64_t ts, const Identity &id) noexcept
{
	m_ts = ts;
	m_signer = id.fingerprint();
	std::sort(m_endpoints.begin(), m_endpoints.end());

	uint8_t signdata[ZT_LOCATOR_MARSHAL_SIZE_MAX];
	const unsigned int signlen = marshal(signdata, true);

	const unsigned int siglen = id.sign(signdata, signlen, m_signature.data(), m_signature.capacity());
	if (siglen == 0)
		return false;
	m_signature.unsafeSetSize(siglen);

	return true;
}

bool Locator::verify(const Identity &id) const noexcept
{
	try {
		if ((m_ts > 0) && (m_signer == id.fingerprint())) {
			uint8_t signdata[ZT_LOCATOR_MARSHAL_SIZE_MAX];
			const unsigned int signlen = marshal(signdata, true);
			return id.verify(signdata, signlen, m_signature.data(), m_signature.size());
		}
	} catch (...) {} // fail verify on any unexpected exception
	return false;
}

char *Locator::toString(char s[ZT_LOCATOR_STRING_SIZE_MAX]) const noexcept
{
	static_assert(ZT_LOCATOR_STRING_SIZE_MAX > ((((ZT_LOCATOR_MARSHAL_SIZE_MAX / 5) + 1) * 8) + ZT_ADDRESS_LENGTH_HEX + 1), "overflow");
	uint8_t bin[ZT_LOCATOR_MARSHAL_SIZE_MAX];
	Address(m_signer.address).toString(s);
	s[ZT_ADDRESS_LENGTH_HEX] = '@';
	Utils::b32e(bin, marshal(bin, false), s + (ZT_ADDRESS_LENGTH_HEX + 1), ZT_LOCATOR_STRING_SIZE_MAX - (ZT_ADDRESS_LENGTH_HEX + 1));
	return s;
}

bool Locator::fromString(const char *s) noexcept
{
	if (!s)
		return false;
	if (strlen(s) < (ZT_ADDRESS_LENGTH_HEX + 1))
		return false;
	uint8_t bin[ZT_LOCATOR_MARSHAL_SIZE_MAX];
	const int bl = Utils::b32d(s + (ZT_ADDRESS_LENGTH_HEX + 1), bin, ZT_LOCATOR_MARSHAL_SIZE_MAX);
	if ((bl <= 0) || (bl > ZT_LOCATOR_MARSHAL_SIZE_MAX))
		return false;
	return unmarshal(bin, bl) > 0;
}

int Locator::marshal(uint8_t data[ZT_LOCATOR_MARSHAL_SIZE_MAX], const bool excludeSignature) const noexcept
{
	Utils::storeBigEndian<uint64_t>(data, (uint64_t) m_ts);
	int p = 8;

	int l = m_signer.marshal(data + p);
	if (l <= 0)
		return -1;
	p += l;

	Utils::storeBigEndian<uint16_t>(data + p, (uint16_t) m_endpoints.size());
	p += 2;
	for (Vector<Endpoint>::const_iterator e(m_endpoints.begin());e != m_endpoints.end();++e) {
		l = e->marshal(data + p);
		if (l <= 0)
			return -1;
		p += l;
	}

	Utils::storeAsIsEndian<uint16_t>(data + p, 0); // length of meta-data, currently always 0
	p += 2;

	if (!excludeSignature) {
		Utils::storeBigEndian<uint16_t>(data + p, (uint16_t) m_signature.size());
		p += 2;
		Utils::copy(data + p, m_signature.data(), m_signature.size());
		p += (int) m_signature.size();
	}

	return p;
}

int Locator::unmarshal(const uint8_t *data, const int len) noexcept
{
	if (unlikely(len < 8))
		return -1;
	m_ts = (int64_t) Utils::loadBigEndian<uint64_t>(data);
	int p = 8;

	int l = m_signer.unmarshal(data + p, len - p);
	if (l <= 0)
		return -1;
	p += l;

	if (unlikely(p + 2) > len)
		return -1;
	unsigned int endpointCount = Utils::loadBigEndian<uint16_t>(data + 8);
	if (unlikely(endpointCount > ZT_LOCATOR_MAX_ENDPOINTS))
		return -1;
	m_endpoints.resize(endpointCount);
	m_endpoints.shrink_to_fit();
	for (unsigned int i = 0;i < endpointCount;++i) {
		l = m_endpoints[i].unmarshal(data + p, len - p);
		if (l <= 0)
			return -1;
		p += l;
	}

	if (unlikely((p + 2) > len))
		return -1;
	p += 2 + (int) Utils::loadBigEndian<uint16_t>(data + p);

	if (unlikely((p + 2) > len))
		return -1;
	const unsigned int siglen = Utils::loadBigEndian<uint16_t>(data + p);
	p += 2;
	if (unlikely((siglen > ZT_SIGNATURE_BUFFER_SIZE) || ((p + (int) siglen) > len)))
		return -1;
	m_signature.unsafeSetSize(siglen);
	Utils::copy(m_signature.data(), data + p, siglen);
	p += siglen;
	if (unlikely(p > len))
		return -1;

	return p;
}

} // namespace ZeroTier

extern "C" {

ZT_Locator *ZT_Locator_create(
	int64_t ts,
	const ZT_Endpoint *endpoints,
	unsigned int endpointCount,
	const ZT_Identity *signer)
{
	try {
		if ((ts <= 0) || (!endpoints) || (endpointCount == 0) || (!signer))
			return nullptr;
		ZeroTier::Locator *loc = new ZeroTier::Locator();
		for (unsigned int i = 0;i < endpointCount;++i)
			loc->add(reinterpret_cast<const ZeroTier::Endpoint *>(endpoints)[i]);
		if (!loc->sign(ts, *reinterpret_cast<const ZeroTier::Identity *>(signer))) {
			delete loc;
			return nullptr;
		}
		return reinterpret_cast<ZT_Locator *>(loc);
	} catch (...) {
		return nullptr;
	}
}

ZT_Locator *ZT_Locator_fromString(const char *str)
{
	try {
		if (!str)
			return nullptr;
		ZeroTier::Locator *loc = new ZeroTier::Locator();
		if (!loc->fromString(str)) {
			delete loc;
			return nullptr;
		}
		return reinterpret_cast<ZT_Locator *>(loc);
	} catch ( ... ) {
		return nullptr;
	}
}

ZT_Locator *ZT_Locator_unmarshal(
	const void *data,
	unsigned int len)
{
	try {
		if ((!data) || (len == 0))
			return nullptr;
		ZeroTier::Locator *loc = new ZeroTier::Locator();
		if (loc->unmarshal(reinterpret_cast<const uint8_t *>(data), (int) len) <= 0) {
			delete loc;
			return nullptr;
		}
		return reinterpret_cast<ZT_Locator *>(loc);
	} catch (...) {
		return nullptr;
	}
}

int ZT_Locator_marshal(const ZT_Locator *loc, void *buf, unsigned int bufSize)
{
	if ((!loc) || (bufSize < ZT_LOCATOR_MARSHAL_SIZE_MAX))
		return -1;
	return reinterpret_cast<const ZeroTier::Locator *>(loc)->marshal(reinterpret_cast<uint8_t *>(buf), (int) bufSize);
}

char *ZT_Locator_toString(
	const ZT_Locator *loc,
	char *buf,
	int capacity)
{
	if ((!loc) || (capacity < ZT_LOCATOR_STRING_SIZE_MAX))
		return nullptr;
	return reinterpret_cast<const ZeroTier::Locator *>(loc)->toString(buf);
}

const ZT_Fingerprint *ZT_Locator_fingerprint(const ZT_Locator *loc)
{
	if (!loc)
		return nullptr;
	return (ZT_Fingerprint *) (&(reinterpret_cast<const ZeroTier::Locator *>(loc)->signer()));
}

int64_t ZT_Locator_timestamp(const ZT_Locator *loc)
{
	if (!loc)
		return 0;
	return reinterpret_cast<const ZeroTier::Locator *>(loc)->timestamp();
}

unsigned int ZT_Locator_endpointCount(const ZT_Locator *loc)
{
	return (loc) ? (unsigned int) (reinterpret_cast<const ZeroTier::Locator *>(loc)->endpoints().size()) : 0;
}

const ZT_Endpoint *ZT_Locator_endpoint(const ZT_Locator *loc, const unsigned int ep)
{
	if (!loc)
		return nullptr;
	if (ep >= (unsigned int) (reinterpret_cast<const ZeroTier::Locator *>(loc)->endpoints().size()))
		return nullptr;
	return reinterpret_cast<const ZT_Endpoint *>(&(reinterpret_cast<const ZeroTier::Locator *>(loc)->endpoints()[ep]));
}

int ZT_Locator_verify(const ZT_Locator *loc, const ZT_Identity *signer)
{
	if ((!loc) || (!signer))
		return 0;
	return reinterpret_cast<const ZeroTier::Locator *>(loc)->verify(*reinterpret_cast<const ZeroTier::Identity *>(signer)) ? 1 : 0;
}

void ZT_Locator_delete(ZT_Locator *loc)
{
	if (loc)
		delete reinterpret_cast<ZeroTier::Locator *>(loc);
}

} // C API functions
