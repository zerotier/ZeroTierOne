/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_LOCATOR_HPP
#define ZT_LOCATOR_HPP

#include "Constants.hpp"
#include "Identity.hpp"
#include "InetAddress.hpp"
#include "Utils.hpp"
#include "Buffer.hpp"
#include "SHA512.hpp"
#include "Str.hpp"
#include "ScopedPtr.hpp"

#include <algorithm>
#include <vector>

// These are absolute maximums -- real locators are never this big
#define ZT_LOCATOR_MAX_PHYSICAL_ADDRESSES 255
#define ZT_LOCATOR_MAX_VIRTUAL_ADDRESSES 255

namespace ZeroTier {

/**
 * Signed information about a node's location on the network
 *
 * A locator is a signed record that contains information about where a node
 * may be found. It can contain static physical addresses or virtual ZeroTier
 * addresses of nodes that can forward to the target node. Locator records
 * can be stored in signed DNS TXT record sets, in LF by roots, in caches,
 * etc.
 */
class Locator
{
public:
	ZT_ALWAYS_INLINE Locator() : _ts(0),_signatureLength(0) {}

	ZT_ALWAYS_INLINE int64_t timestamp() const { return _ts; }
	ZT_ALWAYS_INLINE const Identity &id() const { return _id; }

	ZT_ALWAYS_INLINE const std::vector<InetAddress> &phy() const { return _physical; }
	ZT_ALWAYS_INLINE const std::vector<Identity> &virt() const { return _virtual; }

	/**
	 * Add a physical address to this locator (call before finish() to build a new Locator)
	 */
	ZT_ALWAYS_INLINE void add(const InetAddress &ip)
	{
		if (_physical.size() < ZT_LOCATOR_MAX_PHYSICAL_ADDRESSES)
			_physical.push_back(ip);
	}

	/**
	 * Add a forwarding ZeroTier node to this locator (call before finish() to build a new Locator)
	 */
	ZT_ALWAYS_INLINE void add(const Identity &zt)
	{
		if (_virtual.size() < ZT_LOCATOR_MAX_VIRTUAL_ADDRESSES)
			_virtual.push_back(zt);
	}

	/**
	 * Method to be called after add() is called for each address or forwarding node
	 *
	 * @param id Identity that this locator describes (must contain private key)
	 * @param ts Current time
	 * @return True if completion and signature were successful
	 */
	ZT_ALWAYS_INLINE bool finish(const Identity &id,const int64_t ts)
	{
		_ts = ts;
		_id = id;
		std::sort(_physical.begin(),_physical.end());
		_physical.erase(std::unique(_physical.begin(),_physical.end()),_physical.end());
		std::sort(_virtual.begin(),_virtual.end());
		_virtual.erase(std::unique(_virtual.begin(),_virtual.end()),_virtual.end());
		try {
			ScopedPtr< Buffer<65536> > tmp(new Buffer<65536>());
			serialize(*tmp,true);
			_signatureLength = id.sign(tmp->data(),tmp->size(),_signature,ZT_SIGNATURE_BUFFER_SIZE);
			return (_signatureLength > 0);
		} catch ( ... ) {
			return false;
		}
	}

	/**
	 * Verify this locator's signature against its embedded signing identity
	 */
	ZT_ALWAYS_INLINE bool verify() const
	{
		if ((_signatureLength == 0)||(_signatureLength > sizeof(_signature)))
			return false;
		try {
			ScopedPtr< Buffer<65536> > tmp(new Buffer<65536>());
			serialize(*tmp,true);
			return _id.verify(tmp->data(),tmp->size(),_signature,_signatureLength);
		} catch ( ... ) {
			return false;
		}
	}

	/**
	 * Make a DNS name contiaining a public key that can sign DNS entries
	 *
	 * This generates the initial fields of a DNS name that contains an
	 * encoded public key. Users may append any domain suffix to this name.
	 *
	 * @return First field(s) of DNS name
	 */
	static inline Str makeSecureDnsName(const uint8_t p384SigningKeyPublic[ZT_ECC384_PUBLIC_KEY_SIZE])
	{
		uint8_t tmp[ZT_ECC384_PUBLIC_KEY_SIZE+2];
		memcpy(tmp,p384SigningKeyPublic,ZT_ECC384_PUBLIC_KEY_SIZE);
		const uint16_t crc = Utils::crc16(tmp,ZT_ECC384_PUBLIC_KEY_SIZE);
		tmp[ZT_ECC384_PUBLIC_KEY_SIZE-2] = (uint8_t)(crc >> 8);
		tmp[ZT_ECC384_PUBLIC_KEY_SIZE-1] = (uint8_t)(crc);
		Str name;
		char b32[128];
		Utils::b32e(tmp,35,b32,sizeof(b32));
		name << "ztl-";
		name << b32;
		Utils::b32e(tmp + 35,(ZT_ECC384_PUBLIC_KEY_SIZE+2) - 35,b32,sizeof(b32));
		name << ".ztl-";
		name << b32;
		return name;
	}

	/**
	 * This searches for an extracts a public key from a DNS name, if one is present.
	 *
	 * @return True if a key was found and successfully decoded
	 */
	static inline bool decodeSecureDnsName(const char *name,uint8_t p384SigningKeyPublic[ZT_ECC384_PUBLIC_KEY_SIZE])
	{
		uint8_t b32[128];
		unsigned int b32ptr = 0;
		char tmp[1024];
		Utils::scopy(tmp,sizeof(tmp),name);
		bool ok = false;
		for(char *saveptr=(char *)0,*p=Utils::stok(tmp,".",&saveptr);p;p=Utils::stok((char *)0,".",&saveptr)) {
			if (b32ptr >= sizeof(b32))
				break;
			if ((strlen(p) <= 4)||(memcmp(p,"ztl-",4) != 0))
				continue;
			int s = Utils::b32d(p + 4,b32 + b32ptr,sizeof(b32) - b32ptr);
			if (s > 0) {
				b32ptr += (unsigned int)s;
				if (b32ptr > 2) {
					const uint16_t crc = Utils::crc16(b32,b32ptr);
					if ((b32[b32ptr-2] == (uint8_t)(crc >> 8))&&(b32[b32ptr-1] == (uint8_t)(crc & 0xff))) {
						ok = true;
						break;
					}
				}
			} else break;
		}

		if (ok) {
			if (b32ptr == (ZT_ECC384_PUBLIC_KEY_SIZE + 2)) {
				memcpy(p384SigningKeyPublic,b32,ZT_ECC384_PUBLIC_KEY_SIZE);
				return true;
			}
		}

		return false;
	}

	/**
	 * Make DNS TXT records for this locator
	 *
	 * DNS TXT records are signed by an entirely separate key that is added along
	 * with DNS names to nodes to allow them to verify DNS results. It's separate
	 * from the locator's signature so that a single DNS record can point to more
	 * than one locator or be served by things like geo-aware DNS.
	 *
	 * Right now only NIST P-384 is supported for signing DNS records. NIST EDDSA
	 * is used here so that FIPS-only nodes can always use DNS to locate roots as
	 * FIPS-only nodes may be required to disable non-FIPS algorithms.
	 */
	inline std::vector<Str> makeTxtRecords(const uint8_t p384SigningKeyPrivate[ZT_ECC384_PUBLIC_KEY_SIZE])
	{
		uint8_t s384[48];
		char enc[256];

		ScopedPtr< Buffer<65536> > tmp(new Buffer<65536>());
		serialize(*tmp,false);
		SHA384(s384,tmp->data(),tmp->size());
		const unsigned int sigLocation = tmp->size();
		tmp->addSize(ZT_ECC384_SIGNATURE_SIZE);
		ECC384ECDSASign(p384SigningKeyPrivate,s384,((uint8_t *)tmp->unsafeData()) + sigLocation);

		// Blob must be broken into multiple TXT records that must remain sortable so they are prefixed by a hex value.
		// 186-byte chunks yield 248-byte base64 chunks which leaves some margin below the limit of 255.
		std::vector<Str> txtRecords;
		unsigned int txtRecNo = 0;
		for(unsigned int p=0;p<tmp->size();) {
			unsigned int chunkSize = tmp->size() - p;
			if (chunkSize > 186) chunkSize = 186;

			Utils::b64e(((const uint8_t *)tmp->data()) + p,chunkSize,enc,sizeof(enc));
			p += chunkSize;

			txtRecords.push_back(Str());
			txtRecords.back() << Utils::HEXCHARS[(txtRecNo >> 4) & 0xf] << Utils::HEXCHARS[txtRecNo & 0xf] << enc;
			++txtRecNo;
		}

		return txtRecords;
	}

	/**
	 * Decode TXT records
	 *
	 * TXT records can be provided as an iterator over std::string, Str, or char *
	 * values, and TXT records can be provided in any order. Any oversize or empty
	 * entries will be ignored.
	 *
	 * This method checks the decoded locator's signature using the supplied DNS TXT
	 * record signing public key. False is returned if the TXT records are invalid,
	 * incomplete, or fail signature check. If true is returned this Locator object
	 * now contains the contents of the supplied TXT records.
	 *
	 * @return True if new Locator is valid
	 */
	template<typename I>
	inline bool decodeTxtRecords(const Str &dnsName,I start,I end)
	{
		uint8_t dec[256],s384[48];
		try {
			std::vector<Str> txtRecords;
			while (start != end) {
				try {
					if (start->length() > 2)
						txtRecords.push_back(*start);
				} catch ( ... ) {} // skip any records that trigger out of bounds exceptions
				++start;
			}
			if (txtRecords.empty())
				return false;
			std::sort(txtRecords.begin(),txtRecords.end());

			ScopedPtr< Buffer<65536> > tmp(new Buffer<65536>());
			for(std::vector<Str>::const_iterator i(txtRecords.begin());i!=txtRecords.end();++i)
				tmp->append(dec,Utils::b64d(i->c_str() + 2,dec,sizeof(dec)));

			uint8_t p384SigningKeyPublic[ZT_ECC384_PUBLIC_KEY_SIZE];
			if (decodeSecureDnsName(dnsName.c_str(),p384SigningKeyPublic)) {
				if (tmp->size() <= ZT_ECC384_SIGNATURE_SIZE)
					return false;
				SHA384(s384,tmp->data(),tmp->size() - ZT_ECC384_SIGNATURE_SIZE);
				if (!ECC384ECDSAVerify(p384SigningKeyPublic,s384,((const uint8_t *)tmp->data()) + (tmp->size() - ZT_ECC384_SIGNATURE_SIZE)))
					return false;
			}

			deserialize(*tmp,0);

			return verify();
		} catch ( ... ) {
			return false;
		}
	}

	inline bool deserialize(const void *data,unsigned int len)
	{
		ScopedPtr< Buffer<65536> > tmp(new Buffer<65536>());
		tmp->append(data,len);
		try {
			deserialize(*tmp,0);
			return true;
		} catch ( ... ) {
			return false;
		}
	}

	template<unsigned int C>
	inline void serialize(Buffer<C> &b,const bool forSign = false) const
	{
		if (forSign) b.append((uint64_t)0x7f7f7f7f7f7f7f7fULL);

		b.append((uint8_t)0); // version/flags, currently 0
		b.append((uint64_t)_ts);
		_id.serialize(b,false);
		b.append((uint8_t)_physical.size());
		for(std::vector<InetAddress>::const_iterator i(_physical.begin());i!=_physical.end();++i)
			i->serialize(b);
		b.append((uint8_t)_virtual.size());
		for(std::vector<Identity>::const_iterator i(_virtual.begin());i!=_virtual.end();++i)
			i->serialize(b,false);
		if (!forSign) {
			b.append((uint16_t)_signatureLength);
			b.append(_signature,_signatureLength);
		}
		b.append((uint16_t)0); // length of additional fields, currently 0

		if (forSign) b.append((uint64_t)0x7f7f7f7f7f7f7f7fULL);
	}

	template<unsigned int C>
	inline unsigned int deserialize(const Buffer<C> &b,unsigned int startAt = 0)
	{
		unsigned int p = startAt;

		if (b[p++] != 0)
			throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_INVALID_TYPE;
		_ts = (int64_t)b.template at<uint64_t>(p); p += 8;
		p += _id.deserialize(b,p);
		const unsigned int physicalCount = b[p++];
		_physical.resize(physicalCount);
		for(unsigned int i=0;i<physicalCount;++i)
			p += _physical[i].deserialize(b,p);
		const unsigned int virtualCount = b[p++];
		_virtual.resize(virtualCount);
		for(unsigned int i=0;i<virtualCount;++i)
			p += _virtual[i].deserialize(b,p);
		_signatureLength = b.template at<uint16_t>(p); p += 2;
		if (_signatureLength > ZT_SIGNATURE_BUFFER_SIZE)
			throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_OVERFLOW;
		memcpy(_signature,b.field(p,_signatureLength),_signatureLength);
		p += _signatureLength;
		p += b.template at<uint16_t>(p); p += 2;
		if (p > b.size())
			throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_OVERFLOW;

		return (p - startAt);
	}

	ZT_ALWAYS_INLINE operator bool() const { return (_id); }

	ZT_ALWAYS_INLINE bool addressesEqual(const Locator &l) const { return ((_physical == l._physical)&&(_virtual == l._virtual)); }

	ZT_ALWAYS_INLINE bool operator==(const Locator &l) const
	{
		return (
			(_ts == l._ts)&&
			(_id == l._id)&&
			(_physical == l._physical)&&
			(_virtual == l._virtual)&&
			(_signatureLength == l._signatureLength)&&
			(memcmp(_signature,l._signature,_signatureLength) == 0));
	}
	ZT_ALWAYS_INLINE bool operator!=(const Locator &l) const { return (!(*this == l)); }
	ZT_ALWAYS_INLINE bool operator<(const Locator &l) const
	{
		if (_ts < l._ts) return true; else if (_ts > l._ts) return false;
		if (_id < l._id) return true; else if (_id > l._id) return false;
		if (_physical < l._physical) return true; else if (_physical > l._physical) return false;
		if (_virtual < l._virtual) return true; else if (_virtual > l._virtual) return false;
		if (_signatureLength < l._signatureLength) return true;
		return (_signatureLength == l._signatureLength) ? (memcmp(_signature,l._signature,_signatureLength) < 0) : false;
	}
	ZT_ALWAYS_INLINE bool operator>(const Locator &l) const { return (l < *this); }
	ZT_ALWAYS_INLINE bool operator<=(const Locator &l) const { return (!(l < *this)); }
	ZT_ALWAYS_INLINE bool operator>=(const Locator &l) const { return (!(*this < l)); }

	ZT_ALWAYS_INLINE unsigned long hashCode() const { return (unsigned long)(_id.address().toInt() ^ (uint64_t)_ts); }

private:
	int64_t _ts;
	Identity _id;
	std::vector<InetAddress> _physical;
	std::vector<Identity> _virtual;
	unsigned int _signatureLength;
	uint8_t _signature[ZT_SIGNATURE_BUFFER_SIZE];
};

} // namespace ZeroTier

#endif
