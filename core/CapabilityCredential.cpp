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

#include "CapabilityCredential.hpp"
#include "Utils.hpp"
#include "Constants.hpp"
#include "MAC.hpp"

namespace ZeroTier {

bool CapabilityCredential::sign(const Identity &from, const Address &to) noexcept
{
	uint8_t buf[ZT_CAPABILITY_MARSHAL_SIZE_MAX + 16];
	m_issuedTo = to;
	m_signedBy = from.address();
	m_signatureLength = from.sign(buf, (unsigned int) marshal(buf, true), m_signature, sizeof(m_signature));
	return m_signatureLength > 0;
}

int CapabilityCredential::marshal(uint8_t data[ZT_CAPABILITY_MARSHAL_SIZE_MAX], const bool forSign) const noexcept
{
	int p = 0;

	if (forSign) {
		for (int k = 0;k < 8;++k)
			data[p++] = 0x7f;
	}

	Utils::storeBigEndian<uint64_t>(data + p, m_nwid);
	p += 8;
	Utils::storeBigEndian<uint64_t>(data + p, (uint64_t) m_ts);
	p += 8;
	Utils::storeBigEndian<uint32_t>(data + p, m_id);
	p += 4;

	Utils::storeBigEndian<uint16_t>(data + p, (uint16_t) m_ruleCount);
	p += 2;
	p += CapabilityCredential::marshalVirtualNetworkRules(data + p, m_rules, m_ruleCount);

	// LEGACY: older versions supported multiple records with this being a maximum custody
	// chain length. This is deprecated so set the max chain length to one.
	data[p++] = (uint8_t) 1;

	if (!forSign) {
		m_issuedTo.copyTo(data + p);
		p += ZT_ADDRESS_LENGTH;
		m_signedBy.copyTo(data + 0);
		p += ZT_ADDRESS_LENGTH;
		data[p++] = 1; // LEGACY: old versions require a reserved byte here
		Utils::storeBigEndian<uint16_t>(data + p, (uint16_t) m_signatureLength);
		p += 2;
		Utils::copy(data + p, m_signature, m_signatureLength);
		p += (int) m_signatureLength;

		// LEGACY: older versions supported more than one record terminated by a zero address.
		for (int k = 0;k < ZT_ADDRESS_LENGTH;++k)
			data[p++] = 0;
	}

	data[p++] = 0;
	data[p++] = 0; // uint16_t size of additional fields, currently 0

	if (forSign) {
		for (int k = 0;k < 8;++k)
			data[p++] = 0x7f;
	}

	return p;
}

int CapabilityCredential::unmarshal(const uint8_t *data, int len) noexcept
{
	if (len < 22)
		return -1;

	m_nwid = Utils::loadBigEndian<uint64_t>(data);
	m_ts = (int64_t) Utils::loadBigEndian<uint64_t>(data + 8);
	m_id = Utils::loadBigEndian<uint32_t>(data + 16);

	const unsigned int rc = Utils::loadBigEndian<uint16_t>(data + 20);
	if (rc > ZT_MAX_CAPABILITY_RULES)
		return -1;
	const int rulesLen = unmarshalVirtualNetworkRules(data + 22, len - 22, m_rules, m_ruleCount, rc);
	if (rulesLen < 0)
		return rulesLen;
	int p = 22 + rulesLen;

	if (p >= len)
		return -1;
	++p; // LEGACY: skip old max record count

	// LEGACY: since it was once supported to have multiple records, scan them all. Since
	// this feature was never used, just set the signature and issued to and other related
	// fields each time and we should only ever see one. If there's more than one and the
	// last is not the controller, this credential will just fail validity check.
	for (unsigned int i = 0;;++i) {
		if ((p + ZT_ADDRESS_LENGTH) > len)
			return -1;
		const Address to(data + p);
		p += ZT_ADDRESS_LENGTH;

		if (!to)
			break;

		m_issuedTo = to;
		if ((p + ZT_ADDRESS_LENGTH) > len)
			return -1;
		m_signedBy.setTo(data + p);
		p += ZT_ADDRESS_LENGTH + 1; // LEGACY: +1 to skip reserved field

		if ((p + 2) > len)
			return -1;
		m_signatureLength = Utils::loadBigEndian<uint16_t>(data + p);
		p += 2;
		if ((m_signatureLength > sizeof(m_signature)) || ((p + (int) m_signatureLength) > len))
			return -1;
		Utils::copy(m_signature, data + p, m_signatureLength);
		p += (int) m_signatureLength;
	}

	if ((p + 2) > len)
		return -1;
	p += 2 + Utils::loadBigEndian<uint16_t>(data + p);

	if (p > len)
		return -1;

	return p;
}

int CapabilityCredential::marshalVirtualNetworkRules(uint8_t *data, const ZT_VirtualNetworkRule *const rules, const unsigned int ruleCount) noexcept
{
	int p = 0;
	for (unsigned int i = 0;i < ruleCount;++i) {
		data[p++] = rules[i].t;
		switch ((ZT_VirtualNetworkRuleType) (rules[i].t & 0x3fU)) {
			default:
				data[p++] = 0;
				break;
			case ZT_NETWORK_RULE_ACTION_TEE:
			case ZT_NETWORK_RULE_ACTION_WATCH:
			case ZT_NETWORK_RULE_ACTION_REDIRECT:
				data[p++] = 14;
				Utils::storeBigEndian<uint64_t>(data + p, rules[i].v.fwd.address);
				p += 8;
				Utils::storeBigEndian<uint32_t>(data + p, rules[i].v.fwd.flags);
				p += 4;
				Utils::storeBigEndian<uint16_t>(data + p, rules[i].v.fwd.length);
				p += 2;
				break;
			case ZT_NETWORK_RULE_MATCH_SOURCE_ZEROTIER_ADDRESS:
			case ZT_NETWORK_RULE_MATCH_DEST_ZEROTIER_ADDRESS:
				data[p++] = 5;
				Address(rules[i].v.zt).copyTo(data + p);
				p += ZT_ADDRESS_LENGTH;
				break;
			case ZT_NETWORK_RULE_MATCH_VLAN_ID:
				data[p++] = 2;
				Utils::storeBigEndian<uint16_t>(data + p, rules[i].v.vlanId);
				p += 2;
				break;
			case ZT_NETWORK_RULE_MATCH_VLAN_PCP:
				data[p++] = 1;
				data[p++] = rules[i].v.vlanPcp;
				break;
			case ZT_NETWORK_RULE_MATCH_VLAN_DEI:
				data[p++] = 1;
				data[p++] = rules[i].v.vlanDei;
				break;
			case ZT_NETWORK_RULE_MATCH_MAC_SOURCE:
			case ZT_NETWORK_RULE_MATCH_MAC_DEST:
				data[p++] = 6;
				MAC(rules[i].v.mac).copyTo(data + p);
				p += 6;
				break;
			case ZT_NETWORK_RULE_MATCH_IPV4_SOURCE:
			case ZT_NETWORK_RULE_MATCH_IPV4_DEST:
				data[p++] = 5;
				data[p++] = reinterpret_cast<const uint8_t *>(&(rules[i].v.ipv4.ip))[0];
				data[p++] = reinterpret_cast<const uint8_t *>(&(rules[i].v.ipv4.ip))[1];
				data[p++] = reinterpret_cast<const uint8_t *>(&(rules[i].v.ipv4.ip))[2];
				data[p++] = reinterpret_cast<const uint8_t *>(&(rules[i].v.ipv4.ip))[3];
				data[p++] = rules[i].v.ipv4.mask;
				break;
			case ZT_NETWORK_RULE_MATCH_IPV6_SOURCE:
			case ZT_NETWORK_RULE_MATCH_IPV6_DEST:
				data[p++] = 17;
				Utils::copy<16>(data + p, rules[i].v.ipv6.ip);
				p += 16;
				data[p++] = rules[i].v.ipv6.mask;
				break;
			case ZT_NETWORK_RULE_MATCH_IP_TOS:
				data[p++] = 3;
				data[p++] = rules[i].v.ipTos.mask;
				data[p++] = rules[i].v.ipTos.value[0];
				data[p++] = rules[i].v.ipTos.value[1];
				break;
			case ZT_NETWORK_RULE_MATCH_IP_PROTOCOL:
				data[p++] = 1;
				data[p++] = rules[i].v.ipProtocol;
				break;
			case ZT_NETWORK_RULE_MATCH_ETHERTYPE:
				data[p++] = 2;
				Utils::storeBigEndian<uint16_t>(data + p, rules[i].v.etherType);
				p += 2;
				break;
			case ZT_NETWORK_RULE_MATCH_ICMP:
				data[p++] = 3;
				data[p++] = rules[i].v.icmp.type;
				data[p++] = rules[i].v.icmp.code;
				data[p++] = rules[i].v.icmp.flags;
				break;
			case ZT_NETWORK_RULE_MATCH_IP_SOURCE_PORT_RANGE:
			case ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE:
				data[p++] = 4;
				Utils::storeBigEndian<uint16_t>(data + p, rules[i].v.port[0]);
				p += 2;
				Utils::storeBigEndian<uint16_t>(data + p, rules[i].v.port[1]);
				p += 2;
				break;
			case ZT_NETWORK_RULE_MATCH_CHARACTERISTICS:
				data[p++] = 8;
				Utils::storeBigEndian<uint64_t>(data + p, rules[i].v.characteristics);
				p += 8;
				break;
			case ZT_NETWORK_RULE_MATCH_FRAME_SIZE_RANGE:
				data[p++] = 4;
				Utils::storeBigEndian<uint16_t>(data + p, rules[i].v.frameSize[0]);
				p += 2;
				Utils::storeBigEndian<uint16_t>(data + p, rules[i].v.frameSize[1]);
				p += 2;
				break;
			case ZT_NETWORK_RULE_MATCH_RANDOM:
				data[p++] = 4;
				Utils::storeBigEndian<uint32_t>(data + p, rules[i].v.randomProbability);
				p += 4;
				break;
			case ZT_NETWORK_RULE_MATCH_TAGS_DIFFERENCE:
			case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_AND:
			case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_OR:
			case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_XOR:
			case ZT_NETWORK_RULE_MATCH_TAGS_EQUAL:
			case ZT_NETWORK_RULE_MATCH_TAG_SENDER:
			case ZT_NETWORK_RULE_MATCH_TAG_RECEIVER:
				data[p++] = 8;
				Utils::storeBigEndian<uint32_t>(data + p, rules[i].v.tag.id);
				p += 4;
				Utils::storeBigEndian<uint32_t>(data + p, rules[i].v.tag.value);
				p += 4;
				break;
			case ZT_NETWORK_RULE_MATCH_INTEGER_RANGE:
				data[p++] = 19;
				Utils::storeBigEndian<uint64_t>(data + p, rules[i].v.intRange.start);
				p += 8;
				Utils::storeBigEndian<uint64_t>(data + p, rules[i].v.intRange.start + (uint64_t) rules[i].v.intRange.end);
				p += 8;
				Utils::storeBigEndian<uint16_t>(data + p, rules[i].v.intRange.idx);
				p += 2;
				data[p++] = rules[i].v.intRange.format;
				break;
		}
	}
	return p;
}

int CapabilityCredential::unmarshalVirtualNetworkRules(const uint8_t *const data, const int len, ZT_VirtualNetworkRule *const rules, unsigned int &ruleCount, const unsigned int maxRuleCount) noexcept
{
	int p = 0;
	unsigned int rc = 0;
	while (rc < maxRuleCount) {
		if (p >= len)
			return -1;
		rules[ruleCount].t = data[p++];
		const int fieldLen = (int) data[p++];
		if ((p + fieldLen) > len)
			return -1;
		switch ((ZT_VirtualNetworkRuleType) (rules[ruleCount].t & 0x3fU)) {
			default:
				break;
			case ZT_NETWORK_RULE_ACTION_TEE:
			case ZT_NETWORK_RULE_ACTION_WATCH:
			case ZT_NETWORK_RULE_ACTION_REDIRECT:
				if ((p + 14) > len) return -1;
				rules[ruleCount].v.fwd.address = Utils::loadBigEndian<uint64_t>(data + p);
				p += 8;
				rules[ruleCount].v.fwd.flags = Utils::loadBigEndian<uint32_t>(data + p);
				p += 4;
				rules[ruleCount].v.fwd.length = Utils::loadBigEndian<uint16_t>(data + p);
				p += 2;
				break;
			case ZT_NETWORK_RULE_MATCH_SOURCE_ZEROTIER_ADDRESS:
			case ZT_NETWORK_RULE_MATCH_DEST_ZEROTIER_ADDRESS:
				if ((p + ZT_ADDRESS_LENGTH) > len) return -1;
				rules[ruleCount].v.zt = Address(data + p).toInt();
				p += ZT_ADDRESS_LENGTH;
				break;
			case ZT_NETWORK_RULE_MATCH_VLAN_ID:
				if ((p + 2) > len) return -1;
				rules[ruleCount].v.vlanId = Utils::loadBigEndian<uint16_t>(data + p);
				p += 2;
				break;
			case ZT_NETWORK_RULE_MATCH_VLAN_PCP:
				if ((p + 1) > len) return -1;
				rules[ruleCount].v.vlanPcp = data[p++];
				break;
			case ZT_NETWORK_RULE_MATCH_VLAN_DEI:
				if ((p + 1) > len) return -1;
				rules[ruleCount].v.vlanDei = data[p++];
				break;
			case ZT_NETWORK_RULE_MATCH_MAC_SOURCE:
			case ZT_NETWORK_RULE_MATCH_MAC_DEST:
				if ((p + 6) > len) return -1;
				Utils::copy<6>(rules[ruleCount].v.mac, data + p);
				p += 6;
				break;
			case ZT_NETWORK_RULE_MATCH_IPV4_SOURCE:
			case ZT_NETWORK_RULE_MATCH_IPV4_DEST:
				if ((p + 5) > len) return -1;
				Utils::copy<4>(&(rules[ruleCount].v.ipv4.ip), data + p);
				p += 4;
				rules[ruleCount].v.ipv4.mask = data[p++];
				break;
			case ZT_NETWORK_RULE_MATCH_IPV6_SOURCE:
			case ZT_NETWORK_RULE_MATCH_IPV6_DEST:
				if ((p + 17) > len) return -1;
				Utils::copy<16>(rules[ruleCount].v.ipv6.ip, data + p);
				p += 16;
				rules[ruleCount].v.ipv6.mask = data[p++];
				break;
			case ZT_NETWORK_RULE_MATCH_IP_TOS:
				if ((p + 3) > len) return -1;
				rules[ruleCount].v.ipTos.mask = data[p++];
				rules[ruleCount].v.ipTos.value[0] = data[p++];
				rules[ruleCount].v.ipTos.value[1] = data[p++];
				break;
			case ZT_NETWORK_RULE_MATCH_IP_PROTOCOL:
				if ((p + 1) > len) return -1;
				rules[ruleCount].v.ipProtocol = data[p++];
				break;
			case ZT_NETWORK_RULE_MATCH_ETHERTYPE:
				if ((p + 2) > len) return -1;
				rules[ruleCount].v.etherType = Utils::loadBigEndian<uint16_t>(data + p);
				p += 2;
				break;
			case ZT_NETWORK_RULE_MATCH_ICMP:
				if ((p + 3) > len) return -1;
				rules[ruleCount].v.icmp.type = data[p++];
				rules[ruleCount].v.icmp.code = data[p++];
				rules[ruleCount].v.icmp.flags = data[p++];
				break;
			case ZT_NETWORK_RULE_MATCH_IP_SOURCE_PORT_RANGE:
			case ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE:
				if ((p + 4) > len) return -1;
				rules[ruleCount].v.port[0] = Utils::loadBigEndian<uint16_t>(data + p);
				p += 2;
				rules[ruleCount].v.port[1] = Utils::loadBigEndian<uint16_t>(data + p);
				p += 2;
				break;
			case ZT_NETWORK_RULE_MATCH_CHARACTERISTICS:
				if ((p + 8) > len) return -1;
				rules[ruleCount].v.characteristics = Utils::loadBigEndian<uint64_t>(data + p);
				p += 8;
				break;
			case ZT_NETWORK_RULE_MATCH_FRAME_SIZE_RANGE:
				if ((p + 4) > len) return -1;
				rules[ruleCount].v.frameSize[0] = Utils::loadBigEndian<uint16_t>(data + p);
				p += 2;
				rules[ruleCount].v.frameSize[1] = Utils::loadBigEndian<uint16_t>(data + p);
				p += 2;
				break;
			case ZT_NETWORK_RULE_MATCH_RANDOM:
				if ((p + 4) > len) return -1;
				rules[ruleCount].v.randomProbability = Utils::loadBigEndian<uint32_t>(data + p);
				p += 4;
				break;
			case ZT_NETWORK_RULE_MATCH_TAGS_DIFFERENCE:
			case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_AND:
			case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_OR:
			case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_XOR:
			case ZT_NETWORK_RULE_MATCH_TAGS_EQUAL:
			case ZT_NETWORK_RULE_MATCH_TAG_SENDER:
			case ZT_NETWORK_RULE_MATCH_TAG_RECEIVER:
				if ((p + 4) > len) return -1;
				rules[ruleCount].v.tag.id = Utils::loadBigEndian<uint32_t>(data + p);
				p += 4;
				rules[ruleCount].v.tag.value = Utils::loadBigEndian<uint32_t>(data + p);
				p += 4;
				break;
			case ZT_NETWORK_RULE_MATCH_INTEGER_RANGE:
				if ((p + 19) > len) return -1;
				rules[ruleCount].v.intRange.start = Utils::loadBigEndian<uint64_t>(data + p);
				p += 8;
				rules[ruleCount].v.intRange.end = (uint32_t) (Utils::loadBigEndian<uint64_t>(data + p) - rules[ruleCount].v.intRange.start);
				p += 8;
				rules[ruleCount].v.intRange.idx = Utils::loadBigEndian<uint16_t>(data + p);
				p += 2;
				rules[ruleCount].v.intRange.format = data[p++];
				break;
		}
		p += fieldLen;
		++rc;
	}
	ruleCount = rc;
	return p;
}

} // namespace ZeroTier
