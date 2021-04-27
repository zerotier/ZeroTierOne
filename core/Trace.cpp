/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#include "Trace.hpp"

#include "Context.hpp"
#include "FCV.hpp"
#include "InetAddress.hpp"
#include "Node.hpp"
#include "Peer.hpp"

// NOTE: packet IDs are always handled in network byte order, so no need to convert them.

namespace ZeroTier {

Trace::Trace(const Context& ctx) : m_ctx(ctx), m_traceFlags(0)
{
}

void Trace::unexpectedError(const CallContext& cc, uint32_t codeLocation, const char* message, ...)
{
    FCV<uint8_t, 4096> buf;
    Dictionary::append(buf, ZT_TRACE_FIELD_TYPE, ZT_TRACE_UNEXPECTED_ERROR);
    Dictionary::append(buf, ZT_TRACE_FIELD_CODE_LOCATION, codeLocation);
    Dictionary::append(buf, ZT_TRACE_FIELD_MESSAGE, message);
    buf.push_back(0);
    m_ctx.node->postEvent(cc.tPtr, ZT_EVENT_TRACE, buf.data());
}

void Trace::m_resettingPathsInScope(void* tPtr, uint32_t codeLocation, const Identity& reporter, const InetAddress& from, const InetAddress& oldExternal, const InetAddress& newExternal, ZT_InetAddress_IpScope scope)
{
    FCV<uint8_t, 4096> buf;
    Dictionary::append(buf, ZT_TRACE_FIELD_TYPE, ZT_TRACE_VL1_RESETTING_PATHS_IN_SCOPE);
    Dictionary::append(buf, ZT_TRACE_FIELD_CODE_LOCATION, codeLocation);
    if (reporter)
        Dictionary::appendObject(buf, ZT_TRACE_FIELD_IDENTITY_FINGERPRINT, reporter.fingerprint());
    if (from)
        Dictionary::appendObject(buf, ZT_TRACE_FIELD_TRIGGER_FROM_ENDPOINT, Endpoint(from));
    if (oldExternal)
        Dictionary::appendObject(buf, ZT_TRACE_FIELD_OLD_ENDPOINT, Endpoint(oldExternal));
    if (newExternal)
        Dictionary::appendObject(buf, ZT_TRACE_FIELD_NEW_ENDPOINT, Endpoint(newExternal));
    Dictionary::append(buf, ZT_TRACE_FIELD_RESET_ADDRESS_SCOPE, scope);
    buf.push_back(0);
    m_ctx.node->postEvent(tPtr, ZT_EVENT_TRACE, buf.data());
}

void Trace::m_tryingNewPath(
    void* tPtr,
    uint32_t codeLocation,
    const Identity& trying,
    const InetAddress& physicalAddress,
    const InetAddress& triggerAddress,
    uint64_t triggeringPacketId,
    uint8_t triggeringPacketVerb,
    const Identity& triggeringPeer)
{
    if ((trying) && (physicalAddress)) {
        FCV<uint8_t, 4096> buf;
        Dictionary::append(buf, ZT_TRACE_FIELD_TYPE, ZT_TRACE_VL1_TRYING_NEW_PATH);
        Dictionary::append(buf, ZT_TRACE_FIELD_CODE_LOCATION, codeLocation);
        Dictionary::appendObject(buf, ZT_TRACE_FIELD_IDENTITY_FINGERPRINT, trying.fingerprint());
        Dictionary::appendObject(buf, ZT_TRACE_FIELD_ENDPOINT, physicalAddress);
        if (triggerAddress)
            Dictionary::appendObject(buf, ZT_TRACE_FIELD_TRIGGER_FROM_ENDPOINT, Endpoint(triggerAddress));
        Dictionary::appendPacketId(buf, ZT_TRACE_FIELD_TRIGGER_FROM_PACKET_ID, triggeringPacketId);
        Dictionary::append(buf, ZT_TRACE_FIELD_TRIGGER_FROM_PACKET_VERB, triggeringPacketVerb);
        if (triggeringPeer)
            Dictionary::appendObject(buf, ZT_TRACE_FIELD_TRIGGER_FROM_PEER_FINGERPRINT, triggeringPeer.fingerprint());
        buf.push_back(0);
        m_ctx.node->postEvent(tPtr, ZT_EVENT_TRACE, buf.data());
    }
}

void Trace::m_learnedNewPath(void* tPtr, uint32_t codeLocation, uint64_t packetId, const Identity& peerIdentity, const InetAddress& physicalAddress, const InetAddress& replaced)
{
    if (peerIdentity) {
        FCV<uint8_t, 4096> buf;
        Dictionary::append(buf, ZT_TRACE_FIELD_TYPE, ZT_TRACE_VL1_LEARNED_NEW_PATH);
        Dictionary::append(buf, ZT_TRACE_FIELD_CODE_LOCATION, codeLocation);
        Dictionary::appendPacketId(buf, ZT_TRACE_FIELD_PACKET_ID, packetId);
        Dictionary::appendObject(buf, ZT_TRACE_FIELD_IDENTITY_FINGERPRINT, peerIdentity.fingerprint());
        if (physicalAddress)
            Dictionary::appendObject(buf, ZT_TRACE_FIELD_ENDPOINT, Endpoint(physicalAddress));
        if (replaced)
            Dictionary::appendObject(buf, ZT_TRACE_FIELD_OLD_ENDPOINT, Endpoint(replaced));
        buf.push_back(0);
        m_ctx.node->postEvent(tPtr, ZT_EVENT_TRACE, buf.data());
    }
}

void Trace::m_incomingPacketDropped(void* tPtr, uint32_t codeLocation, uint64_t packetId, uint64_t networkId, const Identity& peerIdentity, const InetAddress& physicalAddress, uint8_t hops, uint8_t verb, ZT_TracePacketDropReason reason)
{
    FCV<uint8_t, 4096> buf;
    Dictionary::append(buf, ZT_TRACE_FIELD_TYPE, ZT_TRACE_VL1_INCOMING_PACKET_DROPPED);
    Dictionary::append(buf, ZT_TRACE_FIELD_CODE_LOCATION, codeLocation);
    Dictionary::appendPacketId(buf, ZT_TRACE_FIELD_PACKET_ID, packetId);
    Dictionary::append(buf, ZT_TRACE_FIELD_NETWORK_ID, networkId);
    if (peerIdentity)
        Dictionary::appendObject(buf, ZT_TRACE_FIELD_IDENTITY_FINGERPRINT, peerIdentity.fingerprint());
    if (physicalAddress)
        Dictionary::append(buf, ZT_TRACE_FIELD_ENDPOINT, Endpoint(physicalAddress));
    Dictionary::append(buf, ZT_TRACE_FIELD_PACKET_HOPS, hops);
    Dictionary::append(buf, ZT_TRACE_FIELD_PACKET_VERB, verb);
    Dictionary::append(buf, ZT_TRACE_FIELD_REASON, reason);
    buf.push_back(0);
    m_ctx.node->postEvent(tPtr, ZT_EVENT_TRACE, buf.data());
}

void Trace::m_outgoingNetworkFrameDropped(void* tPtr, uint32_t codeLocation, uint64_t networkId, const MAC& sourceMac, const MAC& destMac, uint16_t etherType, uint16_t frameLength, const uint8_t* frameData, ZT_TraceFrameDropReason reason)
{
    FCV<uint8_t, 4096> buf;
    Dictionary::append(buf, ZT_TRACE_FIELD_TYPE, ZT_TRACE_VL1_INCOMING_PACKET_DROPPED);
    Dictionary::append(buf, ZT_TRACE_FIELD_CODE_LOCATION, codeLocation);
    Dictionary::append(buf, ZT_TRACE_FIELD_NETWORK_ID, networkId);
    Dictionary::append(buf, ZT_TRACE_FIELD_SOURCE_MAC, sourceMac.toInt());
    Dictionary::append(buf, ZT_TRACE_FIELD_DEST_MAC, destMac.toInt());
    Dictionary::append(buf, ZT_TRACE_FIELD_ETHERTYPE, etherType);
    Dictionary::append(buf, ZT_TRACE_FIELD_FRAME_LENGTH, frameLength);
    if (frameData)
        Dictionary::append(buf, ZT_TRACE_FIELD_FRAME_DATA, frameData, std::min((unsigned int)64, (unsigned int)frameLength));
    Dictionary::append(buf, ZT_TRACE_FIELD_REASON, reason);
    buf.push_back(0);
    m_ctx.node->postEvent(tPtr, ZT_EVENT_TRACE, buf.data());
}

void Trace::m_incomingNetworkFrameDropped(
    void* tPtr,
    uint32_t codeLocation,
    uint64_t networkId,
    const MAC& sourceMac,
    const MAC& destMac,
    const uint16_t etherType,
    const Identity& peerIdentity,
    const InetAddress& physicalAddress,
    uint8_t hops,
    uint16_t frameLength,
    const uint8_t* frameData,
    uint8_t verb,
    bool credentialRequestSent,
    ZT_TraceFrameDropReason reason)
{
    FCV<uint8_t, 4096> buf;
    Dictionary::append(buf, ZT_TRACE_FIELD_TYPE, ZT_TRACE_VL2_INCOMING_FRAME_DROPPED);
    Dictionary::append(buf, ZT_TRACE_FIELD_CODE_LOCATION, codeLocation);
    Dictionary::append(buf, ZT_TRACE_FIELD_NETWORK_ID, networkId);
    Dictionary::append(buf, ZT_TRACE_FIELD_SOURCE_MAC, sourceMac.toInt());
    Dictionary::append(buf, ZT_TRACE_FIELD_DEST_MAC, destMac.toInt());
    Dictionary::append(buf, ZT_TRACE_FIELD_ETHERTYPE, etherType);
    Dictionary::appendObject(buf, ZT_TRACE_FIELD_IDENTITY_FINGERPRINT, peerIdentity.fingerprint());
    if (physicalAddress)
        Dictionary::appendObject(buf, ZT_TRACE_FIELD_ENDPOINT, Endpoint(physicalAddress));
    Dictionary::append(buf, ZT_TRACE_FIELD_PACKET_HOPS, hops);
    Dictionary::append(buf, ZT_TRACE_FIELD_PACKET_VERB, verb);
    Dictionary::append(buf, ZT_TRACE_FIELD_FRAME_LENGTH, frameLength);
    if (frameData)
        Dictionary::append(buf, ZT_TRACE_FIELD_FRAME_DATA, frameData, std::min((unsigned int)64, (unsigned int)frameLength));
    Dictionary::append(buf, ZT_TRACE_FIELD_FLAG_CREDENTIAL_REQUEST_SENT, credentialRequestSent);
    Dictionary::append(buf, ZT_TRACE_FIELD_REASON, reason);
    buf.push_back(0);
    m_ctx.node->postEvent(tPtr, ZT_EVENT_TRACE, buf.data());
}

void Trace::m_networkConfigRequestSent(void* tPtr, uint32_t codeLocation, uint64_t networkId)
{
    FCV<uint8_t, 4096> buf;
    Dictionary::append(buf, ZT_TRACE_FIELD_TYPE, ZT_TRACE_VL2_NETWORK_CONFIG_REQUESTED);
    Dictionary::append(buf, ZT_TRACE_FIELD_CODE_LOCATION, codeLocation);
    Dictionary::append(buf, ZT_TRACE_FIELD_NETWORK_ID, networkId);
    buf.push_back(0);
    m_ctx.node->postEvent(tPtr, ZT_EVENT_TRACE, buf.data());
}

void Trace::m_networkFilter(
    void* tPtr,
    uint32_t codeLocation,
    uint64_t networkId,
    const uint8_t* primaryRuleSetLog,
    const uint8_t* matchingCapabilityRuleSetLog,
    uint32_t matchingCapabilityId,
    int64_t matchingCapabilityTimestamp,
    const Address& source,
    const Address& dest,
    const MAC& sourceMac,
    const MAC& destMac,
    uint16_t frameLength,
    const uint8_t* frameData,
    uint16_t etherType,
    uint16_t vlanId,
    bool noTee,
    bool inbound,
    int accept)
{
    FCV<uint8_t, 4096> buf;
    Dictionary::append(buf, ZT_TRACE_FIELD_TYPE, ZT_TRACE_VL2_NETWORK_FILTER);
    Dictionary::append(buf, ZT_TRACE_FIELD_CODE_LOCATION, codeLocation);
    Dictionary::append(buf, ZT_TRACE_FIELD_NETWORK_ID, networkId);
    if ((primaryRuleSetLog) && (! Utils::allZero(primaryRuleSetLog, 512)))
        Dictionary::append(buf, ZT_TRACE_FIELD_PRIMARY_RULE_SET_LOG, primaryRuleSetLog, 512);
    if ((matchingCapabilityRuleSetLog) && (! Utils::allZero(matchingCapabilityRuleSetLog, 512)))
        Dictionary::append(buf, ZT_TRACE_FIELD_MATCHING_CAPABILITY_RULE_SET_LOG, matchingCapabilityRuleSetLog, 512);
    Dictionary::append(buf, ZT_TRACE_FIELD_MATCHING_CAPABILITY_ID, matchingCapabilityId);
    Dictionary::append(buf, ZT_TRACE_FIELD_MATCHING_CAPABILITY_TIMESTAMP, matchingCapabilityTimestamp);
    Dictionary::append(buf, ZT_TRACE_FIELD_SOURCE_ZT_ADDRESS, source);
    Dictionary::append(buf, ZT_TRACE_FIELD_DEST_ZT_ADDRESS, dest);
    Dictionary::append(buf, ZT_TRACE_FIELD_SOURCE_MAC, sourceMac.toInt());
    Dictionary::append(buf, ZT_TRACE_FIELD_DEST_MAC, destMac.toInt());
    Dictionary::append(buf, ZT_TRACE_FIELD_FRAME_LENGTH, frameLength);
    if (frameData)
        Dictionary::append(buf, ZT_TRACE_FIELD_FRAME_DATA, frameData, std::min((unsigned int)64, (unsigned int)frameLength));
    Dictionary::append(buf, ZT_TRACE_FIELD_ETHERTYPE, etherType);
    Dictionary::append(buf, ZT_TRACE_FIELD_VLAN_ID, vlanId);
    Dictionary::append(buf, ZT_TRACE_FIELD_RULE_FLAG_NOTEE, noTee);
    Dictionary::append(buf, ZT_TRACE_FIELD_RULE_FLAG_INBOUND, inbound);
    Dictionary::append(buf, ZT_TRACE_FIELD_RULE_FLAG_ACCEPT, (int32_t)accept);
    buf.push_back(0);
    m_ctx.node->postEvent(tPtr, ZT_EVENT_TRACE, buf.data());
}

void Trace::m_credentialRejected(void* tPtr, uint32_t codeLocation, uint64_t networkId, const Identity& identity, uint32_t credentialId, int64_t credentialTimestamp, uint8_t credentialType, ZT_TraceCredentialRejectionReason reason)
{
    FCV<uint8_t, 4096> buf;
    Dictionary::append(buf, ZT_TRACE_FIELD_TYPE, ZT_TRACE_VL2_NETWORK_CREDENTIAL_REJECTED);
    Dictionary::append(buf, ZT_TRACE_FIELD_CODE_LOCATION, codeLocation);
    Dictionary::append(buf, ZT_TRACE_FIELD_NETWORK_ID, networkId);
    Dictionary::appendObject(buf, ZT_TRACE_FIELD_IDENTITY_FINGERPRINT, identity.fingerprint());
    Dictionary::append(buf, ZT_TRACE_FIELD_CREDENTIAL_ID, credentialId);
    Dictionary::append(buf, ZT_TRACE_FIELD_CREDENTIAL_TIMESTAMP, credentialTimestamp);
    Dictionary::append(buf, ZT_TRACE_FIELD_CREDENTIAL_TYPE, credentialType);
    Dictionary::append(buf, ZT_TRACE_FIELD_REASON, reason);
    buf.push_back(0);
    m_ctx.node->postEvent(tPtr, ZT_EVENT_TRACE, buf.data());
}

}   // namespace ZeroTier
