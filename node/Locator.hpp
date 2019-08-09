/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifndef ZT_LOCATOR_HPP
#define ZT_LOCATOR_HPP

#include "Constants.hpp"
#include "Identity.hpp"
#include "InetAddress.hpp"
#include "Utils.hpp"
#include "Buffer.hpp"
#include "SHA512.hpp"
#include "Str.hpp"

#include <algorithm>
#include <vector>

#define ZT_LOCATOR_MAX_PHYSICAL_ADDRESSES 255
#define ZT_LOCATOR_MAX_VIRTUAL_ADDRESSES 255

namespace ZeroTier {

/**
 * Signed information about a node's location on the network
 * 
 * A locator can be stored in DNS as a series of TXT records with a DNS name
 * that includes a public key that can be used to validate the locator's
 * signature. That way DNS records can't be spoofed even if no DNSSEC or
 * anything else is present to secure DNS.
 */
class Locator
{
public:
	Locator() : _signatureLength(0) {}

	inline const std::vector<InetAddress> &phy() const { return _physical; }
	inline const std::vector<Identity> &virt() const { return _virtual; }

	inline void add(const InetAddress &ip)
	{
		if (_physical.size() < ZT_LOCATOR_MAX_PHYSICAL_ADDRESSES)
			_physical.push_back(ip);
	}
	inline void add(const Identity &zt)
	{
		if (_virtual.size() < ZT_LOCATOR_MAX_VIRTUAL_ADDRESSES)
			_virtual.push_back(zt);
	}

	inline void finish(const Identity &id,const int64_t ts)
	{
		_ts = ts;
		_id = id;
		std::sort(_physical.begin(),_physical.end());
		_physical.erase(std::unique(_physical.begin(),_physical.end()),_physical.end());
		std::sort(_virtual.begin(),_virtual.end());
		_virtual.erase(std::unique(_virtual.begin(),_virtual.end()),_virtual.end());
	}

	inline bool sign(const Identity &signingId)
	{
		if (!signingId.hasPrivate())
			return false;
		if (signingId == _id) {
			_signedBy.zero();
		} else {
			_signedBy = signingId;
		}
		Buffer<65536> *tmp = new Buffer<65536>();
		try {
			serialize(*tmp,true);
			_signatureLength = signingId.sign(tmp->data(),tmp->size(),_signature,ZT_SIGNATURE_BUFFER_SIZE);
			delete tmp;
			return (_signatureLength > 0);
		} catch ( ... ) {
			delete tmp;
			return false;
		}
	}

	inline bool verify() const
	{
		if ((_signatureLength == 0)||(_signatureLength > sizeof(_signature)))
			return false;
		Buffer<65536> *tmp = nullptr;
		try {
			tmp = new Buffer<65536>();
			serialize(*tmp,true);
			const bool ok = (_signedBy) ? _signedBy.verify(tmp->data(),tmp->size(),_signature,_signatureLength) : _id.verify(tmp->data(),tmp->size(),_signature,_signatureLength);
			delete tmp;
			return ok;
		} catch ( ... ) {
			if (tmp) delete tmp;
			return false;
		}
	}

	inline std::vector<Str> makeTxtRecords(const uint8_t p384SigningKeyPublic[ZT_ECC384_PUBLIC_KEY_SIZE],const uint8_t p384SigningKeyPrivate[ZT_ECC384_PUBLIC_KEY_SIZE])
	{
		uint8_t s384[48],dnsSig[ZT_ECC384_SIGNATURE_SIZE];
		char enc[256];

		Buffer<65536> *const tmp = new Buffer<65536>();
		serialize(*tmp,false);
		SHA384(s384,tmp->data(),tmp->size());
		ECC384ECDSASign(p384SigningKeyPrivate,s384,dnsSig);
		tmp->append(dnsSig,ZT_ECC384_SIGNATURE_SIZE);

		// Blob must be broken into multiple TXT records that must remain sortable so they are prefixed by a hex value.
		// 186-byte chunks yield 248-byte base64 chunks which leaves some margin below the limit of 255.
		std::vector<Str> txtRecords;
		for(unsigned int p=0;p<tmp->size();p+=186) {
			unsigned int rem = tmp->size() - p;
			if (rem > 186) rem = 186;
			Utils::b64e(((const uint8_t *)tmp->data()) + p,rem,enc,sizeof(enc));
			txtRecords.push_back(Str());
			txtRecords.back() << Utils::HEXCHARS[(p >> 4) & 0xf] << Utils::HEXCHARS[p & 0xf] << enc;
		}

		delete tmp;
		return txtRecords;
	}

	template<typename I>
	inline bool decodeTxtRecords(I start,I end,const uint8_t p384SigningKeyPublic[ZT_ECC384_PUBLIC_KEY_SIZE])
	{
		uint8_t dec[256],s384[48];
		Buffer<65536> *tmp = nullptr;
		try {
			tmp = new Buffer<65536>();
			while (start != end) {
				tmp->append(dec,Utils::b64d(start->c_str(),dec,sizeof(dec)));
				++start;
			}

			if (tmp->size() <= ZT_ECC384_SIGNATURE_SIZE) {
				delete tmp;
				return false;
			}
			SHA384(s384,tmp->data(),tmp->size() - ZT_ECC384_SIGNATURE_SIZE);
			if (!ECC384ECDSAVerify(p384SigningKeyPublic,s384,((const uint8_t *)tmp->data()) + (tmp->size() - ZT_ECC384_SIGNATURE_SIZE))) {
				delete tmp;
				return false;
			}

			deserialize(*tmp,0);

			delete tmp;
			return verify();
		} catch ( ... ) {
			if (tmp) delete tmp;
			return false;
		}
	}

	template<unsigned int C>
	inline void serialize(Buffer<C> &b,const bool forSign = false) const
	{
		if (forSign) b.append((uint64_t)0x7f7f7f7f7f7f7f7fULL);

		b.append((uint8_t)0); // version/flags, currently 0
		b.append((uint64_t)_ts);
		_id.serialise(b,false);
		if (_signedBy) {
			b.append((uint8_t)1); // number of signers, current max is 1
			_signedBy.serialize(b,false);
		} else {
			b.append((uint8_t)0); // signer is _id
		}
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
		const unsigned int signerCount = b[p++];
		if (signerCount > 1)
			throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_OVERFLOW;
		if (signerCount == 1) {
			p += _signedBy.deserialize(b,p);
		} else {
			_signedBy.zero();
		}
		const unsigned int physicalCount = b[p++];
		_physical.resize(physicalCount);
		for(unsigned int i=0;i<physicalCount;++i)
			p += _physical[i].deserialize(b,p);
		const unsigned int virtualCount = b[p++];
		_virtual.resize(virtualCount);
		for(unsigned int i=0;i<virtualCount;++i)
			p += _virtual[i].deserialize(b,p);
		_signatureLen = b.template at<uint16_t>(p); p += 2;
		if (_signatureLength > ZT_SIGNATURE_BUFFER_SIZE)
			throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_OVERFLOW;
		memcpy(_signature,b.field(p,_signatureLength),_signatureLength);
		p += _signatureLength;
		p += b.template at<uint16_t>(p); p += 2;
		if (p > b.size())
			throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_OVERFLOW;

		return (p - startAt);
	}

private:
	int64_t _ts;
	Identity _id;
	Identity _signedBy; // signed by _id if nil/zero
	std::vector<InetAddress> _physical;
	std::vector<Identity> _virtual;
	unsigned int _signatureLength;
	uint8_t _signature[ZT_SIGNATURE_BUFFER_SIZE];
};

} // namespace ZeroTier

#endif
