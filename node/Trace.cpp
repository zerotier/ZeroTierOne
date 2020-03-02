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
#include "Path.hpp"
#include "InetAddress.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstdarg>

// NOTE: packet IDs are always handled in network byte order, so no need to convert them.

namespace ZeroTier {

Trace::Trace(const RuntimeEnvironment *renv) :
	RR(renv),
	_vl1(false),
	_vl2(false),
	_vl2Filter(false),
	_vl2Multicast(false)
{
}

Trace::Str<ZT_INETADDRESS_STRING_SIZE_MAX> Trace::str(const InetAddress &a,const bool ipOnly)
{
	Str<ZT_INETADDRESS_STRING_SIZE_MAX> s;
	if (ipOnly)
		a.toIpString(s.s);
	else a.toString(s.s);
	return s;
}

Trace::Str<ZT_ADDRESS_STRING_SIZE_MAX> Trace::str(const Address &a)
{
	Str<ZT_ADDRESS_STRING_SIZE_MAX> s;
	a.toString(s.s);
	return s;
}

Trace::Str<ZT_ADDRESS_STRING_SIZE_MAX + ZT_INETADDRESS_STRING_SIZE_MAX + 4> Trace::str(const Address &peerAddress,const SharedPtr<Path> &path)
{
	Str<ZT_ADDRESS_STRING_SIZE_MAX + ZT_INETADDRESS_STRING_SIZE_MAX + 4> s;
	peerAddress.toString(s.s);
	s.s[11] = '(';
	path->address().toString(s.s + 12);
	int x = strlen(s.s);
	s.s[x] = ')';
	s.s[x+1] = 0;
	return s;
}

void Trace::unexpectedError(
	void *tPtr,
	uint32_t codeLocation,
	const char *message,
	...)
{
	va_list ap;
	ZT_TraceEvent_UNEXPECTED_ERROR ev;
	ev.evSize = ZT_CONST_TO_BE_UINT16(sizeof(ev));
	ev.evType = ZT_CONST_TO_BE_UINT16(ZT_TRACE_UNEXPECTED_ERROR);
	ev.codeLocation = codeLocation;
	memset(ev.message,0,sizeof(ev.message));
	va_start(ap,message);
	vsnprintf(ev.message,sizeof(ev.message),message,ap);
	va_end(ap);
	RR->node->postEvent(tPtr,ZT_EVENT_TRACE,&ev);
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
	ZT_TraceEvent_VL1_RESETTING_PATHS_IN_SCOPE ev;
	ev.evSize = ZT_CONST_TO_BE_UINT16(sizeof(ev));
	ev.evType = ZT_CONST_TO_BE_UINT16(ZT_TRACE_VL1_RESETTING_PATHS_IN_SCOPE);
	ev.codeLocation = Utils::hton(codeLocation);
	from.forTrace(ev.from);
	oldExternal.forTrace(ev.oldExternal);
	newExternal.forTrace(ev.newExternal);
	ev.scope = (uint8_t)scope;
	RR->node->postEvent(tPtr,ZT_EVENT_TRACE,&ev);
}

void Trace::_tryingNewPath(
	void *const tPtr,
	const uint32_t codeLocation,
	const Identity &trying,
	const InetAddress &physicalAddress,
	const InetAddress &triggerAddress,
	const uint64_t triggeringPacketId,
	const uint8_t triggeringPacketVerb,
	const Identity &triggeringPeer,
	const ZT_TraceTryingNewPathReason reason)
{
	ZT_TraceEvent_VL1_TRYING_NEW_PATH ev;
	ev.evSize = ZT_CONST_TO_BE_UINT16(sizeof(ev));
	ev.evType = ZT_CONST_TO_BE_UINT16(ZT_TRACE_VL1_TRYING_NEW_PATH);
	ev.codeLocation = Utils::hton(codeLocation);
	trying.fingerprint().setZTFingerprint(&ev.peer);
	physicalAddress.forTrace(ev.physicalAddress);
	triggerAddress.forTrace(ev.triggerAddress);
	ev.triggeringPacketId = triggeringPacketId;
	ev.triggeringPacketVerb = triggeringPacketVerb;
	triggeringPeer.fingerprint().setZTFingerprint(&ev.triggeringPeer);
	ev.reason = (uint8_t)reason;
	RR->node->postEvent(tPtr,ZT_EVENT_TRACE,&ev);
}

void Trace::_learnedNewPath(
	void *const tPtr,
	const uint32_t codeLocation,
	const uint64_t packetId,
	const Identity &peerIdentity,
	const InetAddress &physicalAddress,
	const InetAddress &replaced)
{
	ZT_TraceEvent_VL1_LEARNED_NEW_PATH ev;
	ev.evSize = ZT_CONST_TO_BE_UINT16(sizeof(ev));
	ev.evType = ZT_CONST_TO_BE_UINT16(ZT_TRACE_VL1_LEARNED_NEW_PATH);
	ev.codeLocation = Utils::hton(codeLocation);
	ev.packetId = packetId; // packet IDs are kept in big-endian
	peerIdentity.fingerprint().setZTFingerprint(&ev.peer);
	physicalAddress.forTrace(ev.physicalAddress);
	replaced.forTrace(ev.replaced);

	RR->node->postEvent(tPtr,ZT_EVENT_TRACE,&ev);
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
	ZT_TraceEvent_VL1_INCOMING_PACKET_DROPPED ev;
	ev.evSize = ZT_CONST_TO_BE_UINT16(sizeof(ev));
	ev.evType = ZT_CONST_TO_BE_UINT16(ZT_TRACE_VL1_INCOMING_PACKET_DROPPED);
	ev.codeLocation = Utils::hton(codeLocation);
	ev.packetId = packetId; // packet IDs are kept in big-endian
	ev.networkId = Utils::hton(networkId);
	peerIdentity.fingerprint().setZTFingerprint(&ev.peer);
	physicalAddress.forTrace(ev.physicalAddress);
	ev.hops = hops;
	ev.verb = verb;
	ev.reason = (uint8_t)reason;

	RR->node->postEvent(tPtr,ZT_EVENT_TRACE,&ev);
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
	ZT_TraceEvent_VL2_OUTGOING_FRAME_DROPPED ev;
	ev.evSize = ZT_CONST_TO_BE_UINT16(sizeof(ev));
	ev.evType = ZT_CONST_TO_BE_UINT16(ZT_TRACE_VL2_OUTGOING_FRAME_DROPPED);
	ev.codeLocation = Utils::hton(codeLocation);
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
	ZT_TraceEvent_VL2_INCOMING_FRAME_DROPPED ev;
	ev.evSize = ZT_CONST_TO_BE_UINT16(sizeof(ev));
	ev.evType = ZT_CONST_TO_BE_UINT16(ZT_TRACE_VL2_INCOMING_FRAME_DROPPED);
	ev.codeLocation = Utils::hton(codeLocation);
	ev.networkId = Utils::hton(networkId);
	ev.sourceMac = Utils::hton(sourceMac.toInt());
	ev.destMac = Utils::hton(destMac.toInt());
	peerIdentity.fingerprint().setZTFingerprint(&ev.sender);
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
	const uint32_t codeLocation,
	const uint64_t networkId)
{
	ZT_TraceEvent_VL2_NETWORK_CONFIG_REQUESTED ev;
	ev.evSize = ZT_CONST_TO_BE_UINT16(sizeof(ev));
	ev.evType = ZT_CONST_TO_BE_UINT16(ZT_TRACE_VL2_NETWORK_CONFIG_REQUESTED);
	ev.codeLocation = Utils::hton(codeLocation);
	ev.networkId = Utils::hton(networkId);
	RR->node->postEvent(tPtr,ZT_EVENT_TRACE,&ev);
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
	ZT_TraceEvent_VL2_NETWORK_FILTER ev;
	ev.evSize = ZT_CONST_TO_BE_UINT16(sizeof(ev));
	ev.evType = ZT_CONST_TO_BE_UINT16(ZT_TRACE_VL2_NETWORK_FILTER);
	ev.codeLocation = Utils::hton(codeLocation);
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
	const uint32_t codeLocation,
	const uint64_t networkId,
	const Address &address,
	const Identity &identity,
	const uint32_t credentialId,
	const int64_t credentialTimestamp,
	const uint8_t credentialType,
	const ZT_TraceCredentialRejectionReason reason)
{
	ZT_TraceEvent_VL2_CREDENTIAL_REJECTED ev;
	ev.evSize = ZT_CONST_TO_BE_UINT16(sizeof(ev));
	ev.evType = ZT_CONST_TO_BE_UINT16(ZT_TRACE_VL2_NETWORK_FILTER);
	ev.codeLocation = Utils::hton(codeLocation);
	ev.networkId = Utils::hton(networkId);
	if (identity) {
		identity.fingerprint().setZTFingerprint(&ev.peer);
	} else {
		ev.peer.address = address.toInt();
		memset(ev.peer.hash,0,sizeof(ev.peer.hash));
	}
	ev.credentialId = Utils::hton(credentialId);
	ev.credentialTimestamp = Utils::hton(credentialTimestamp);
	ev.credentialType = credentialType;
	ev.reason = (uint8_t)reason;
	RR->node->postEvent(tPtr,ZT_EVENT_TRACE,&ev);
}

} // namespace ZeroTier
