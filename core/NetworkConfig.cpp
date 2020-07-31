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

#include <cstdint>

#include <algorithm>

#include "NetworkConfig.hpp"
#include "ScopedPtr.hpp"
#include "Buf.hpp"

namespace ZeroTier {

bool NetworkConfig::toDictionary(Dictionary &d) const
{
	uint8_t tmp[ZT_BUF_MEM_SIZE];
	try {
		d.clear();

		d.add(ZT_NETWORKCONFIG_DICT_KEY_NETWORK_ID,this->networkId);
		d.add(ZT_NETWORKCONFIG_DICT_KEY_TIMESTAMP,this->timestamp);
		d.add(ZT_NETWORKCONFIG_DICT_KEY_CREDENTIAL_TIME_MAX_DELTA,this->credentialTimeMaxDelta);
		d.add(ZT_NETWORKCONFIG_DICT_KEY_REVISION,this->revision);
		d.add(ZT_NETWORKCONFIG_DICT_KEY_ISSUED_TO,this->issuedTo.toString((char *)tmp));
		d.add(ZT_NETWORKCONFIG_DICT_KEY_ISSUED_TO_IDENTITY_HASH,this->issuedToFingerprintHash,ZT_FINGERPRINT_HASH_SIZE);
		d.add(ZT_NETWORKCONFIG_DICT_KEY_FLAGS,this->flags);
		d.add(ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_LIMIT,(uint64_t)this->multicastLimit);
		d.add(ZT_NETWORKCONFIG_DICT_KEY_TYPE,(uint64_t)this->type);
		d.add(ZT_NETWORKCONFIG_DICT_KEY_NAME,this->name);
		d.add(ZT_NETWORKCONFIG_DICT_KEY_MTU,(uint64_t)this->mtu);

		if (this->com) {
			d.add(ZT_NETWORKCONFIG_DICT_KEY_COM,tmp,this->com.marshal(tmp));
		}

		Vector<uint8_t> *blob = &(d[ZT_NETWORKCONFIG_DICT_KEY_CAPABILITIES]);
		for (unsigned int i = 0; i < this->capabilityCount; ++i) {
			int l = this->capabilities[i].marshal(tmp);
			if (l < 0)
				return false;
			blob->insert(blob->end(),tmp,tmp + l);
		}

		blob = &(d[ZT_NETWORKCONFIG_DICT_KEY_TAGS]);
		for (unsigned int i = 0; i < this->tagCount; ++i) {
			int l = this->tags[i].marshal(tmp);
			if (l < 0)
				return false;
			blob->insert(blob->end(),tmp,tmp + l);
		}

		blob = &(d[ZT_NETWORKCONFIG_DICT_KEY_CERTIFICATES_OF_OWNERSHIP]);
		for (unsigned int i = 0; i < this->certificateOfOwnershipCount; ++i) {
			int l = this->certificatesOfOwnership[i].marshal(tmp);
			if (l < 0)
				return false;
			blob->insert(blob->end(),tmp,tmp + l);
		}

		blob = &(d[ZT_NETWORKCONFIG_DICT_KEY_SPECIALISTS]);
		for (unsigned int i = 0; i < this->specialistCount; ++i) {
			Utils::storeBigEndian<uint64_t>(tmp,this->specialists[i]);
			blob->insert(blob->end(),tmp,tmp + 8);
		}

		blob = &(d[ZT_NETWORKCONFIG_DICT_KEY_ROUTES]);
		for (unsigned int i = 0; i < this->routeCount; ++i) {
			int l = asInetAddress(this->routes[i].target).marshal(tmp);
			if (l < 0)
				return false;
			blob->insert(blob->end(),tmp,tmp + l);
			l = asInetAddress(this->routes[i].via).marshal(tmp);
			if (l < 0)
				return false;
			blob->insert(blob->end(),tmp,tmp + l);
		}

		blob = &(d[ZT_NETWORKCONFIG_DICT_KEY_STATIC_IPS]);
		for (unsigned int i = 0; i < this->staticIpCount; ++i) {
			int l = this->staticIps[i].marshal(tmp);
			if (l < 0)
				return false;
			blob->insert(blob->end(),tmp,tmp + l);
		}

		blob = &(d[ZT_NETWORKCONFIG_DICT_KEY_RULES]);
		if (this->ruleCount) {
			blob->resize(ruleCount * ZT_VIRTUALNETWORKRULE_MARSHAL_SIZE_MAX);
			int l = CapabilityCredential::marshalVirtualNetworkRules(blob->data(), rules, ruleCount);
			if (l > 0)
				blob->resize(l);
		}

		return true;
	} catch ( ... ) {}
	return false;
}

bool NetworkConfig::fromDictionary(const Dictionary &d)
{
	static const NetworkConfig NIL_NC;
	try {
		*this = NIL_NC;

		this->networkId = d.getUI(ZT_NETWORKCONFIG_DICT_KEY_NETWORK_ID,0);
		if (!this->networkId)
			return false;
		this->timestamp = d.getUI(ZT_NETWORKCONFIG_DICT_KEY_TIMESTAMP,0);
		if (this->timestamp <= 0)
			return false;
		this->credentialTimeMaxDelta = d.getUI(ZT_NETWORKCONFIG_DICT_KEY_CREDENTIAL_TIME_MAX_DELTA,0);
		this->revision = d.getUI(ZT_NETWORKCONFIG_DICT_KEY_REVISION,0);
		this->issuedTo = d.getUI(ZT_NETWORKCONFIG_DICT_KEY_ISSUED_TO,0);
		const Vector<uint8_t> *blob = &(d[ZT_NETWORKCONFIG_DICT_KEY_ISSUED_TO_IDENTITY_HASH]);
		if (blob->size() == ZT_FINGERPRINT_HASH_SIZE) {
			Utils::copy<ZT_FINGERPRINT_HASH_SIZE>(this->issuedToFingerprintHash,blob->data());
		} else {
			Utils::zero<ZT_FINGERPRINT_HASH_SIZE>(this->issuedToFingerprintHash);
		}
		if (!this->issuedTo)
			return false;
		this->multicastLimit = (unsigned int)d.getUI(ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_LIMIT,0);
		d.getS(ZT_NETWORKCONFIG_DICT_KEY_NAME,this->name,sizeof(this->name));
		this->mtu = (unsigned int)d.getUI(ZT_NETWORKCONFIG_DICT_KEY_MTU,ZT_DEFAULT_MTU);
		if (this->mtu < 1280)
			this->mtu = 1280; // minimum MTU allowed by IPv6 standard and others
		else if (this->mtu > ZT_MAX_MTU)
			this->mtu = ZT_MAX_MTU;

		if (d.getUI(ZT_NETWORKCONFIG_DICT_KEY_VERSION,0) < 6) {
			return false;
		} else {
			this->flags = d.getUI(ZT_NETWORKCONFIG_DICT_KEY_FLAGS,0);
			this->type = (ZT_VirtualNetworkType)d.getUI(ZT_NETWORKCONFIG_DICT_KEY_TYPE,(uint64_t)ZT_NETWORK_TYPE_PRIVATE);

			blob = &(d[ZT_NETWORKCONFIG_DICT_KEY_COM]);
			if (!blob->empty()) {
				if (this->com.unmarshal(blob->data(),(int)(blob->size()) < 0))
					return false;
			}

			blob = &(d[ZT_NETWORKCONFIG_DICT_KEY_CAPABILITIES]);
			if (!blob->empty()) {
				try {
					unsigned int p = 0;
					while (p < blob->size()) {
						CapabilityCredential cap;
						int l = cap.unmarshal(blob->data() + p,(int)(blob->size() - p));
						if (l < 0)
							return false;
						p += l;
						if (this->capabilityCount < ZT_MAX_NETWORK_CAPABILITIES)
							this->capabilities[this->capabilityCount++] = cap;
					}
				} catch ( ... ) {}
				std::sort(&(this->capabilities[0]),&(this->capabilities[this->capabilityCount]));
			}

			blob = &(d[ZT_NETWORKCONFIG_DICT_KEY_TAGS]);
			if (!blob->empty()) {
				try {
					unsigned int p = 0;
					while (p < blob->size()) {
						TagCredential tag;
						int l = tag.unmarshal(blob->data() + p,(int)(blob->size() - p));
						if (l < 0)
							return false;
						p += l;
						if (this->tagCount < ZT_MAX_NETWORK_TAGS)
							this->tags[this->tagCount++] = tag;
					}
				} catch ( ... ) {}
				std::sort(&(this->tags[0]),&(this->tags[this->tagCount]));
			}

			blob = &(d[ZT_NETWORKCONFIG_DICT_KEY_CERTIFICATES_OF_OWNERSHIP]);
			if (!blob->empty()) {
				try {
					unsigned int p = 0;
					while (p < blob->size()) {
						OwnershipCredential coo;
						int l = coo.unmarshal(blob->data() + p,(int)(blob->size() - p));
						if (l < 0)
							return false;
						p += l;
						if (this->certificateOfOwnershipCount < ZT_MAX_CERTIFICATES_OF_OWNERSHIP)
							this->certificatesOfOwnership[certificateOfOwnershipCount++] = coo;
					}
				} catch ( ... ) {}
				std::sort(&(this->certificatesOfOwnership[0]),&(this->certificatesOfOwnership[this->certificateOfOwnershipCount]));
			}

			blob = &(d[ZT_NETWORKCONFIG_DICT_KEY_SPECIALISTS]);
			if (!blob->empty()) {
				unsigned int p = 0;
				while (((p + 8) <= blob->size())&&(specialistCount < ZT_MAX_NETWORK_SPECIALISTS)) {
					this->specialists[this->specialistCount++] = Utils::loadBigEndian<uint64_t>(blob->data() + p);
					p += 8;
				}
			}

			blob = &(d[ZT_NETWORKCONFIG_DICT_KEY_ROUTES]);
			if (!blob->empty()) {
				unsigned int p = 0;
				while ((p < blob->size())&&(routeCount < ZT_MAX_NETWORK_ROUTES)) {
					int l = asInetAddress(this->routes[this->routeCount].target).unmarshal(blob->data(),(int)(blob->size() - p));
					if (l < 0)
						return false;
					p += l;
					if (p >= blob->size())
						return false;
					l = asInetAddress(this->routes[this->routeCount].via).unmarshal(blob->data(),(int)(blob->size() - p));
					if (l < 0)
						return false;
					p += l;
					if ((p + 4) > blob->size())
						return false;
					this->routes[this->routeCount].flags = Utils::loadBigEndian<uint16_t>(blob->data() + p); p += 2;
					this->routes[this->routeCount].metric = Utils::loadBigEndian<uint16_t>(blob->data() + p); p += 2;
					++this->routeCount;
				}
			}

			blob = &(d[ZT_NETWORKCONFIG_DICT_KEY_STATIC_IPS]);
			if (!blob->empty()) {
				unsigned int p = 0;
				while ((p < blob->size())&&(staticIpCount < ZT_MAX_ZT_ASSIGNED_ADDRESSES)) {
					int l = this->staticIps[this->staticIpCount].unmarshal(blob->data() + p,(int)(blob->size() - p));
					if (l < 0)
						return false;
					p += l;
					++this->staticIpCount;
				}
			}

			blob = &(d[ZT_NETWORKCONFIG_DICT_KEY_RULES]);
			if (!blob->empty()) {
				this->ruleCount = 0;
				if (CapabilityCredential::unmarshalVirtualNetworkRules(blob->data(), (int)blob->size(), this->rules, this->ruleCount, ZT_MAX_NETWORK_RULES) < 0)
					return false;
			}
		}

		return true;
	} catch ( ... ) {}
	return false;
}

bool NetworkConfig::addSpecialist(const Address &a,const uint64_t f) noexcept
{
	const uint64_t aint = a.toInt();
	for(unsigned int i=0;i<specialistCount;++i) {
		if ((specialists[i] & 0xffffffffffULL) == aint) {
			specialists[i] |= f;
			return true;
		}
	}
	if (specialistCount < ZT_MAX_NETWORK_SPECIALISTS) {
		specialists[specialistCount++] = f | aint;
		return true;
	}
	return false;
}

} // namespace ZeroTier
