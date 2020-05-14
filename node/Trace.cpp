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

#include "Trace.hpp"
#include "RuntimeEnvironment.hpp"
#include "Node.hpp"
#include "Peer.hpp"
#include "InetAddress.hpp"
#include "FCV.hpp"

// NOTE: packet IDs are always handled in network byte order, so no need to convert them.

namespace ZeroTier {

Trace::Trace(const RuntimeEnvironment *renv) :
	RR(renv),
	_f(0)
{
}

void Trace::unexpectedError(
	void *tPtr,
	uint32_t codeLocation,
	const char *message,
	...)
{
	FCV<uint8_t,4096> buf;
	Dictionary::append(buf,ZT_TRACE_FIELD_TYPE,ZT_TRACE_UNEXPECTED_ERROR);
	Dictionary::append(buf,ZT_TRACE_FIELD_CODE_LOCATION,codeLocation);
	Dictionary::append(buf,ZT_TRACE_FIELD_MESSAGE,message);
	buf.push_back(0);
	RR->node->postEvent(tPtr,ZT_EVENT_TRACE,buf.data());
}

void Trace::_resettingPathsInScope(
	void *const tPtr,
	const uint32_t codeLocation,
	const Identity &reporter,
	const InetAddress &from,
	const InetAddress &oldExternal,
	const InetAddress &newExternal,
	const InetAddress::IpScope scope)
{
	FCV<uint8_t,4096> buf;
	Dictionary::append(buf,ZT_TRACE_FIELD_TYPE,ZT_TRACE_VL1_RESETTING_PATHS_IN_SCOPE);
	Dictionary::append(buf,ZT_TRACE_FIELD_CODE_LOCATION,codeLocation);
	if (from)
		Dictionary::appendObject(buf,ZT_TRACE_FIELD_ENDPOINT,Endpoint(from));
	if (oldExternal)
		Dictionary::appendObject(buf,ZT_TRACE_FIELD_OLD_ENDPOINT,Endpoint(oldExternal));
	if (newExternal)
		Dictionary::appendObject(buf,ZT_TRACE_FIELD_NEW_ENDPOINT,Endpoint(newExternal));
	Dictionary::append(buf,ZT_TRACE_FIELD_RESET_ADDRESS_SCOPE,scope);
	buf.push_back(0);
	RR->node->postEvent(tPtr,ZT_EVENT_TRACE,buf.data());
}

void Trace::_tryingNewPath(
	void *const tPtr,
	const uint32_t codeLocation,
	const Identity &trying,
	const InetAddress &physicalAddress,
	const InetAddress &triggerAddress,
	const uint64_t triggeringPacketId,
	const uint8_t triggeringPacketVerb,
	const Identity &triggeringPeer)
{
	FCV<uint8_t,4096> buf;
	Dictionary::append(buf,ZT_TRACE_FIELD_TYPE,ZT_TRACE_VL1_TRYING_NEW_PATH);
	Dictionary::append(buf,ZT_TRACE_FIELD_CODE_LOCATION,codeLocation);
	Dictionary::append(buf,ZT_TRACE_FIELD_IDENTITY_FINGERPRINT_HASH,trying.fingerprint().hash(),ZT_FINGERPRINT_HASH_SIZE);
	if (triggerAddress)
		Dictionary::appendObject(buf,ZT_TRACE_FIELD_TRIGGER_FROM_ENDPOINT,Endpoint(triggerAddress));
	Dictionary::appendPacketId(buf,ZT_TRACE_FIELD_TRIGGER_FROM_PACKET_ID,triggeringPacketId);
	Dictionary::append(buf,ZT_TRACE_FIELD_TRIGGER_FROM_PACKET_VERB,triggeringPacketVerb);
	if (triggeringPeer)
		Dictionary::append(buf,ZT_TRACE_FIELD_TRIGGER_FROM_PEER_FINGERPRINT_HASH,triggeringPeer.fingerprint().hash(),ZT_FINGERPRINT_HASH_SIZE);
	buf.push_back(0);
	RR->node->postEvent(tPtr,ZT_EVENT_TRACE,buf.data());
}

void Trace::_learnedNewPath(
	void *const tPtr,
	const uint32_t codeLocation,
	const uint64_t packetId,
	const Identity &peerIdentity,
	const InetAddress &physicalAddress,
	const InetAddress &replaced)
{
	FCV<uint8_t,4096> buf;
	Dictionary::append(buf,ZT_TRACE_FIELD_TYPE,ZT_TRACE_VL1_LEARNED_NEW_PATH);
	Dictionary::append(buf,ZT_TRACE_FIELD_CODE_LOCATION,codeLocation);
	Dictionary::appendPacketId(buf,ZT_TRACE_FIELD_PACKET_ID,packetId);
	Dictionary::append(buf,ZT_TRACE_FIELD_IDENTITY_FINGERPRINT_HASH,peerIdentity.fingerprint().hash(),ZT_FINGERPRINT_HASH_SIZE);
	if (physicalAddress)
		Dictionary::appendObject(buf,ZT_TRACE_FIELD_ENDPOINT,Endpoint(physicalAddress));
	if (replaced)
		Dictionary::appendObject(buf,ZT_TRACE_FIELD_OLD_ENDPOINT,Endpoint(replaced));
	buf.push_back(0);
	RR->node->postEvent(tPtr,ZT_EVENT_TRACE,buf.data());
}

void Trace::_incomingPacketDropped(
	void *const tPtr,
	const uint32_t codeLocation,
	const uint64_t packetId,
	const uint64_t networkId,
	const Identity &peerIdentity,
	const InetAddress &physicalAddress,
	const uint8_t hops,
	const uint8_t verb,
	const ZT_TracePacketDropReason reason)
{
	FCV<uint8_t,4096> buf;
	Dictionary::append(buf,ZT_TRACE_FIELD_TYPE,ZT_TRACE_VL1_INCOMING_PACKET_DROPPED);
	Dictionary::append(buf,ZT_TRACE_FIELD_CODE_LOCATION,codeLocation);
	Dictionary::appendPacketId(buf,ZT_TRACE_FIELD_PACKET_ID,packetId);
	Dictionary::append(buf,ZT_TRACE_FIELD_NETWORK_ID,networkId);
	Dictionary::append(buf,ZT_TRACE_FIELD_IDENTITY_FINGERPRINT_HASH,peerIdentity.fingerprint().hash(),ZT_FINGERPRINT_HASH_SIZE);
	if (physicalAddress)
		Dictionary::append(buf,ZT_TRACE_FIELD_ENDPOINT,Endpoint(physicalAddress));
	Dictionary::append(buf,ZT_TRACE_FIELD_PACKET_HOPS,hops);
	Dictionary::append(buf,ZT_TRACE_FIELD_PACKET_VERB,verb);
	Dictionary::append(buf,ZT_TRACE_FIELD_REASON,reason);
	buf.push_back(0);
	RR->node->postEvent(tPtr,ZT_EVENT_TRACE,buf.data());
}

void Trace::_outgoingNetworkFrameDropped(
	void *const tPtr,
	const uint32_t codeLocation,
	const uint64_t networkId,
	const MAC &sourceMac,
	const MAC &destMac,
	const uint16_t etherType,
	const uint16_t frameLength,
	const uint8_t *frameData,
	const ZT_TraceFrameDropReason reason)
{
	FCV<uint8_t,4096> buf;
	Dictionary::append(buf,ZT_TRACE_FIELD_TYPE,ZT_TRACE_VL1_INCOMING_PACKET_DROPPED);
	Dictionary::append(buf,ZT_TRACE_FIELD_CODE_LOCATION,codeLocation);
	Dictionary::append(buf,ZT_TRACE_FIELD_SOURCE_MAC,sourceMac.toInt());
	Dictionary::append(buf,ZT_TRACE_FIELD_DEST_MAC,destMac.toInt());
	Dictionary::append(buf,ZT_TRACE_FIELD_ETHERTYPE,etherType);
	Dictionary::append(buf,ZT_TRACE_FIELD_FRAME_LENGTH,frameLength);
	if (frameData)
		Dictionary::append(buf,ZT_TRACE_FIELD_FRAME_DATA,frameData,std::min((unsigned int)64,(unsigned int)frameLength));
	Dictionary::append(buf,ZT_TRACE_FIELD_REASON,reason);
	buf.push_back(0);
	RR->node->postEvent(tPtr,ZT_EVENT_TRACE,buf.data());
}

void Trace::_incomingNetworkFrameDropped(
	void *const tPtr,
	const uint32_t codeLocation,
	const uint64_t networkId,
	const MAC &sourceMac,
	const MAC &destMac,
	const Identity &peerIdentity,
	const InetAddress &physicalAddress,
	const uint8_t hops,
	const uint16_t frameLength,
	const uint8_t *frameData,
	const uint8_t verb,
	const bool credentialRequestSent,
	const ZT_TraceFrameDropReason reason)
{
	FCV<uint8_t,4096> buf;
	Dictionary::append(buf,ZT_TRACE_FIELD_TYPE,ZT_TRACE_VL2_INCOMING_FRAME_DROPPED);
	Dictionary::append(buf,ZT_TRACE_FIELD_CODE_LOCATION,codeLocation);
	Dictionary::append(buf,ZT_TRACE_FIELD_SOURCE_MAC,sourceMac.toInt());
	Dictionary::append(buf,ZT_TRACE_FIELD_DEST_MAC,destMac.toInt());
	Dictionary::append(buf,ZT_TRACE_FIELD_IDENTITY_FINGERPRINT_HASH,peerIdentity.fingerprint().hash(),ZT_FINGERPRINT_HASH_SIZE);
	if (physicalAddress)
		Dictionary::appendObject(buf,ZT_TRACE_FIELD_ENDPOINT,Endpoint(physicalAddress));
	Dictionary::append(buf,ZT_TRACE_FIELD_PACKET_HOPS,hops);
	Dictionary::append(buf,ZT_TRACE_FIELD_PACKET_VERB,verb);
	Dictionary::append(buf,ZT_TRACE_FIELD_FRAME_LENGTH,frameLength);
	if (frameData)
		Dictionary::append(buf,ZT_TRACE_FIELD_FRAME_DATA,frameData,std::min((unsigned int)64,(unsigned int)frameLength));
	Dictionary::append(buf,ZT_TRACE_FIELD_FLAG_CREDENTIAL_REQUEST_SENT,credentialRequestSent);
	Dictionary::append(buf,ZT_TRACE_FIELD_REASON,reason);
	buf.push_back(0);
	RR->node->postEvent(tPtr,ZT_EVENT_TRACE,buf.data());
}

void Trace::_networkConfigRequestSent(
	void *const tPtr,
	const uint32_t codeLocation,
	const uint64_t networkId)
{
	FCV<uint8_t,4096> buf;
	Dictionary::append(buf,ZT_TRACE_FIELD_TYPE,ZT_TRACE_VL2_NETWORK_CONFIG_REQUESTED);
	Dictionary::append(buf,ZT_TRACE_FIELD_CODE_LOCATION,codeLocation);
	Dictionary::append(buf,ZT_TRACE_FIELD_NETWORK_ID,networkId);
	buf.push_back(0);
	RR->node->postEvent(tPtr,ZT_EVENT_TRACE,buf.data());
}

void Trace::_networkFilter(
	void *const tPtr,
	const uint32_t codeLocation,
	const uint64_t networkId,
	const uint8_t primaryRuleSetLog[512],
	const uint8_t matchingCapabilityRuleSetLog[512],
	const uint32_t matchingCapabilityId,
	const int64_t matchingCapabilityTimestamp,
	const Address &source,
	const Address &dest,
	const MAC &sourceMac,
	const MAC &destMac,
	const uint16_t frameLength,
	const uint8_t *frameData,
	const uint16_t etherType,
	const uint16_t vlanId,
	const bool noTee,
	const bool inbound,
	const int accept)
{
	FCV<uint8_t,4096> buf;
	Dictionary::append(buf,ZT_TRACE_FIELD_TYPE,ZT_TRACE_VL2_NETWORK_FILTER);
	Dictionary::append(buf,ZT_TRACE_FIELD_CODE_LOCATION,codeLocation);
	Dictionary::append(buf,ZT_TRACE_FIELD_NETWORK_ID,networkId);
	if ((primaryRuleSetLog)&&(!Utils::allZero(primaryRuleSetLog,512)))
		Dictionary::append(buf,ZT_TRACE_FIELD_PRIMARY_RULE_SET_LOG,primaryRuleSetLog,512);
	if ((matchingCapabilityRuleSetLog)&&(!Utils::allZero(matchingCapabilityRuleSetLog,512)))
		Dictionary::append(buf,ZT_TRACE_FIELD_MATCHING_CAPABILITY_RULE_SET_LOG,matchingCapabilityRuleSetLog,512);
	Dictionary::append(buf,ZT_TRACE_FIELD_MATCHING_CAPABILITY_ID,matchingCapabilityId);
	Dictionary::append(buf,ZT_TRACE_FIELD_MATCHING_CAPABILITY_TIMESTAMP,matchingCapabilityTimestamp);
	Dictionary::append(buf,ZT_TRACE_FIELD_SOURCE_ZT_ADDRESS,source);
	Dictionary::append(buf,ZT_TRACE_FIELD_DEST_ZT_ADDRESS,dest);
	Dictionary::append(buf,ZT_TRACE_FIELD_SOURCE_MAC,sourceMac.toInt());
	Dictionary::append(buf,ZT_TRACE_FIELD_DEST_MAC,destMac.toInt());
	Dictionary::append(buf,ZT_TRACE_FIELD_FRAME_LENGTH,frameLength);
	if (frameData)
		Dictionary::append(buf,ZT_TRACE_FIELD_FRAME_DATA,frameData,std::min((unsigned int)64,(unsigned int)frameLength));
	Dictionary::append(buf,ZT_TRACE_FIELD_ETHERTYPE,etherType);
	Dictionary::append(buf,ZT_TRACE_FIELD_VLAN_ID,vlanId);
	Dictionary::append(buf,ZT_TRACE_FIELD_RULE_FLAG_NOTEE,noTee);
	Dictionary::append(buf,ZT_TRACE_FIELD_RULE_FLAG_INBOUND,inbound);
	Dictionary::append(buf,ZT_TRACE_FIELD_RULE_FLAG_ACCEPT,(int32_t)accept);
	buf.push_back(0);
	RR->node->postEvent(tPtr,ZT_EVENT_TRACE,buf.data());
}

void Trace::_credentialRejected(
	void *const tPtr,
	const uint32_t codeLocation,
	const uint64_t networkId,
	const Identity &identity,
	const uint32_t credentialId,
	const int64_t credentialTimestamp,
	const uint8_t credentialType,
	const ZT_TraceCredentialRejectionReason reason)
{
	FCV<uint8_t,4096> buf;
	Dictionary::append(buf,ZT_TRACE_FIELD_TYPE,ZT_TRACE_VL2_NETWORK_FILTER);
	Dictionary::append(buf,ZT_TRACE_FIELD_CODE_LOCATION,codeLocation);
	Dictionary::append(buf,ZT_TRACE_FIELD_NETWORK_ID,networkId);
	Dictionary::append(buf,ZT_TRACE_FIELD_IDENTITY_FINGERPRINT_HASH,identity.fingerprint().hash(),ZT_FINGERPRINT_HASH_SIZE);
	Dictionary::append(buf,ZT_TRACE_FIELD_CREDENTIAL_ID,credentialId);
	Dictionary::append(buf,ZT_TRACE_FIELD_CREDENTIAL_TIMESTAMP,credentialTimestamp);
	Dictionary::append(buf,ZT_TRACE_FIELD_CREDENTIAL_TYPE,credentialType);
	Dictionary::append(buf,ZT_TRACE_FIELD_REASON,reason);
	buf.push_back(0);
	RR->node->postEvent(tPtr,ZT_EVENT_TRACE,buf.data());
}

} // namespace ZeroTier
