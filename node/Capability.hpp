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

#ifndef ZT_CAPABILITY_HPP
#define ZT_CAPABILITY_HPP

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Constants.hpp"
#include "Credential.hpp"
#include "Address.hpp"
#include "C25519.hpp"
#include "Utils.hpp"
#include "Buffer.hpp"
#include "Identity.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * A set of grouped and signed network flow rules
 *
 * On the sending side the sender does the following for each packet:
 *
 * (1) Evaluates its capabilities in ascending order of ID to determine
 *     which capability allows it to transmit this packet.
 * (2) If it has not done so lately, it then sends this capability to the
 *     receiving peer ("presents" it).
 * (3) The sender then sends the packet.
 *
 * On the receiving side the receiver evaluates the capabilities presented
 * by the sender. If any valid un-expired capability allows this packet it
 * is accepted.
 *
 * Note that this is after evaluation of network scope rules and only if
 * network scope rules do not deliver an explicit match.
 *
 * Capabilities support a chain of custody. This is currently unused but
 * in the future would allow the publication of capabilities that can be
 * handed off between nodes. Limited transferability of capabilities is
 * a feature of true capability based security.
 */
class Capability : public Credential
{
	friend class Credential;

public:
	static inline Credential::Type credentialType() { return Credential::CREDENTIAL_TYPE_CAPABILITY; }

	inline Capability() :
		_nwid(0),
		_ts(0),
		_id(0),
		_maxCustodyChainLength(0),
		_ruleCount(0)
	{
		memset(_rules,0,sizeof(_rules));
		memset(_custody,0,sizeof(_custody));
	}

	/**
	 * @param id Capability ID
	 * @param nwid Network ID
	 * @param ts Timestamp (at controller)
	 * @param mccl Maximum custody chain length (1 to create non-transferable capability)
	 * @param rules Network flow rules for this capability
	 * @param ruleCount Number of flow rules
	 */
	inline Capability(uint32_t id,uint64_t nwid,int64_t ts,unsigned int mccl,const ZT_VirtualNetworkRule *rules,unsigned int ruleCount) :
		_nwid(nwid),
		_ts(ts),
		_id(id),
		_maxCustodyChainLength((mccl > 0) ? ((mccl < ZT_MAX_CAPABILITY_CUSTODY_CHAIN_LENGTH) ? mccl : (unsigned int)ZT_MAX_CAPABILITY_CUSTODY_CHAIN_LENGTH) : 1),
		_ruleCount((ruleCount < ZT_MAX_CAPABILITY_RULES) ? ruleCount : ZT_MAX_CAPABILITY_RULES)
	{
		if (_ruleCount > 0)
			memcpy(_rules,rules,sizeof(ZT_VirtualNetworkRule) * _ruleCount);
	}

	/**
	 * @return Rules -- see ruleCount() for size of array
	 */
	inline const ZT_VirtualNetworkRule *rules() const { return _rules; }

	/**
	 * @return Number of rules in rules()
	 */
	inline unsigned int ruleCount() const { return _ruleCount; }

	/**
	 * @return ID and evaluation order of this capability in network
	 */
	inline uint32_t id() const { return _id; }

	/**
	 * @return Network ID for which this capability was issued
	 */
	inline uint64_t networkId() const { return _nwid; }

	/**
	 * @return Timestamp
	 */
	inline int64_t timestamp() const { return _ts; }

	/**
	 * @return Last 'to' address in chain of custody
	 */
	inline Address issuedTo() const
	{
		Address i2;
		for(unsigned int i=0;i<ZT_MAX_CAPABILITY_CUSTODY_CHAIN_LENGTH;++i) {
			if (!_custody[i].to)
				return i2;
			else i2 = _custody[i].to;
		}
		return i2;
	}

	/**
	 * Sign this capability and add signature to its chain of custody
	 *
	 * If this returns false, this object should be considered to be
	 * in an undefined state and should be discarded. False can be returned
	 * if there is no more room for signatures (max chain length reached)
	 * or if the 'from' identity does not include a secret key to allow
	 * it to sign anything.
	 *
	 * @param from Signing identity (must have secret)
	 * @param to Recipient of this signature
	 * @return True if signature successful and chain of custody appended
	 */
	inline bool sign(const Identity &from,const Address &to)
	{
		try {
			for(unsigned int i=0;((i<_maxCustodyChainLength)&&(i<ZT_MAX_CAPABILITY_CUSTODY_CHAIN_LENGTH));++i) {
				if (!(_custody[i].to)) {
					Buffer<(sizeof(Capability) * 2)> tmp;
					this->serialize(tmp,true);
					_custody[i].to = to;
					_custody[i].from = from.address();
					_custody[i].signatureLength = from.sign(tmp.data(),tmp.size(),_custody[i].signature,sizeof(_custody[i].signature));
					return true;
				}
			}
		} catch ( ... ) {}
		return false;
	}

	/**
	 * Verify this capability's chain of custody and signatures
	 *
	 * @param RR Runtime environment to provide for peer lookup, etc.
	 */
	inline Credential::VerifyResult verify(const RuntimeEnvironment *RR,void *tPtr) const { return _verify(RR,tPtr,*this); }

	template<unsigned int C>
	static inline void serializeRules(Buffer<C> &b,const ZT_VirtualNetworkRule *rules,unsigned int ruleCount)
	{
		for(unsigned int i=0;i<ruleCount;++i) {
			// Each rule consists of its 8-bit type followed by the size of that type's
			// field followed by field data. The inclusion of the size will allow non-supported
			// rules to be ignored but still parsed.
			b.append((uint8_t)rules[i].t);
			switch((ZT_VirtualNetworkRuleType)(rules[i].t & 0x3f)) {
				default:
					b.append((uint8_t)0);
					break;
				case ZT_NETWORK_RULE_ACTION_TEE:
				case ZT_NETWORK_RULE_ACTION_WATCH:
				case ZT_NETWORK_RULE_ACTION_REDIRECT:
					b.append((uint8_t)14);
					b.append((uint64_t)rules[i].v.fwd.address);
					b.append((uint32_t)rules[i].v.fwd.flags);
					b.append((uint16_t)rules[i].v.fwd.length); // unused for redirect
					break;
				case ZT_NETWORK_RULE_MATCH_SOURCE_ZEROTIER_ADDRESS:
				case ZT_NETWORK_RULE_MATCH_DEST_ZEROTIER_ADDRESS:
					b.append((uint8_t)5);
					Address(rules[i].v.zt).appendTo(b);
					break;
				case ZT_NETWORK_RULE_MATCH_VLAN_ID:
					b.append((uint8_t)2);
					b.append((uint16_t)rules[i].v.vlanId);
					break;
				case ZT_NETWORK_RULE_MATCH_VLAN_PCP:
					b.append((uint8_t)1);
					b.append((uint8_t)rules[i].v.vlanPcp);
					break;
				case ZT_NETWORK_RULE_MATCH_VLAN_DEI:
					b.append((uint8_t)1);
					b.append((uint8_t)rules[i].v.vlanDei);
					break;
				case ZT_NETWORK_RULE_MATCH_MAC_SOURCE:
				case ZT_NETWORK_RULE_MATCH_MAC_DEST:
					b.append((uint8_t)6);
					b.append(rules[i].v.mac,6);
					break;
				case ZT_NETWORK_RULE_MATCH_IPV4_SOURCE:
				case ZT_NETWORK_RULE_MATCH_IPV4_DEST:
					b.append((uint8_t)5);
					b.append(&(rules[i].v.ipv4.ip),4);
					b.append((uint8_t)rules[i].v.ipv4.mask);
					break;
				case ZT_NETWORK_RULE_MATCH_IPV6_SOURCE:
				case ZT_NETWORK_RULE_MATCH_IPV6_DEST:
					b.append((uint8_t)17);
					b.append(rules[i].v.ipv6.ip,16);
					b.append((uint8_t)rules[i].v.ipv6.mask);
					break;
				case ZT_NETWORK_RULE_MATCH_IP_TOS:
					b.append((uint8_t)3);
					b.append((uint8_t)rules[i].v.ipTos.mask);
					b.append((uint8_t)rules[i].v.ipTos.value[0]);
					b.append((uint8_t)rules[i].v.ipTos.value[1]);
					break;
				case ZT_NETWORK_RULE_MATCH_IP_PROTOCOL:
					b.append((uint8_t)1);
					b.append((uint8_t)rules[i].v.ipProtocol);
					break;
				case ZT_NETWORK_RULE_MATCH_ETHERTYPE:
					b.append((uint8_t)2);
					b.append((uint16_t)rules[i].v.etherType);
					break;
				case ZT_NETWORK_RULE_MATCH_ICMP:
					b.append((uint8_t)3);
					b.append((uint8_t)rules[i].v.icmp.type);
					b.append((uint8_t)rules[i].v.icmp.code);
					b.append((uint8_t)rules[i].v.icmp.flags);
					break;
				case ZT_NETWORK_RULE_MATCH_IP_SOURCE_PORT_RANGE:
				case ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE:
					b.append((uint8_t)4);
					b.append((uint16_t)rules[i].v.port[0]);
					b.append((uint16_t)rules[i].v.port[1]);
					break;
				case ZT_NETWORK_RULE_MATCH_CHARACTERISTICS:
					b.append((uint8_t)8);
					b.append((uint64_t)rules[i].v.characteristics);
					break;
				case ZT_NETWORK_RULE_MATCH_FRAME_SIZE_RANGE:
					b.append((uint8_t)4);
					b.append((uint16_t)rules[i].v.frameSize[0]);
					b.append((uint16_t)rules[i].v.frameSize[1]);
					break;
				case ZT_NETWORK_RULE_MATCH_RANDOM:
					b.append((uint8_t)4);
					b.append((uint32_t)rules[i].v.randomProbability);
					break;
				case ZT_NETWORK_RULE_MATCH_TAGS_DIFFERENCE:
				case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_AND:
				case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_OR:
				case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_XOR:
				case ZT_NETWORK_RULE_MATCH_TAGS_EQUAL:
				case ZT_NETWORK_RULE_MATCH_TAG_SENDER:
				case ZT_NETWORK_RULE_MATCH_TAG_RECEIVER:
					b.append((uint8_t)8);
					b.append((uint32_t)rules[i].v.tag.id);
					b.append((uint32_t)rules[i].v.tag.value);
					break;
				case ZT_NETWORK_RULE_MATCH_INTEGER_RANGE:
					b.append((uint8_t)19);
					b.append((uint64_t)rules[i].v.intRange.start);
					b.append((uint64_t)(rules[i].v.intRange.start + (uint64_t)rules[i].v.intRange.end)); // more future-proof
					b.append((uint16_t)rules[i].v.intRange.idx);
					b.append((uint8_t)rules[i].v.intRange.format);
					break;
			}
		}
	}

	template<unsigned int C>
	static inline void deserializeRules(const Buffer<C> &b,unsigned int &p,ZT_VirtualNetworkRule *rules,unsigned int &ruleCount,const unsigned int maxRuleCount)
	{
		while ((ruleCount < maxRuleCount)&&(p < b.size())) {
			rules[ruleCount].t = (uint8_t)b[p++];
			const unsigned int fieldLen = (unsigned int)b[p++];
			switch((ZT_VirtualNetworkRuleType)(rules[ruleCount].t & 0x3f)) {
				default:
					break;
				case ZT_NETWORK_RULE_ACTION_TEE:
				case ZT_NETWORK_RULE_ACTION_WATCH:
				case ZT_NETWORK_RULE_ACTION_REDIRECT:
					rules[ruleCount].v.fwd.address = b.template at<uint64_t>(p);
					rules[ruleCount].v.fwd.flags = b.template at<uint32_t>(p + 8);
					rules[ruleCount].v.fwd.length = b.template at<uint16_t>(p + 12);
					break;
				case ZT_NETWORK_RULE_MATCH_SOURCE_ZEROTIER_ADDRESS:
				case ZT_NETWORK_RULE_MATCH_DEST_ZEROTIER_ADDRESS:
					rules[ruleCount].v.zt = Address(b.field(p,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH).toInt();
					break;
				case ZT_NETWORK_RULE_MATCH_VLAN_ID:
					rules[ruleCount].v.vlanId = b.template at<uint16_t>(p);
					break;
				case ZT_NETWORK_RULE_MATCH_VLAN_PCP:
					rules[ruleCount].v.vlanPcp = (uint8_t)b[p];
					break;
				case ZT_NETWORK_RULE_MATCH_VLAN_DEI:
					rules[ruleCount].v.vlanDei = (uint8_t)b[p];
					break;
				case ZT_NETWORK_RULE_MATCH_MAC_SOURCE:
				case ZT_NETWORK_RULE_MATCH_MAC_DEST:
					memcpy(rules[ruleCount].v.mac,b.field(p,6),6);
					break;
				case ZT_NETWORK_RULE_MATCH_IPV4_SOURCE:
				case ZT_NETWORK_RULE_MATCH_IPV4_DEST:
					memcpy(&(rules[ruleCount].v.ipv4.ip),b.field(p,4),4);
					rules[ruleCount].v.ipv4.mask = (uint8_t)b[p + 4];
					break;
				case ZT_NETWORK_RULE_MATCH_IPV6_SOURCE:
				case ZT_NETWORK_RULE_MATCH_IPV6_DEST:
					memcpy(rules[ruleCount].v.ipv6.ip,b.field(p,16),16);
					rules[ruleCount].v.ipv6.mask = (uint8_t)b[p + 16];
					break;
				case ZT_NETWORK_RULE_MATCH_IP_TOS:
					rules[ruleCount].v.ipTos.mask = (uint8_t)b[p];
					rules[ruleCount].v.ipTos.value[0] = (uint8_t)b[p+1];
					rules[ruleCount].v.ipTos.value[1] = (uint8_t)b[p+2];
					break;
				case ZT_NETWORK_RULE_MATCH_IP_PROTOCOL:
					rules[ruleCount].v.ipProtocol = (uint8_t)b[p];
					break;
				case ZT_NETWORK_RULE_MATCH_ETHERTYPE:
					rules[ruleCount].v.etherType = b.template at<uint16_t>(p);
					break;
				case ZT_NETWORK_RULE_MATCH_ICMP:
					rules[ruleCount].v.icmp.type = (uint8_t)b[p];
					rules[ruleCount].v.icmp.code = (uint8_t)b[p+1];
					rules[ruleCount].v.icmp.flags = (uint8_t)b[p+2];
					break;
				case ZT_NETWORK_RULE_MATCH_IP_SOURCE_PORT_RANGE:
				case ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE:
					rules[ruleCount].v.port[0] = b.template at<uint16_t>(p);
					rules[ruleCount].v.port[1] = b.template at<uint16_t>(p + 2);
					break;
				case ZT_NETWORK_RULE_MATCH_CHARACTERISTICS:
					rules[ruleCount].v.characteristics = b.template at<uint64_t>(p);
					break;
				case ZT_NETWORK_RULE_MATCH_FRAME_SIZE_RANGE:
					rules[ruleCount].v.frameSize[0] = b.template at<uint16_t>(p);
					rules[ruleCount].v.frameSize[1] = b.template at<uint16_t>(p + 2);
					break;
				case ZT_NETWORK_RULE_MATCH_RANDOM:
					rules[ruleCount].v.randomProbability = b.template at<uint32_t>(p);
					break;
				case ZT_NETWORK_RULE_MATCH_TAGS_DIFFERENCE:
				case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_AND:
				case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_OR:
				case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_XOR:
				case ZT_NETWORK_RULE_MATCH_TAGS_EQUAL:
				case ZT_NETWORK_RULE_MATCH_TAG_SENDER:
				case ZT_NETWORK_RULE_MATCH_TAG_RECEIVER:
					rules[ruleCount].v.tag.id = b.template at<uint32_t>(p);
					rules[ruleCount].v.tag.value = b.template at<uint32_t>(p + 4);
					break;
				case ZT_NETWORK_RULE_MATCH_INTEGER_RANGE:
					rules[ruleCount].v.intRange.start = b.template at<uint64_t>(p);
					rules[ruleCount].v.intRange.end = (uint32_t)(b.template at<uint64_t>(p + 8) - rules[ruleCount].v.intRange.start);
					rules[ruleCount].v.intRange.idx = b.template at<uint16_t>(p + 16);
					rules[ruleCount].v.intRange.format = (uint8_t)b[p + 18];
					break;
			}
			p += fieldLen;
			++ruleCount;
		}
	}

	template<unsigned int C>
	inline void serialize(Buffer<C> &b,const bool forSign = false) const
	{
		if (forSign) b.append((uint64_t)0x7f7f7f7f7f7f7f7fULL);

		// These are the same between Tag and Capability
		b.append(_nwid);
		b.append(_ts);
		b.append(_id);

		b.append((uint16_t)_ruleCount);
		serializeRules(b,_rules,_ruleCount);
		b.append((uint8_t)_maxCustodyChainLength);

		if (!forSign) {
			for(unsigned int i=0;;++i) {
				if ((i < _maxCustodyChainLength)&&(i < ZT_MAX_CAPABILITY_CUSTODY_CHAIN_LENGTH)&&(_custody[i].to)) {
					_custody[i].to.appendTo(b);
					_custody[i].from.appendTo(b);
					b.append((uint8_t)1);
					b.append((uint16_t)_custody[i].signatureLength);
					b.append(_custody[i].signature,_custody[i].signatureLength);
				} else {
					b.append((unsigned char)0,ZT_ADDRESS_LENGTH); // zero 'to' terminates chain
					break;
				}
			}
		}

		// This is the size of any additional fields, currently 0.
		b.append((uint16_t)0);

		if (forSign) b.append((uint64_t)0x7f7f7f7f7f7f7f7fULL);
	}

	template<unsigned int C>
	inline unsigned int deserialize(const Buffer<C> &b,unsigned int startAt = 0)
	{
		*this = Capability();

		unsigned int p = startAt;

		_nwid = b.template at<uint64_t>(p); p += 8;
		_ts = b.template at<uint64_t>(p); p += 8;
		_id = b.template at<uint32_t>(p); p += 4;

		const unsigned int rc = b.template at<uint16_t>(p); p += 2;
		if (rc > ZT_MAX_CAPABILITY_RULES)
			throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_OVERFLOW;
		deserializeRules(b,p,_rules,_ruleCount,rc);

		_maxCustodyChainLength = (unsigned int)b[p++];
		if ((_maxCustodyChainLength < 1)||(_maxCustodyChainLength > ZT_MAX_CAPABILITY_CUSTODY_CHAIN_LENGTH))
			throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_OVERFLOW;

		for(unsigned int i=0;;++i) {
			const Address to(b.field(p,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH); p += ZT_ADDRESS_LENGTH;
			if (!to)
				break;
			if ((i >= _maxCustodyChainLength)||(i >= ZT_MAX_CAPABILITY_CUSTODY_CHAIN_LENGTH))
				throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_OVERFLOW;
			_custody[i].to = to;
			_custody[i].from.setTo(b.field(p,ZT_ADDRESS_LENGTH),ZT_ADDRESS_LENGTH); p += ZT_ADDRESS_LENGTH;
			if (b[p++] == 1) {
				_custody[i].signatureLength = b.template at<uint16_t>(p);
				if (_custody[i].signatureLength > sizeof(_custody[i].signature))
					throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_INVALID_CRYPTOGRAPHIC_TOKEN;
				p += 2;
				memcpy(_custody[i].signature,b.field(p,_custody[i].signatureLength),_custody[i].signatureLength); p += _custody[i].signatureLength;
			} else {
				p += 2 + b.template at<uint16_t>(p);
			}
		}

		p += 2 + b.template at<uint16_t>(p);
		if (p > b.size())
			throw ZT_EXCEPTION_INVALID_SERIALIZED_DATA_OVERFLOW;

		return (p - startAt);
	}

	// Provides natural sort order by ID
	inline bool operator<(const Capability &c) const { return (_id < c._id); }

	inline bool operator==(const Capability &c) const { return (memcmp(this,&c,sizeof(Capability)) == 0); }
	inline bool operator!=(const Capability &c) const { return (memcmp(this,&c,sizeof(Capability)) != 0); }

private:
	uint64_t _nwid;
	int64_t _ts;
	uint32_t _id;

	unsigned int _maxCustodyChainLength;

	unsigned int _ruleCount;
	ZT_VirtualNetworkRule _rules[ZT_MAX_CAPABILITY_RULES];

	struct {
		Address to;
		Address from;
		unsigned int signatureLength;
		uint8_t signature[ZT_SIGNATURE_BUFFER_SIZE];
	} _custody[ZT_MAX_CAPABILITY_CUSTODY_CHAIN_LENGTH];
};

} // namespace ZeroTier

#endif
