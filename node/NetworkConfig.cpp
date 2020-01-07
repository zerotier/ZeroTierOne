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

#include <cstdint>

#include <algorithm>

#include "NetworkConfig.hpp"
#include "ScopedPtr.hpp"

namespace ZeroTier {

bool NetworkConfig::toDictionary(Dictionary<ZT_NETWORKCONFIG_DICT_CAPACITY> &d,bool includeLegacy) const
{
	ScopedPtr< Buffer<ZT_NETWORKCONFIG_DICT_CAPACITY> > tmp(new Buffer<ZT_NETWORKCONFIG_DICT_CAPACITY>());
	char tmp2[128];

	d.clear();

	// Try to put the more human-readable fields first

	if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_NETWORK_ID,this->networkId)) return false;
	if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_TIMESTAMP,this->timestamp)) return false;
	if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_CREDENTIAL_TIME_MAX_DELTA,this->credentialTimeMaxDelta)) return false;
	if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_REVISION,this->revision)) return false;
	if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_ISSUED_TO,this->issuedTo.toString(tmp2))) return false;
	if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_FLAGS,this->flags)) return false;
	if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_TOKEN,this->token)) return false;
	if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_LIMIT,(uint64_t)this->multicastLimit)) return false;
	if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_TYPE,(uint64_t)this->type)) return false;
	if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_NAME,this->name)) return false;
	if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_MTU,(uint64_t)this->mtu)) return false;

	// Then add binary blobs

	if (this->com) {
		tmp->clear();
		this->com.serialize(*tmp);
		if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_COM,*tmp)) return false;
	}

	tmp->clear();
	for(unsigned int i=0;i<this->capabilityCount;++i)
		this->capabilities[i].serialize(*tmp);
	if (tmp->size()) {
		if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_CAPABILITIES,*tmp)) return false;
	}

	tmp->clear();
	for(unsigned int i=0;i<this->tagCount;++i)
		this->tags[i].serialize(*tmp);
	if (tmp->size()) {
		if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_TAGS,*tmp)) return false;
	}

	tmp->clear();
	for(unsigned int i=0;i<this->certificateOfOwnershipCount;++i)
		this->certificatesOfOwnership[i].serialize(*tmp);
	if (tmp->size()) {
		if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_CERTIFICATES_OF_OWNERSHIP,*tmp)) return false;
	}

	tmp->clear();
	for(unsigned int i=0;i<this->specialistCount;++i)
		tmp->append((uint64_t)this->specialists[i]);
	if (tmp->size()) {
		if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_SPECIALISTS,*tmp)) return false;
	}

	tmp->clear();
	for(unsigned int i=0;i<this->routeCount;++i) {
		reinterpret_cast<const InetAddress *>(&(this->routes[i].target))->serialize(*tmp);
		reinterpret_cast<const InetAddress *>(&(this->routes[i].via))->serialize(*tmp);
		tmp->append((uint16_t)this->routes[i].flags);
		tmp->append((uint16_t)this->routes[i].metric);
	}
	if (tmp->size()) {
		if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_ROUTES,*tmp)) return false;
	}

	tmp->clear();
	for(unsigned int i=0;i<this->staticIpCount;++i)
		this->staticIps[i].serialize(*tmp);
	if (tmp->size()) {
		if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_STATIC_IPS,*tmp)) return false;
	}

	if (this->ruleCount) {
		tmp->clear();
		Capability::serializeRules(*tmp,rules,ruleCount);
		if (tmp->size()) {
			if (!d.add(ZT_NETWORKCONFIG_DICT_KEY_RULES,*tmp)) return false;
		}
	}

	return true;
}

bool NetworkConfig::fromDictionary(const Dictionary<ZT_NETWORKCONFIG_DICT_CAPACITY> &d)
{
	static const NetworkConfig NIL_NC;
	ScopedPtr< Buffer<ZT_NETWORKCONFIG_DICT_CAPACITY> > tmp(new Buffer<ZT_NETWORKCONFIG_DICT_CAPACITY>());

	try {
		*this = NIL_NC;

		// Fields that are always present, new or old
		this->networkId = d.getUI(ZT_NETWORKCONFIG_DICT_KEY_NETWORK_ID,0);
		if (!this->networkId)
			return false;
		this->timestamp = d.getUI(ZT_NETWORKCONFIG_DICT_KEY_TIMESTAMP,0);
		this->credentialTimeMaxDelta = d.getUI(ZT_NETWORKCONFIG_DICT_KEY_CREDENTIAL_TIME_MAX_DELTA,0);
		this->revision = d.getUI(ZT_NETWORKCONFIG_DICT_KEY_REVISION,0);
		this->issuedTo = d.getUI(ZT_NETWORKCONFIG_DICT_KEY_ISSUED_TO,0);
		if (!this->issuedTo)
			return false;
		this->multicastLimit = (unsigned int)d.getUI(ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_LIMIT,0);
		d.get(ZT_NETWORKCONFIG_DICT_KEY_NAME,this->name,sizeof(this->name));

		this->mtu = (unsigned int)d.getUI(ZT_NETWORKCONFIG_DICT_KEY_MTU,ZT_DEFAULT_MTU);
		if (this->mtu < 1280)
			this->mtu = 1280; // minimum MTU allowed by IPv6 standard and others
		else if (this->mtu > ZT_MAX_MTU)
			this->mtu = ZT_MAX_MTU;

		if (d.getUI(ZT_NETWORKCONFIG_DICT_KEY_VERSION,0) < 6) {
			return false;
		} else {
			// Otherwise we can use the new fields
			this->flags = d.getUI(ZT_NETWORKCONFIG_DICT_KEY_FLAGS,0);
			this->token = d.getUI(ZT_NETWORKCONFIG_DICT_KEY_TOKEN,0);
			this->type = (ZT_VirtualNetworkType)d.getUI(ZT_NETWORKCONFIG_DICT_KEY_TYPE,(uint64_t)ZT_NETWORK_TYPE_PRIVATE);

			if (d.get(ZT_NETWORKCONFIG_DICT_KEY_COM,*tmp))
				this->com.deserialize(*tmp,0);

			if (d.get(ZT_NETWORKCONFIG_DICT_KEY_CAPABILITIES,*tmp)) {
				try {
					unsigned int p = 0;
					while (p < tmp->size()) {
						Capability cap;
						p += cap.deserialize(*tmp,p);
						this->capabilities[this->capabilityCount++] = cap;
					}
				} catch ( ... ) {}
				std::sort(&(this->capabilities[0]),&(this->capabilities[this->capabilityCount]));
			}

			if (d.get(ZT_NETWORKCONFIG_DICT_KEY_TAGS,*tmp)) {
				try {
					unsigned int p = 0;
					while (p < tmp->size()) {
						Tag tag;
						p += tag.deserialize(*tmp,p);
						this->tags[this->tagCount++] = tag;
					}
				} catch ( ... ) {}
				std::sort(&(this->tags[0]),&(this->tags[this->tagCount]));
			}

			if (d.get(ZT_NETWORKCONFIG_DICT_KEY_CERTIFICATES_OF_OWNERSHIP,*tmp)) {
				unsigned int p = 0;
				while (p < tmp->size()) {
					if (certificateOfOwnershipCount < ZT_MAX_CERTIFICATES_OF_OWNERSHIP)
						p += certificatesOfOwnership[certificateOfOwnershipCount++].deserialize(*tmp,p);
					else {
						CertificateOfOwnership foo;
						p += foo.deserialize(*tmp,p);
					}
				}
			}

			if (d.get(ZT_NETWORKCONFIG_DICT_KEY_SPECIALISTS,*tmp)) {
				unsigned int p = 0;
				while ((p + 8) <= tmp->size()) {
					if (specialistCount < ZT_MAX_NETWORK_SPECIALISTS)
						this->specialists[this->specialistCount++] = tmp->at<uint64_t>(p);
					p += 8;
				}
			}

			if (d.get(ZT_NETWORKCONFIG_DICT_KEY_ROUTES,*tmp)) {
				unsigned int p = 0;
				while ((p < tmp->size())&&(routeCount < ZT_MAX_NETWORK_ROUTES)) {
					p += reinterpret_cast<InetAddress *>(&(this->routes[this->routeCount].target))->deserialize(*tmp,p);
					p += reinterpret_cast<InetAddress *>(&(this->routes[this->routeCount].via))->deserialize(*tmp,p);
					this->routes[this->routeCount].flags = tmp->at<uint16_t>(p); p += 2;
					this->routes[this->routeCount].metric = tmp->at<uint16_t>(p); p += 2;
					++this->routeCount;
				}
			}

			if (d.get(ZT_NETWORKCONFIG_DICT_KEY_STATIC_IPS,*tmp)) {
				unsigned int p = 0;
				while ((p < tmp->size())&&(staticIpCount < ZT_MAX_ZT_ASSIGNED_ADDRESSES)) {
					p += this->staticIps[this->staticIpCount++].deserialize(*tmp,p);
				}
			}

			if (d.get(ZT_NETWORKCONFIG_DICT_KEY_RULES,*tmp)) {
				this->ruleCount = 0;
				unsigned int p = 0;
				Capability::deserializeRules(*tmp,p,this->rules,this->ruleCount,ZT_MAX_NETWORK_RULES);
			}
		}

		return true;
	} catch ( ... ) {
		return false;
	}
}

} // namespace ZeroTier
