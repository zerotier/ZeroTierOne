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

#ifndef ZT_MULTICASTSUBSCRIPTIONS_HPP
#define ZT_MULTICASTSUBSCRIPTIONS_HPP

#include "Constants.hpp"
#include "MulticastGroup.hpp"
#include "Identity.hpp"
#include "Buffer.hpp"

namespace ZeroTier {

/**
 * A compact collection of multicast subscriptions identified by 32-bit hash values
 */
class MulticastSubscriptions
{
public:
	inline MulticastSubscriptions() : _signatureLength(0) {}

	inline void add(const MulticastGroup &mg)
	{
		if (_subscriptions.size() < ZT_MAX_MULTICAST_SUBSCRIPTIONS)
			_subscriptions.push_back(mg.id32());
	}

	inline bool sign(const Identity &signer,const int64_t ts)
	{
		_ts = ts;
		std::sort(_subscriptions.begin(),_subscriptions.end());
		_subscriptions.erase(std::unique(_subscriptions.begin(),_subscriptions.end()),_subscriptions.end());

		_SRec tmp;
		tmp.ts = Utils::hton((uint64_t)ts);
		for(unsigned long i=0,j=(unsigned long)_subscriptions.size();i<j;++i)
			tmp.g[i] = Utils::hton(_subscriptions[i]);

		_signatureLength = signer.sign(&tmp,(unsigned int)((_subscriptions.size() * sizeof(uint32_t)) + sizeof(uint64_t)),_signature,sizeof(_signature));
		return (_signatureLength > 0);
	}

	inline bool verify(const Identity &signer)
	{
		if ((_signatureLength == 0)||(_signatureLength > ZT_SIGNATURE_BUFFER_SIZE))
			return false;
		_SRec tmp;
		tmp.ts = Utils::hton((uint64_t)_ts);
		for(unsigned long i=0,j=(unsigned long)_subscriptions.size();i<j;++i)
			tmp.g[i] = Utils::hton(_subscriptions[i]);
		return signer.verify(&tmp,(unsigned int)((_subscriptions.size() * sizeof(uint32_t)) + sizeof(uint64_t)),_signature,_signatureLength);
	}

	inline int64_t timestamp() const { return _ts; }
	inline unsigned int count() const { return (unsigned int)_subscriptions.size(); }
	inline bool contains(const MulticastGroup &mg) const { return std::binary_search(_subscriptions.begin(),_subscriptions.end(),mg.id32()); }

	template<unsigned int C>
	inline void serialize(Buffer<C> &b) const
	{
		b.append((uint64_t)_ts);
		b.append((uint16_t)_subscriptions.size());
		for(std::vector<uint32_t>::const_iterator i(_subscriptions.begin());i!=_subscriptions.end();++i)
			b.append(*i);
		b.append((uint16_t)_signatureLength);
		b.append(_signature,_signatureLength);
	}

	template<unsigned int C>
	inline unsigned int deserialize(const Buffer<C> &b,unsigned int startAt = 0)
	{
		unsigned int p = startAt;
		_ts = (int64_t)(b.template at<uint64_t>(p)); p += 8;
		_subscriptions.resize(b.template at<uint16_t>(p)); p += 2;
		for(std::vector<uint32_t>::iterator i(_subscriptions.begin());i!=_subscriptions.end();++i) {
			*i = b.template at<uint32_t>(p);
			p += 4;
		}
		_signatureLength = b.template at<uint16_t>(p); p += 2;
		if (_signatureLength > ZT_SIGNATURE_BUFFER_SIZE)
			throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_OVERFLOW;
		memcpy(_signature,b.field(p,_signatureLength),_signatureLength); p += _signatureLength;
		return (p - startAt);
	}

private:
	ZT_PACKED_STRUCT(struct _SRec {
		uint64_t ts;
		uint32_t g[ZT_MAX_MULTICAST_SUBSCRIPTIONS];
	});

	int64_t _ts;
	std::vector<uint32_t> _subscriptions;
	unsigned int _signatureLength;
	uint8_t _signature[ZT_SIGNATURE_BUFFER_SIZE];
};

} // namespace ZeroTier

#endif
