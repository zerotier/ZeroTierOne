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

// Macro to avoid calling hton() on values known at compile time.
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define CONST_TO_BE_UINT16(x) ((uint16_t)((uint16_t)((uint16_t)(x) << 8U) | (uint16_t)((uint16_t)(x) >> 8U)))
#else
#define CONST_TO_BE_UINT16(x) ((uint16_t)(x))
#endif

// NOTE: packet IDs are always handled in network byte order, so no need to convert them.

namespace ZeroTier {

Trace::Trace(const RuntimeEnvironment *renv) :
	RR(renv),
	_vl1(false),
	_vl2(false),
	_vl2Filter(false),
	_vl2Multicast(false),
	_eventBufSize(0)
{
}

void Trace::_resettingPathsInScope(
	void *const tPtr,
	const Identity &reporter,
	const InetAddress &from,
	const InetAddress &oldExternal,
	const InetAddress &newExternal,
	const InetAddress::IpScope scope)
{
	ZT_TraceEvent_VL1_RESETTING_PATHS_IN_SCOPE ev;
	ev.evSize = CONST_TO_BE_UINT16(sizeof(ev));
	ev.evType = CONST_TO_BE_UINT16(ZT_TRACE_VL1_RESETTING_PATHS_IN_SCOPE);
	from.forTrace(ev.from);
	oldExternal.forTrace(ev.oldExternal);
	newExternal.forTrace(ev.newExternal);
	ev.scope = (uint8_t)scope;

	RR->node->postEvent(tPtr,ZT_EVENT_TRACE,&ev);
}

void Trace::_tryingNewPath(
	void *const tPtr,
	const Identity &trying,
	const InetAddress &physicalAddress,
	const InetAddress &triggerAddress,
	uint64_t triggeringPacketId,
	uint8_t triggeringPacketVerb,
	uint64_t triggeredByAddress,
	const uint8_t *triggeredByIdentityHash,
	ZT_TraceTryingNewPathReason reason)
{
	ZT_TraceEvent_VL1_TRYING_NEW_PATH ev;
	ev.evSize = CONST_TO_BE_UINT16(sizeof(ev));
	ev.evType = CONST_TO_BE_UINT16(ZT_TRACE_VL1_TRYING_NEW_PATH);
	ev.address = Utils::hton(trying.address().toInt());
	memcpy(ev.identityHash,trying.hash(),48);
	physicalAddress.forTrace(ev.physicalAddress);
	triggerAddress.forTrace(ev.triggerAddress);
	ev.triggeringPacketId = triggeringPacketId;
	ev.triggeringPacketVerb = triggeringPacketVerb;
	ev.triggeredByAddress = Utils::hton(triggeredByAddress);
	if (triggeredByIdentityHash)
		memcpy(ev.triggeredByIdentityHash,triggeredByIdentityHash,48);
	else memset(ev.triggeredByIdentityHash,0,48);
	ev.reason = (uint8_t)reason;

	RR->node->postEvent(tPtr,ZT_EVENT_TRACE,&ev);
}

void Trace::_learnedNewPath(
	void *const tPtr,
	uint64_t packetId,
	const Identity &peerIdentity,
	const InetAddress &physicalAddress,
	const InetAddress &replaced)
{
	ZT_TraceEvent_VL1_LEARNED_NEW_PATH ev;
	ev.evSize = CONST_TO_BE_UINT16(sizeof(ev));
	ev.evType = CONST_TO_BE_UINT16(ZT_TRACE_VL1_LEARNED_NEW_PATH);
	ev.packetId = packetId;
	ev.address = Utils::hton(peerIdentity.address().toInt());
	memcpy(ev.identityHash,peerIdentity.hash(),48);
	physicalAddress.forTrace(ev.physicalAddress);
	replaced.forTrace(ev.replaced);

	RR->node->postEvent(tPtr,ZT_EVENT_TRACE,&ev);
}

void Trace::_incomingPacketDropped(
	void *const tPtr,
	uint64_t packetId,
	uint64_t networkId,
	const Identity &peerIdentity,
	const InetAddress &physicalAddress,
	uint8_t hops,
	uint8_t verb,
	ZT_TracePacketDropReason reason)
{
	ZT_TraceEvent_VL1_INCOMING_PACKET_DROPPED ev;
	ev.evSize = CONST_TO_BE_UINT16(sizeof(ev));
	ev.evType = CONST_TO_BE_UINT16(ZT_TRACE_VL1_INCOMING_PACKET_DROPPED);
	ev.packetId = packetId;
	ev.networkId = Utils::hton(networkId);
	if (peerIdentity) {
		ev.address = Utils::hton(peerIdentity.address().toInt());
		memcpy(ev.identityHash,peerIdentity.hash(),48);
	} else {
		ev.address = 0;
		memset(ev.identityHash,0,48);
	}
	physicalAddress.forTrace(ev.physicalAddress);
	ev.hops = hops;
	ev.verb = verb;
	ev.reason = (uint8_t)reason;

	RR->node->postEvent(tPtr,ZT_EVENT_TRACE,&ev);
}

void Trace::_outgoingNetworkFrameDropped(
	void *const tPtr,
	uint64_t networkId,
	const MAC &sourceMac,
	const MAC &destMac,
	uint16_t etherType,
	uint16_t frameLength,
	const uint8_t *frameData,
	ZT_TraceFrameDropReason reason)
{
	ZT_TraceEvent_VL2_OUTGOING_FRAME_DROPPED ev;
	ev.evSize = CONST_TO_BE_UINT16(sizeof(ev));
	ev.evType = CONST_TO_BE_UINT16(ZT_TRACE_VL2_OUTGOING_FRAME_DROPPED);
	ev.networkId = Utils::hton(networkId);
	ev.sourceMac = Utils::hton(sourceMac.toInt());
	ev.destMac = Utils::hton(destMac.toInt());
	ev.etherType = Utils::hton(etherType);
	ev.frameLength = Utils::hton(frameLength);
	if (frameData) {
		unsigned int l = frameLength;
		if (l > sizeof(ev.frameHead))
			l = sizeof(ev.frameHead);
		memcpy(ev.frameHead,frameData,l);
		memset(ev.frameHead + l,0,sizeof(ev.frameHead) - l);
	}
	ev.reason = (uint8_t)reason;

	RR->node->postEvent(tPtr,ZT_EVENT_TRACE,&ev);
}

void Trace::_incomingNetworkFrameDropped(
	void *const tPtr,
	uint64_t networkId,
	const MAC &sourceMac,
	const MAC &destMac,
	const Identity &peerIdentity,
	const InetAddress &physicalAddress,
	uint8_t hops,
	uint16_t frameLength,
	const uint8_t *frameData,
	uint8_t verb,
	bool credentialRequestSent,
	ZT_TraceFrameDropReason reason)
{
	ZT_TraceEvent_VL2_INCOMING_FRAME_DROPPED ev;
	ev.evSize = CONST_TO_BE_UINT16(sizeof(ev));
	ev.evType = CONST_TO_BE_UINT16(ZT_TRACE_VL2_INCOMING_FRAME_DROPPED);
	ev.networkId = Utils::hton(networkId);
	ev.sourceMac = Utils::hton(sourceMac.toInt());
	ev.destMac = Utils::hton(destMac.toInt());
	ev.address = Utils::hton(peerIdentity.address().toInt());
	physicalAddress.forTrace(ev.physicalAddress);
	ev.hops = hops;
	ev.frameLength = Utils::hton(frameLength);
	if (frameData) {
		unsigned int l = frameLength;
		if (l > sizeof(ev.frameHead))
			l = sizeof(ev.frameHead);
		memcpy(ev.frameHead,frameData,l);
		memset(ev.frameHead + l,0,sizeof(ev.frameHead) - l);
	}
	ev.verb = verb;
	ev.credentialRequestSent = (uint8_t)credentialRequestSent;
	ev.reason = (uint8_t)reason;

	RR->node->postEvent(tPtr,ZT_EVENT_TRACE,&ev);
}

void Trace::_networkConfigRequestSent(
	void *const tPtr,
	uint64_t networkId)
{
	ZT_TraceEvent_VL2_NETWORK_CONFIG_REQUESTED ev;
	ev.evSize = CONST_TO_BE_UINT16(sizeof(ev));
	ev.evType = CONST_TO_BE_UINT16(ZT_TRACE_VL2_NETWORK_CONFIG_REQUESTED);
	ev.networkId = Utils::hton(networkId);

	RR->node->postEvent(tPtr,ZT_EVENT_TRACE,&ev);
}

void Trace::_networkFilter(
	void *const tPtr,
	uint64_t networkId,
	const uint8_t primaryRuleSetLog[512],
	const uint8_t matchingCapabilityRuleSetLog[512],
	uint32_t matchingCapabilityId,
	int64_t matchingCapabilityTimestamp,
	const Address &source,
	const Address &dest,
	const MAC &sourceMac,
	const MAC &destMac,
	uint16_t frameLength,
	const uint8_t *frameData,
	uint16_t etherType,
	uint16_t vlanId,
	bool noTee,
	bool inbound,
	int accept)
{
	ZT_TraceEvent_VL2_NETWORK_FILTER ev;
	ev.evSize = CONST_TO_BE_UINT16(sizeof(ev));
	ev.evType = CONST_TO_BE_UINT16(ZT_TRACE_VL2_NETWORK_FILTER);
	ev.networkId = Utils::hton(networkId);
	memcpy(ev.primaryRuleSetLog,primaryRuleSetLog,sizeof(ev.primaryRuleSetLog));
	if (matchingCapabilityRuleSetLog)
		memcpy(ev.matchingCapabilityRuleSetLog,matchingCapabilityRuleSetLog,sizeof(ev.matchingCapabilityRuleSetLog));
	else memset(ev.matchingCapabilityRuleSetLog,0,sizeof(ev.matchingCapabilityRuleSetLog));
	ev.matchingCapabilityId = Utils::hton(matchingCapabilityId);
	ev.matchingCapabilityTimestamp = Utils::hton(matchingCapabilityTimestamp);
	ev.source = Utils::hton(source.toInt());
	ev.dest = Utils::hton(dest.toInt());
	ev.sourceMac = Utils::hton(sourceMac.toInt());
	ev.destMac = Utils::hton(destMac.toInt());
	ev.frameLength = Utils::hton(frameLength);
	if (frameData) {
		unsigned int l = frameLength;
		if (l > sizeof(ev.frameHead))
			l = sizeof(ev.frameHead);
		memcpy(ev.frameHead,frameData,l);
		memset(ev.frameHead + l,0,sizeof(ev.frameHead) - l);
	}
	ev.etherType = Utils::hton(etherType);
	ev.vlanId = Utils::hton(vlanId);
	ev.noTee = (uint8_t)noTee;
	ev.inbound = (uint8_t)inbound;
	ev.accept = (int8_t)accept;

	RR->node->postEvent(tPtr,ZT_EVENT_TRACE,&ev);
}

void Trace::_credentialRejected(
	void *const tPtr,
	uint64_t networkId,
	const Address &address,
	uint32_t credentialId,
	int64_t credentialTimestamp,
	uint8_t credentialType,
	ZT_TraceCredentialRejectionReason reason)
{
	ZT_TraceEvent_VL2_CREDENTIAL_REJECTED ev;
	ev.evSize = CONST_TO_BE_UINT16(sizeof(ev));
	ev.evType = CONST_TO_BE_UINT16(ZT_TRACE_VL2_NETWORK_FILTER);
	ev.networkId = Utils::hton(networkId);
	ev.address = Utils::hton(address.toInt());
	ev.credentialId = Utils::hton(credentialId);
	ev.credentialTimestamp = Utils::hton(credentialTimestamp);
	ev.credentialType = credentialType;
	ev.reason = (uint8_t)reason;

	RR->node->postEvent(tPtr,ZT_EVENT_TRACE,&ev);
}

} // namespace ZeroTier
