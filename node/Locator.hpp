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

#include <vector>

#define ZT_LOCATOR_MAX_PHYSICAL_ENDPOINTS 32
#define ZT_LOCATOR_MAX_VIRTUAL_ENDPOINTS 32

namespace ZeroTier {

/**
 * Signed information about a node's location on the network
 */
class Locator
{
public:
	Locator() :
		_signatureLength(0),
		_orgSignatureLength(0) {}

	inline void addLocation(const InetAddress &phy) { if (_physical.size() < ZT_LOCATOR_MAX_PHYSICAL_ENDPOINTS) _physical.push_back(phy); }
	inline void addLocation(const Identity &v) { if (_virtual.size() < ZT_LOCATOR_MAX_VIRTUAL_ENDPOINTS) _virtual.push_back(v); }

	inline const std::vector<InetAddress> &physical() const { return _physical; }
	inline const std::vector<Identity> &virt() const { return _virtual; }

	void sign(const Identity &id,const Identity &organization,const int64_t timestamp);
	bool verify() const;

	void generateDNSRecords(char *buf,unsigned int buflen);

	template<unsigned int C>
	inline void serialize(Buffer<C> &b,const bool forSign = false) const
	{
		if (forSign) b.append((uint64_t)0x7f7f7f7f7f7f7f7fULL);

		b.append((uint64_t)_ts);
		_id.serialize(b,false);
		_organization.serialize(b,false);
		b.append((uint16_t)_physical.size());
		for(std::vector<InetAddress>::const_iterator i(_physical.begin());i!=_physical.end();++i)
			i->serialize(b);
		b.append((uint16_t)_virtual.size());
		for(std::vector<InetAddress>::const_iterator i(_virtual.begin());i!=_virtual.end();++i)
			i->serialize(b,false);
		if (!forSign) {
			b.append((uint16_t)_signatureLength);
			b.append(_signature,_signatureLength);
			b.append((uint16_t)_orgSignatureLength);
			b.append(_orgSignature,_orgSignatureLength);
		}
		b.append((uint16_t)0); // length of additional fields, currently 0

		if (forSign) b.append((uint64_t)0x7f7f7f7f7f7f7f7fULL);
	}

	template<unsigned int C>
	inline unsigned int deserialize(const Buffer<C> &b,unsigned int startAt = 0)
	{
		unsigned int p = startAt;

		_ts = (uint64_t)b.template at<uint64_t>(p); p += 8;
		p += _id.deserialize(b,p);
		p += _organization.deserialize(b,p);
		unsigned int cnt = b.template at<uint16_t>(p); p += 2;
		if (cnt > ZT_LOCATOR_MAX_PHYSICAL_ENDPOINTS)
			throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_OVERFLOW;
		_physical.resize(cnt);
		for(std::vector<InetAddress>::iterator i(_physical.begin());i!=_physical.end();++i)
			p += i->deserialize(b,p);
		cnt = b.template at<uint16_t>(p); p += 2;
		if (cnt > ZT_LOCATOR_MAX_VIRTUAL_ENDPOINTS)
			throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_OVERFLOW;
		_virtual.resize(cnt);
		for(std::vector<Identity>::iterator i(_virtual.begin());i!=_virtual.end();++i)
			p += i->deserialize(b,p);
		p += 2 + b.template at<uint16_t>(p);
		if (p > b.size())
			throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_OVERFLOW;

		return (p - startAt);
	}

private:
	int64_t _ts;
	Identity _id;
	Identity _organization;
	std::vector<InetAddress> _physical;
	std::vector<Identity> _virtual;
	unsigned int _signatureLength;
	unsigned int _orgSignatureLength;
	uint8_t _signature[ZT_SIGNATURE_BUFFER_SIZE];
	uint8_t _orgSignature[ZT_SIGNATURE_BUFFER_SIZE];
};

} // namespace ZeroTier

#endif
