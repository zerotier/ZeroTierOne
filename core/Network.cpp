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

#include "Network.hpp"

#include "Address.hpp"
#include "Buf.hpp"
#include "Constants.hpp"
#include "Context.hpp"
#include "InetAddress.hpp"
#include "MAC.hpp"
#include "NetworkController.hpp"
#include "Peer.hpp"
#include "ScopedPtr.hpp"
#include "Trace.hpp"

#include <cstdlib>
#include <cstring>
#include <set>

namespace ZeroTier {

namespace {

// Returns true if packet appears valid; pos and proto will be set
bool _ipv6GetPayload(const uint8_t *frameData, unsigned int frameLen, unsigned int &pos, unsigned int &proto) noexcept
{
    if (frameLen < 40)
        return false;
    pos   = 40;
    proto = frameData[6];
    while (pos <= frameLen) {
        switch (proto) {
            case 0:     // hop-by-hop options
            case 43:    // routing
            case 60:    // destination options
            case 135:   // mobility options
                if ((pos + 8) > frameLen)
                    return false;   // invalid!
                proto = frameData[pos];
                pos += ((unsigned int)frameData[pos + 1] * 8) + 8;
                break;

                // case 44: // fragment -- we currently can't parse these and they are deprecated in IPv6 anyway
                // case 50:
                // case 51: // IPSec ESP and AH -- we have to stop here since this is encrypted stuff
            default: return true;
        }
    }
    return false;   // overflow == invalid
}

enum _doZtFilterResult {
    DOZTFILTER_NO_MATCH,
    DOZTFILTER_DROP,
    DOZTFILTER_REDIRECT,
    DOZTFILTER_ACCEPT,
    DOZTFILTER_SUPER_ACCEPT
};

ZT_INLINE _doZtFilterResult _doZtFilter(
    const Context &ctx, Trace::RuleResultLog &rrl, const NetworkConfig &nconf,
    const Member *membership,   // can be NULL
    const bool inbound, const Address &ztSource,
    Address &ztDest,   // MUTABLE -- is changed on REDIRECT actions
    const MAC &macSource, const MAC &macDest, const uint8_t *const frameData, const unsigned int frameLen,
    const unsigned int etherType, const unsigned int vlanId,
    const ZT_VirtualNetworkRule *rules,   // cannot be NULL
    const unsigned int ruleCount,
    Address &cc,                   // MUTABLE -- set to TEE destination if TEE action is taken or left alone otherwise
    unsigned int &ccLength,        // MUTABLE -- set to length of packet payload to TEE
    bool &ccWatch,                 // MUTABLE -- set to true for WATCH target as opposed to normal TEE
    uint8_t &qosBucket) noexcept   // MUTABLE -- set to the value of the argument provided to PRIORITY
{
    // Set to true if we are a TEE/REDIRECT/WATCH target
    bool superAccept = false;

    // The default match state for each set of entries starts as 'true' since an
    // ACTION with no MATCH entries preceding it is always taken.
    uint8_t thisSetMatches = 1;

    rrl.clear();

    for (unsigned int rn = 0; rn < ruleCount; ++rn) {
        const ZT_VirtualNetworkRuleType rt = (ZT_VirtualNetworkRuleType)(rules[rn].t & 0x3fU);

        // First check if this is an ACTION
        if ((unsigned int)rt <= (unsigned int)ZT_NETWORK_RULE_ACTION__MAX_ID) {
            if (thisSetMatches) {
                switch (rt) {
                    case ZT_NETWORK_RULE_ACTION_PRIORITY:
                        qosBucket = (rules[rn].v.qosBucket >= 0 && rules[rn].v.qosBucket <= 8)
                                        ? rules[rn].v.qosBucket
                                        : 4;   // 4 = default bucket (no priority)
                        return DOZTFILTER_ACCEPT;

                    case ZT_NETWORK_RULE_ACTION_DROP: return DOZTFILTER_DROP;

                    case ZT_NETWORK_RULE_ACTION_ACCEPT:
                        return (superAccept ? DOZTFILTER_SUPER_ACCEPT : DOZTFILTER_ACCEPT);   // match, accept packet

                        // These are initially handled together since preliminary logic is common
                    case ZT_NETWORK_RULE_ACTION_TEE:
                    case ZT_NETWORK_RULE_ACTION_WATCH:
                    case ZT_NETWORK_RULE_ACTION_REDIRECT: {
                        const Address fwdAddr(rules[rn].v.fwd.address);
                        if (fwdAddr == ztSource) {
                            // Skip as no-op since source is target
                        }
                        else if (fwdAddr == ctx.identity.address()) {
                            if (inbound) {
                                return DOZTFILTER_SUPER_ACCEPT;
                            }
                            else {
                            }
                        }
                        else if (fwdAddr == ztDest) {
                        }
                        else {
                            if (rt == ZT_NETWORK_RULE_ACTION_REDIRECT) {
                                ztDest = fwdAddr;
                                return DOZTFILTER_REDIRECT;
                            }
                            else {
                                cc       = fwdAddr;
                                ccLength = (rules[rn].v.fwd.length != 0)
                                               ? ((frameLen < (unsigned int)rules[rn].v.fwd.length)
                                                      ? frameLen
                                                      : (unsigned int)rules[rn].v.fwd.length)
                                               : frameLen;
                                ccWatch  = (rt == ZT_NETWORK_RULE_ACTION_WATCH);
                            }
                        }
                    }
                        continue;

                    case ZT_NETWORK_RULE_ACTION_BREAK:
                        return DOZTFILTER_NO_MATCH;

                        // Unrecognized ACTIONs are ignored as no-ops
                    default: continue;
                }
            }
            else {
                // If this is an incoming packet and we are a TEE or REDIRECT target, we should
                // super-accept if we accept at all. This will cause us to accept redirected or
                // tee'd packets in spite of MAC and ZT addressing checks.
                if (inbound) {
                    switch (rt) {
                        case ZT_NETWORK_RULE_ACTION_TEE:
                        case ZT_NETWORK_RULE_ACTION_WATCH:
                        case ZT_NETWORK_RULE_ACTION_REDIRECT:
                            if (ctx.identity.address().toInt() == rules[rn].v.fwd.address)
                                superAccept = true;
                            break;
                        default: break;
                    }
                }

                thisSetMatches = 1;   // reset to default true for next batch of entries
                continue;
            }
        }

        // Circuit breaker: no need to evaluate an AND if the set's match state
        // is currently false since anything AND false is false.
        if ((!thisSetMatches) && (!(rules[rn].t & 0x40U))) {
            rrl.logSkipped(rn, thisSetMatches);
            continue;
        }

        // If this was not an ACTION evaluate next MATCH and update thisSetMatches with (AND [result])
        uint8_t thisRuleMatches = 0;
        uint64_t ownershipVerificationMask =
            1;   // this magic value means it hasn't been computed yet -- this is done lazily the first time it's needed
        switch (rt) {
            case ZT_NETWORK_RULE_MATCH_SOURCE_ZEROTIER_ADDRESS:
                thisRuleMatches = (uint8_t)(rules[rn].v.zt == ztSource.toInt());
                break;
            case ZT_NETWORK_RULE_MATCH_DEST_ZEROTIER_ADDRESS:
                thisRuleMatches = (uint8_t)(rules[rn].v.zt == ztDest.toInt());
                break;
            case ZT_NETWORK_RULE_MATCH_VLAN_ID:
                thisRuleMatches = (uint8_t)(rules[rn].v.vlanId == (uint16_t)vlanId);
                break;
            case ZT_NETWORK_RULE_MATCH_VLAN_PCP:
                // NOT SUPPORTED YET
                thisRuleMatches = (uint8_t)(rules[rn].v.vlanPcp == 0);
                break;
            case ZT_NETWORK_RULE_MATCH_VLAN_DEI:
                // NOT SUPPORTED YET
                thisRuleMatches = (uint8_t)(rules[rn].v.vlanDei == 0);
                break;
            case ZT_NETWORK_RULE_MATCH_MAC_SOURCE:
                thisRuleMatches = (uint8_t)(MAC(rules[rn].v.mac) == macSource);
                break;
            case ZT_NETWORK_RULE_MATCH_MAC_DEST: thisRuleMatches = (uint8_t)(MAC(rules[rn].v.mac) == macDest); break;
            case ZT_NETWORK_RULE_MATCH_IPV4_SOURCE:
                if ((etherType == ZT_ETHERTYPE_IPV4) && (frameLen >= 20)) {
                    thisRuleMatches =
                        (uint8_t)(InetAddress((const void *)&(rules[rn].v.ipv4.ip), 4, rules[rn].v.ipv4.mask)
                                      .containsAddress(InetAddress((const void *)(frameData + 12), 4, 0)));
                }
                else {
                    thisRuleMatches = 0;
                }
                break;
            case ZT_NETWORK_RULE_MATCH_IPV4_DEST:
                if ((etherType == ZT_ETHERTYPE_IPV4) && (frameLen >= 20)) {
                    thisRuleMatches =
                        (uint8_t)(InetAddress((const void *)&(rules[rn].v.ipv4.ip), 4, rules[rn].v.ipv4.mask)
                                      .containsAddress(InetAddress((const void *)(frameData + 16), 4, 0)));
                }
                else {
                    thisRuleMatches = 0;
                }
                break;
            case ZT_NETWORK_RULE_MATCH_IPV6_SOURCE:
                if ((etherType == ZT_ETHERTYPE_IPV6) && (frameLen >= 40)) {
                    thisRuleMatches =
                        (uint8_t)(InetAddress((const void *)rules[rn].v.ipv6.ip, 16, rules[rn].v.ipv6.mask)
                                      .containsAddress(InetAddress((const void *)(frameData + 8), 16, 0)));
                }
                else {
                    thisRuleMatches = 0;
                }
                break;
            case ZT_NETWORK_RULE_MATCH_IPV6_DEST:
                if ((etherType == ZT_ETHERTYPE_IPV6) && (frameLen >= 40)) {
                    thisRuleMatches =
                        (uint8_t)(InetAddress((const void *)rules[rn].v.ipv6.ip, 16, rules[rn].v.ipv6.mask)
                                      .containsAddress(InetAddress((const void *)(frameData + 24), 16, 0)));
                }
                else {
                    thisRuleMatches = 0;
                }
                break;
            case ZT_NETWORK_RULE_MATCH_IP_TOS:
                if ((etherType == ZT_ETHERTYPE_IPV4) && (frameLen >= 20)) {
                    const uint8_t tosMasked = frameData[1] & rules[rn].v.ipTos.mask;
                    thisRuleMatches =
                        (uint8_t)((tosMasked >= rules[rn].v.ipTos.value[0]) && (tosMasked <= rules[rn].v.ipTos.value[1]));
                }
                else if ((etherType == ZT_ETHERTYPE_IPV6) && (frameLen >= 40)) {
                    const uint8_t tosMasked =
                        (((frameData[0] << 4U) & 0xf0U) | ((frameData[1] >> 4U) & 0x0fU)) & rules[rn].v.ipTos.mask;
                    thisRuleMatches =
                        (uint8_t)((tosMasked >= rules[rn].v.ipTos.value[0]) && (tosMasked <= rules[rn].v.ipTos.value[1]));
                }
                else {
                    thisRuleMatches = 0;
                }
                break;
            case ZT_NETWORK_RULE_MATCH_IP_PROTOCOL:
                if ((etherType == ZT_ETHERTYPE_IPV4) && (frameLen >= 20)) {
                    thisRuleMatches = (uint8_t)(rules[rn].v.ipProtocol == frameData[9]);
                }
                else if (etherType == ZT_ETHERTYPE_IPV6) {
                    unsigned int pos = 0, proto = 0;
                    if (_ipv6GetPayload(frameData, frameLen, pos, proto)) {
                        thisRuleMatches = (uint8_t)(rules[rn].v.ipProtocol == (uint8_t)proto);
                    }
                    else {
                        thisRuleMatches = 0;
                    }
                }
                else {
                    thisRuleMatches = 0;
                }
                break;
            case ZT_NETWORK_RULE_MATCH_ETHERTYPE:
                thisRuleMatches = (uint8_t)(rules[rn].v.etherType == (uint16_t)etherType);
                break;
            case ZT_NETWORK_RULE_MATCH_ICMP:
                if ((etherType == ZT_ETHERTYPE_IPV4) && (frameLen >= 20)) {
                    if (frameData[9] == 0x01) {   // IP protocol == ICMP
                        const unsigned int ihl = (frameData[0] & 0xfU) * 4;
                        if (frameLen >= (ihl + 2)) {
                            if (rules[rn].v.icmp.type == frameData[ihl]) {
                                if ((rules[rn].v.icmp.flags & 0x01) != 0) {
                                    thisRuleMatches = (uint8_t)(frameData[ihl + 1] == rules[rn].v.icmp.code);
                                }
                                else {
                                    thisRuleMatches = 1;
                                }
                            }
                            else {
                                thisRuleMatches = 0;
                            }
                        }
                        else {
                            thisRuleMatches = 0;
                        }
                    }
                    else {
                        thisRuleMatches = 0;
                    }
                }
                else if (etherType == ZT_ETHERTYPE_IPV6) {
                    unsigned int pos = 0, proto = 0;
                    if (_ipv6GetPayload(frameData, frameLen, pos, proto)) {
                        if ((proto == 0x3a) && (frameLen >= (pos + 2))) {
                            if (rules[rn].v.icmp.type == frameData[pos]) {
                                if ((rules[rn].v.icmp.flags & 0x01) != 0) {
                                    thisRuleMatches = (uint8_t)(frameData[pos + 1] == rules[rn].v.icmp.code);
                                }
                                else {
                                    thisRuleMatches = 1;
                                }
                            }
                            else {
                                thisRuleMatches = 0;
                            }
                        }
                        else {
                            thisRuleMatches = 0;
                        }
                    }
                    else {
                        thisRuleMatches = 0;
                    }
                }
                else {
                    thisRuleMatches = 0;
                }
                break;
            case ZT_NETWORK_RULE_MATCH_IP_SOURCE_PORT_RANGE:
            case ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE:
                if ((etherType == ZT_ETHERTYPE_IPV4) && (frameLen >= 20)) {
                    const unsigned int headerLen = 4 * (frameData[0] & 0xfU);
                    int p                        = -1;
                    switch (frameData[9]) {   // IP protocol number
                        // All these start with 16-bit source and destination port in that order
                        case 0x06:   // TCP
                        case 0x11:   // UDP
                        case 0x84:   // SCTP
                        case 0x88:   // UDPLite
                            if (frameLen > (headerLen + 4)) {
                                unsigned int pos =
                                    headerLen + ((rt == ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE) ? 2 : 0);
                                p = (int)(frameData[pos++] << 8U);
                                p |= (int)frameData[pos];
                            }
                            break;
                    }

                    thisRuleMatches =
                        (p >= 0) ? (uint8_t)((p >= (int)rules[rn].v.port[0]) && (p <= (int)rules[rn].v.port[1]))
                                 : (uint8_t)0;
                }
                else if (etherType == ZT_ETHERTYPE_IPV6) {
                    unsigned int pos = 0, proto = 0;
                    if (_ipv6GetPayload(frameData, frameLen, pos, proto)) {
                        int p = -1;
                        switch (proto) {   // IP protocol number
                            // All these start with 16-bit source and destination port in that order
                            case 0x06:   // TCP
                            case 0x11:   // UDP
                            case 0x84:   // SCTP
                            case 0x88:   // UDPLite
                                if (frameLen > (pos + 4)) {
                                    if (rt == ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE)
                                        pos += 2;
                                    p = (int)(frameData[pos++] << 8U);
                                    p |= (int)frameData[pos];
                                }
                                break;
                        }
                        thisRuleMatches =
                            (p > 0) ? (uint8_t)((p >= (int)rules[rn].v.port[0]) && (p <= (int)rules[rn].v.port[1]))
                                    : (uint8_t)0;
                    }
                    else {
                        thisRuleMatches = 0;
                    }
                }
                else {
                    thisRuleMatches = 0;
                }
                break;
            case ZT_NETWORK_RULE_MATCH_CHARACTERISTICS: {
                uint64_t cf = (inbound) ? ZT_RULE_PACKET_CHARACTERISTICS_INBOUND : 0ULL;
                if (macDest.isMulticast())
                    cf |= ZT_RULE_PACKET_CHARACTERISTICS_MULTICAST;
                if (macDest.isBroadcast())
                    cf |= ZT_RULE_PACKET_CHARACTERISTICS_BROADCAST;
                if (ownershipVerificationMask == 1) {
                    ownershipVerificationMask = 0;
                    InetAddress src;
                    if ((etherType == ZT_ETHERTYPE_IPV4) && (frameLen >= 20)) {
                        src.set((const void *)(frameData + 12), 4, 0);
                    }
                    else if ((etherType == ZT_ETHERTYPE_IPV6) && (frameLen >= 40)) {
                        // IPv6 NDP requires special handling, since the src and dest IPs in the packet are empty or
                        // link-local.
                        if ((frameLen >= (40 + 8 + 16)) && (frameData[6] == 0x3a)
                            && ((frameData[40] == 0x87) || (frameData[40] == 0x88))) {
                            if (frameData[40] == 0x87) {
                                // Neighbor solicitations contain no reliable source address, so we implement a small
                                // hack by considering them authenticated. Otherwise you would pretty much have to do
                                // this manually in the rule set for IPv6 to work at all.
                                ownershipVerificationMask |= ZT_RULE_PACKET_CHARACTERISTICS_SENDER_IP_AUTHENTICATED;
                            }
                            else {
                                // Neighbor advertisements on the other hand can absolutely be authenticated.
                                src.set((const void *)(frameData + 40 + 8), 16, 0);
                            }
                        }
                        else {
                            // Other IPv6 packets can be handled normally
                            src.set((const void *)(frameData + 8), 16, 0);
                        }
                    }
                    else if ((etherType == ZT_ETHERTYPE_ARP) && (frameLen >= 28)) {
                        src.set((const void *)(frameData + 14), 4, 0);
                    }
                    if (inbound) {
                        if (membership) {
                            if ((src) && (membership->peerOwnsAddress<InetAddress>(nconf, src)))
                                ownershipVerificationMask |= ZT_RULE_PACKET_CHARACTERISTICS_SENDER_IP_AUTHENTICATED;
                            if (membership->peerOwnsAddress<MAC>(nconf, macSource))
                                ownershipVerificationMask |= ZT_RULE_PACKET_CHARACTERISTICS_SENDER_MAC_AUTHENTICATED;
                        }
                    }
                    else {
                        for (unsigned int i = 0; i < nconf.certificateOfOwnershipCount; ++i) {
                            if ((src) && (nconf.certificatesOfOwnership[i].owns(src)))
                                ownershipVerificationMask |= ZT_RULE_PACKET_CHARACTERISTICS_SENDER_IP_AUTHENTICATED;
                            if (nconf.certificatesOfOwnership[i].owns(macSource))
                                ownershipVerificationMask |= ZT_RULE_PACKET_CHARACTERISTICS_SENDER_MAC_AUTHENTICATED;
                        }
                    }
                }
                cf |= ownershipVerificationMask;
                if ((etherType == ZT_ETHERTYPE_IPV4) && (frameLen >= 20) && (frameData[9] == 0x06)) {
                    const unsigned int headerLen = 4 * (frameData[0] & 0xfU);
                    cf |= (uint64_t)frameData[headerLen + 13];
                    cf |= (((uint64_t)(frameData[headerLen + 12] & 0x0fU)) << 8U);
                }
                else if (etherType == ZT_ETHERTYPE_IPV6) {
                    unsigned int pos = 0, proto = 0;
                    if (_ipv6GetPayload(frameData, frameLen, pos, proto)) {
                        if ((proto == 0x06) && (frameLen > (pos + 14))) {
                            cf |= (uint64_t)frameData[pos + 13];
                            cf |= (((uint64_t)(frameData[pos + 12] & 0x0fU)) << 8U);
                        }
                    }
                }
                thisRuleMatches = (uint8_t)((cf & rules[rn].v.characteristics) != 0);
            } break;
            case ZT_NETWORK_RULE_MATCH_FRAME_SIZE_RANGE:
                thisRuleMatches = (uint8_t)((frameLen >= (unsigned int)rules[rn].v.frameSize[0]) && (frameLen <= (unsigned int)rules[rn].v.frameSize[1]));
                break;
            case ZT_NETWORK_RULE_MATCH_RANDOM:
                thisRuleMatches =
                    (uint8_t)((uint32_t)(Utils::random() & 0xffffffffULL) <= rules[rn].v.randomProbability);
                break;
            case ZT_NETWORK_RULE_MATCH_TAGS_DIFFERENCE:
            case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_AND:
            case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_OR:
            case ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_XOR:
            case ZT_NETWORK_RULE_MATCH_TAGS_EQUAL: {
                const TagCredential *const localTag = std::lower_bound(
                    &(nconf.tags[0]), &(nconf.tags[nconf.tagCount]), rules[rn].v.tag.id,
                    TagCredential::IdComparePredicate());
                if ((localTag != &(nconf.tags[nconf.tagCount])) && (localTag->id() == rules[rn].v.tag.id)) {
                    const TagCredential *const remoteTag =
                        ((membership) ? membership->getTag(nconf, rules[rn].v.tag.id) : (const TagCredential *)0);
                    if (remoteTag) {
                        const uint32_t ltv = localTag->value();
                        const uint32_t rtv = remoteTag->value();
                        if (rt == ZT_NETWORK_RULE_MATCH_TAGS_DIFFERENCE) {
                            const uint32_t diff = (ltv > rtv) ? (ltv - rtv) : (rtv - ltv);
                            thisRuleMatches     = (uint8_t)(diff <= rules[rn].v.tag.value);
                        }
                        else if (rt == ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_AND) {
                            thisRuleMatches = (uint8_t)((ltv & rtv) == rules[rn].v.tag.value);
                        }
                        else if (rt == ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_OR) {
                            thisRuleMatches = (uint8_t)((ltv | rtv) == rules[rn].v.tag.value);
                        }
                        else if (rt == ZT_NETWORK_RULE_MATCH_TAGS_BITWISE_XOR) {
                            thisRuleMatches = (uint8_t)((ltv ^ rtv) == rules[rn].v.tag.value);
                        }
                        else if (rt == ZT_NETWORK_RULE_MATCH_TAGS_EQUAL) {
                            thisRuleMatches =
                                (uint8_t)((ltv == rules[rn].v.tag.value) && (rtv == rules[rn].v.tag.value));
                        }
                        else {   // sanity check, can't really happen
                            thisRuleMatches = 0;
                        }
                    }
                    else {
                        if ((inbound) && (!superAccept)) {
                            thisRuleMatches = 0;
                        }
                        else {
                            // Outbound side is not strict since if we have to match both tags and
                            // we are sending a first packet to a recipient, we probably do not know
                            // about their tags yet. They will filter on inbound and we will filter
                            // once we get their tag. If we are a tee/redirect target we are also
                            // not strict since we likely do not have these tags.
                            thisRuleMatches = 1;
                        }
                    }
                }
                else {
                    thisRuleMatches = 0;
                }
            } break;
            case ZT_NETWORK_RULE_MATCH_TAG_SENDER:
            case ZT_NETWORK_RULE_MATCH_TAG_RECEIVER: {
                if (superAccept) {
                    thisRuleMatches = 1;
                }
                else if (
                    ((rt == ZT_NETWORK_RULE_MATCH_TAG_SENDER) && (inbound))
                    || ((rt == ZT_NETWORK_RULE_MATCH_TAG_RECEIVER) && (!inbound))) {
                    const TagCredential *const remoteTag =
                        ((membership) ? membership->getTag(nconf, rules[rn].v.tag.id) : (const TagCredential *)0);
                    if (remoteTag) {
                        thisRuleMatches = (uint8_t)(remoteTag->value() == rules[rn].v.tag.value);
                    }
                    else {
                        if (rt == ZT_NETWORK_RULE_MATCH_TAG_RECEIVER) {
                            // If we are checking the receiver and this is an outbound packet, we
                            // can't be strict since we may not yet know the receiver's tag.
                            thisRuleMatches = 1;
                        }
                        else {
                            thisRuleMatches = 0;
                        }
                    }
                }
                else {   // sender and outbound or receiver and inbound
                    const TagCredential *const localTag = std::lower_bound(
                        &(nconf.tags[0]), &(nconf.tags[nconf.tagCount]), rules[rn].v.tag.id,
                        TagCredential::IdComparePredicate());
                    if ((localTag != &(nconf.tags[nconf.tagCount])) && (localTag->id() == rules[rn].v.tag.id)) {
                        thisRuleMatches = (uint8_t)(localTag->value() == rules[rn].v.tag.value);
                    }
                    else {
                        thisRuleMatches = 0;
                    }
                }
            } break;
            case ZT_NETWORK_RULE_MATCH_INTEGER_RANGE: {
                uint64_t integer         = 0;
                const unsigned int bits  = (rules[rn].v.intRange.format & 63U) + 1;
                const unsigned int bytes = ((bits + 8 - 1) / 8);   // integer ceiling of division by 8
                if ((rules[rn].v.intRange.format & 0x80U) == 0) {
                    // Big-endian
                    unsigned int idx       = rules[rn].v.intRange.idx + (8 - bytes);
                    const unsigned int eof = idx + bytes;
                    if (eof <= frameLen) {
                        while (idx < eof) {
                            integer <<= 8U;
                            integer |= frameData[idx++];
                        }
                    }
                    integer &= 0xffffffffffffffffULL >> (64 - bits);
                }
                else {
                    // Little-endian
                    unsigned int idx       = rules[rn].v.intRange.idx;
                    const unsigned int eof = idx + bytes;
                    if (eof <= frameLen) {
                        while (idx < eof) {
                            integer >>= 8U;
                            integer |= ((uint64_t)frameData[idx++]) << 56U;
                        }
                    }
                    integer >>= (64 - bits);
                }
                thisRuleMatches = (uint8_t)((integer >= rules[rn].v.intRange.start) && (integer <= (rules[rn].v.intRange.start + (uint64_t)rules[rn].v.intRange.end)));
            } break;

                // The result of an unsupported MATCH is configurable at the network
                // level via a flag.
            default:
                thisRuleMatches =
                    (uint8_t)((nconf.flags & ZT_NETWORKCONFIG_FLAG_RULES_RESULT_OF_UNSUPPORTED_MATCH) != 0);
                break;
        }

        rrl.log(rn, thisRuleMatches, thisSetMatches);

        if ((rules[rn].t & 0x40U))
            thisSetMatches |= (thisRuleMatches ^ ((rules[rn].t >> 7U) & 1U));
        else
            thisSetMatches &= (thisRuleMatches ^ ((rules[rn].t >> 7U) & 1U));
    }

    return DOZTFILTER_NO_MATCH;
}

}   // anonymous namespace

const ZeroTier::MulticastGroup Network::BROADCAST(ZeroTier::MAC(0xffffffffffffULL), 0);

Network::Network(
    const Context &ctx, const CallContext &cc, uint64_t nwid, const Fingerprint &controllerFingerprint, void *uptr,
    const NetworkConfig *nconf)
    : m_ctx(ctx)
    , m_uPtr(uptr)
    , m_id(nwid)
    , m_mac(ctx.identity.address(), nwid)
    , m_portInitialized(false)
    , m_destroyed(false)
    , m_lastConfigUpdate(0)
    , _netconfFailure(NETCONF_FAILURE_NONE)
{
    if (controllerFingerprint)
        m_controllerFingerprint = controllerFingerprint;

    if (nconf) {
        this->setConfiguration(cc, *nconf, false);
        m_lastConfigUpdate = 0;   // still want to re-request since it's likely outdated
    }
    else {
        uint64_t tmp[2];
        tmp[0] = nwid;
        tmp[1] = 0;

        bool got = false;
        try {
            Dictionary dict;
            Vector<uint8_t> nconfData(m_ctx.store->get(cc, ZT_STATE_OBJECT_NETWORK_CONFIG, tmp, 1));
            if (nconfData.size() > 2) {
                nconfData.push_back(0);
                if (dict.decode(nconfData.data(), (unsigned int)nconfData.size())) {
                    try {
                        ScopedPtr<NetworkConfig> nconf2(new NetworkConfig());
                        if (nconf2->fromDictionary(dict)) {
                            this->setConfiguration(cc, *nconf2, false);
                            m_lastConfigUpdate = 0;   // still want to re-request an update since it's likely outdated
                            got                = true;
                        }
                    }
                    catch (...) {
                    }
                }
            }
        }
        catch (...) {
        }

        if (!got)
            m_ctx.store->put(cc, ZT_STATE_OBJECT_NETWORK_CONFIG, tmp, 1, "\n", 1);
    }

    if (!m_portInitialized) {
        ZT_VirtualNetworkConfig ctmp;
        m_externalConfig(&ctmp);
        m_ctx.cb.virtualNetworkConfigFunction(
            reinterpret_cast<ZT_Node *>(m_ctx.node), m_ctx.uPtr, cc.tPtr, m_id, &m_uPtr,
            ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_UP, &ctmp);
        m_portInitialized = true;
    }
}

Network::~Network()
{
    m_memberships_l.lock();
    m_config_l.lock();
    m_config_l.unlock();
    m_memberships_l.unlock();

    ZT_VirtualNetworkConfig ctmp;
    m_externalConfig(&ctmp);

    if (m_destroyed) {
        // This is done in Node::leave() so we can pass tPtr properly
        // m_ctx.node->configureVirtualNetworkPort((void
        // *)0,_id,&_uPtr,ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY,&ctmp);
    }
    else {
        m_ctx.cb.virtualNetworkConfigFunction(
            reinterpret_cast<ZT_Node *>(m_ctx.node), m_ctx.uPtr, nullptr, m_id, &m_uPtr,
            ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_DOWN, &ctmp);
    }
}

bool Network::filterOutgoingPacket(
    const CallContext &cc, const bool noTee, const Address &ztSource, const Address &ztDest, const MAC &macSource,
    const MAC &macDest, const uint8_t *frameData, const unsigned int frameLen, const unsigned int etherType,
    const unsigned int vlanId, uint8_t &qosBucket)
{
    Trace::RuleResultLog rrl, crrl;
    Address ztFinalDest(ztDest);
    int localCapabilityIndex = -1;
    int accept               = 0;
    Address ccNodeAddress;
    unsigned int ccLength = 0;
    bool ccWatch          = false;

    Mutex::Lock l1(m_memberships_l);
    Mutex::Lock l2(m_config_l);

    Member *membership;
    if (ztDest) {
        Map<Address, Member>::iterator mm(m_memberships.find(ztDest));
        membership = (mm == m_memberships.end()) ? nullptr : &(mm->second);
    }
    else {
        membership = nullptr;
    }

    switch (_doZtFilter(
        m_ctx, rrl, m_config, membership, false, ztSource, ztFinalDest, macSource, macDest, frameData, frameLen,
        etherType, vlanId, m_config.rules, m_config.ruleCount, ccNodeAddress, ccLength, ccWatch, qosBucket)) {
        case DOZTFILTER_NO_MATCH: {
            for (unsigned int c = 0; c < m_config.capabilityCount; ++c) {
                ztFinalDest = ztDest;   // sanity check, shouldn't be possible if there was no match
                Address cc2;
                unsigned int ccLength2 = 0;
                bool ccWatch2          = false;
                switch (_doZtFilter(
                    m_ctx, crrl, m_config, membership, false, ztSource, ztFinalDest, macSource, macDest, frameData,
                    frameLen, etherType, vlanId, m_config.capabilities[c].rules(), m_config.capabilities[c].ruleCount(),
                    cc2, ccLength2, ccWatch2, qosBucket)) {
                    case DOZTFILTER_NO_MATCH:
                    case DOZTFILTER_DROP:   // explicit DROP in a capability just terminates its evaluation and is an
                                            // anti-pattern
                        break;

                    case DOZTFILTER_REDIRECT:   // interpreted as ACCEPT but ztFinalDest will have been changed in
                                                // _doZtFilter()
                    case DOZTFILTER_ACCEPT:
                    case DOZTFILTER_SUPER_ACCEPT:   // no difference in behavior on outbound side in capabilities
                        localCapabilityIndex = (int)c;
                        accept               = 1;

                        if ((!noTee) && (cc2)) {
                            // TODO
                            /*
                            Packet outp(cc2,m_ctx.identity.address(),Packet::VERB_EXT_FRAME);
                            outp.append(_id);
                            outp.append((uint8_t)(ccWatch2 ? 0x16 : 0x02));
                            macDest.appendTo(outp);
                            macSource.appendTo(outp);
                            outp.append((uint16_t)etherType);
                            outp.append(frameData,ccLength2);
                            outp.compress();
                            m_ctx.sw->send(tPtr,outp,true);
                            */
                        }

                        break;
                }
                if (accept)
                    break;
            }
        } break;

        case DOZTFILTER_DROP:
            m_ctx.t->networkFilter(
                cc, 0xadea5a2a, m_id, rrl.l, nullptr, 0, 0, ztSource, ztDest, macSource, macDest, (uint16_t)frameLen,
                frameData, (uint16_t)etherType, (uint16_t)vlanId, noTee, false, 0);
            return false;

        case DOZTFILTER_REDIRECT:   // interpreted as ACCEPT but ztFinalDest will have been changed in _doZtFilter()
        case DOZTFILTER_ACCEPT: accept = 1; break;

        case DOZTFILTER_SUPER_ACCEPT: accept = 2; break;
    }

    if (accept != 0) {
        if ((!noTee) && (ccNodeAddress)) {
            // TODO
            /*
            Packet outp(cc,m_ctx.identity.address(),Packet::VERB_EXT_FRAME);
            outp.append(_id);
            outp.append((uint8_t)(ccWatch ? 0x16 : 0x02));
            macDest.appendTo(outp);
            macSource.appendTo(outp);
            outp.append((uint16_t)etherType);
            outp.append(frameData,ccLength);
            outp.compress();
            m_ctx.sw->send(tPtr,outp,true);
            */
        }

        if ((ztDest != ztFinalDest) && (ztFinalDest)) {
            // TODO
            /*
            Packet outp(ztFinalDest,m_ctx.identity.address(),Packet::VERB_EXT_FRAME);
            outp.append(_id);
            outp.append((uint8_t)0x04);
            macDest.appendTo(outp);
            macSource.appendTo(outp);
            outp.append((uint16_t)etherType);
            outp.append(frameData,frameLen);
            outp.compress();
            m_ctx.sw->send(tPtr,outp,true);
            */

            // DROP locally since we redirected
            accept = 0;
        }
    }

    if (localCapabilityIndex >= 0) {
        const CapabilityCredential &cap = m_config.capabilities[localCapabilityIndex];
        m_ctx.t->networkFilter(
            cc, 0x56ff1a93, m_id, rrl.l, crrl.l, cap.id(), cap.timestamp(), ztSource, ztDest, macSource, macDest,
            (uint16_t)frameLen, frameData, (uint16_t)etherType, (uint16_t)vlanId, noTee, false, accept);
    }
    else {
        m_ctx.t->networkFilter(
            cc, 0x112fbbab, m_id, rrl.l, nullptr, 0, 0, ztSource, ztDest, macSource, macDest, (uint16_t)frameLen,
            frameData, (uint16_t)etherType, (uint16_t)vlanId, noTee, false, accept);
    }

    return (accept != 0);
}

int Network::filterIncomingPacket(
    const CallContext &cc, const SharedPtr<Peer> &sourcePeer, const Address &ztDest, const MAC &macSource,
    const MAC &macDest, const uint8_t *frameData, const unsigned int frameLen, const unsigned int etherType,
    const unsigned int vlanId)
{
    Address ztFinalDest(ztDest);
    Trace::RuleResultLog rrl, crrl;
    int accept = 0;
    Address ccNodeAddress;
    unsigned int ccLength         = 0;
    bool ccWatch                  = false;
    const CapabilityCredential *c = nullptr;

    uint8_t qosBucket = 255;   // For incoming packets this is a dummy value

    Mutex::Lock l1(m_memberships_l);
    Mutex::Lock l2(m_config_l);

    Member &membership = m_memberships[sourcePeer->address()];

    switch (_doZtFilter(
        m_ctx, rrl, m_config, &membership, true, sourcePeer->address(), ztFinalDest, macSource, macDest, frameData,
        frameLen, etherType, vlanId, m_config.rules, m_config.ruleCount, ccNodeAddress, ccLength, ccWatch, qosBucket)) {
        case DOZTFILTER_NO_MATCH: {
            Member::CapabilityIterator mci(membership, m_config);
            while ((c = mci.next())) {
                ztFinalDest = ztDest;   // sanity check, should be unmodified if there was no match
                Address cc2;
                unsigned int ccLength2 = 0;
                bool ccWatch2          = false;
                switch (_doZtFilter(
                    m_ctx, crrl, m_config, &membership, true, sourcePeer->address(), ztFinalDest, macSource, macDest,
                    frameData, frameLen, etherType, vlanId, c->rules(), c->ruleCount(), cc2, ccLength2, ccWatch2,
                    qosBucket)) {
                    case DOZTFILTER_NO_MATCH:
                    case DOZTFILTER_DROP:   // explicit DROP in a capability just terminates its evaluation and is an
                                            // anti-pattern
                        break;
                    case DOZTFILTER_REDIRECT:   // interpreted as ACCEPT but ztDest will have been changed in
                                                // _doZtFilter()
                    case DOZTFILTER_ACCEPT:
                        accept = 1;   // ACCEPT
                        break;
                    case DOZTFILTER_SUPER_ACCEPT:
                        accept = 2;   // super-ACCEPT
                        break;
                }

                if (accept) {
                    if (cc2) {
                        // TODO
                        /*
                        Packet outp(cc2,m_ctx.identity.address(),Packet::VERB_EXT_FRAME);
                        outp.append(_id);
                        outp.append((uint8_t)(ccWatch2 ? 0x1c : 0x08));
                        macDest.appendTo(outp);
                        macSource.appendTo(outp);
                        outp.append((uint16_t)etherType);
                        outp.append(frameData,ccLength2);
                        outp.compress();
                        m_ctx.sw->send(tPtr,outp,true);
                        */
                    }
                    break;
                }
            }
        } break;

        case DOZTFILTER_DROP:
            // if (_config.remoteTraceTarget)
            //	m_ctx.t->networkFilter(tPtr,*this,rrl,(Trace::RuleResultLog *)0,(Capability
            //*)0,sourcePeer->address(),ztDest,macSource,macDest,frameData,frameLen,etherType,vlanId,false,true,0);
            return 0;   // DROP

        case DOZTFILTER_REDIRECT:   // interpreted as ACCEPT but ztFinalDest will have been changed in _doZtFilter()
        case DOZTFILTER_ACCEPT:
            accept = 1;   // ACCEPT
            break;
        case DOZTFILTER_SUPER_ACCEPT:
            accept = 2;   // super-ACCEPT
            break;
    }

    if (accept) {
        if (ccNodeAddress) {
            // TODO
            /*
            Packet outp(cc,m_ctx.identity.address(),Packet::VERB_EXT_FRAME);
            outp.append(_id);
            outp.append((uint8_t)(ccWatch ? 0x1c : 0x08));
            macDest.appendTo(outp);
            macSource.appendTo(outp);
            outp.append((uint16_t)etherType);
            outp.append(frameData,ccLength);
            outp.compress();
            m_ctx.sw->send(tPtr,outp,true);
            */
        }

        if ((ztDest != ztFinalDest) && (ztFinalDest)) {
            // TODO
            /*
            Packet outp(ztFinalDest,m_ctx.identity.address(),Packet::VERB_EXT_FRAME);
            outp.append(_id);
            outp.append((uint8_t)0x0a);
            macDest.appendTo(outp);
            macSource.appendTo(outp);
            outp.append((uint16_t)etherType);
            outp.append(frameData,frameLen);
            outp.compress();
            m_ctx.sw->send(tPtr,outp,true);
            */

            // if (_config.remoteTraceTarget)
            //	m_ctx.t->networkFilter(tPtr,*this,rrl,(c) ? &crrl : (Trace::RuleResultLog
            //*)0,c,sourcePeer->address(),ztDest,macSource,macDest,frameData,frameLen,etherType,vlanId,false,true,0);
            return 0;   // DROP locally, since we redirected
        }
    }

    // if (_config.remoteTraceTarget)
    //	m_ctx.t->networkFilter(tPtr,*this,rrl,(c) ? &crrl : (Trace::RuleResultLog
    //*)0,c,sourcePeer->address(),ztDest,macSource,macDest,frameData,frameLen,etherType,vlanId,false,true,accept);
    return accept;
}

void Network::multicastSubscribe(const CallContext &cc, const MulticastGroup &mg)
{
    Mutex::Lock l(m_myMulticastGroups_l);
    if (!std::binary_search(m_myMulticastGroups.begin(), m_myMulticastGroups.end(), mg)) {
        m_myMulticastGroups.insert(std::upper_bound(m_myMulticastGroups.begin(), m_myMulticastGroups.end(), mg), mg);
        Mutex::Lock l2(m_memberships_l);
        m_announceMulticastGroups(cc.tPtr, true);
    }
}

void Network::multicastUnsubscribe(const MulticastGroup &mg)
{
    Mutex::Lock l(m_myMulticastGroups_l);
    Vector<MulticastGroup>::iterator i(std::lower_bound(m_myMulticastGroups.begin(), m_myMulticastGroups.end(), mg));
    if ((i != m_myMulticastGroups.end()) && (*i == mg))
        m_myMulticastGroups.erase(i);
}

uint64_t Network::handleConfigChunk(
    const CallContext &cc, uint64_t packetId, const SharedPtr<Peer> &source, const Buf &chunk, int ptr, int size)
{
    // If the controller's full fingerprint is known or was explicitly specified on join(),
    // require that the controller's identity match. Otherwise learn it.
    if (m_controllerFingerprint) {
        if (source->identity().fingerprint() != m_controllerFingerprint)
            return 0;
    }
    else {
        m_controllerFingerprint = source->identity().fingerprint();
    }

    return 0;
#if 0
	if (_destroyed)
		return 0;

	const unsigned int chunkPayloadStart = ptr;
	ptr += 8; // skip network ID, which is already obviously known
	const unsigned int chunkLen = chunk.rI16(ptr);
	const uint8_t *chunkData = chunk.rBnc(ptr,chunkLen);
	if (Buf<>::readOverflow(ptr,size))
		return 0;

	Mutex::Lock l1(_config_l);

	_IncomingConfigChunk *c = nullptr;
	uint64_t configUpdateId;
	int totalLength = 0,chunkIndex = 0;
	if (ptr < size) {
		// If there is more data after the chunk / dictionary, it means this is a new controller
		// that sends signed chunks. We still support really old controllers, but probably not forever.
		const bool fastPropagate = ((chunk.rI8(ptr) & Protocol::NETWORK_CONFIG_FLAG_FAST_PROPAGATE) != 0);
		configUpdateId = chunk.rI64(ptr);
		totalLength = chunk.rI32(ptr);
		chunkIndex = chunk.rI32(ptr);
		++ptr; // skip unused signature type field
		const unsigned int signatureSize = chunk.rI16(ptr);
		const uint8_t *signature = chunk.rBnc(ptr,signatureSize);
		if ((Buf<>::readOverflow(ptr,size))||((chunkIndex + chunkLen) > totalLength)||(totalLength >= ZT_MAX_NETWORK_CONFIG_BYTES)||(signatureSize > ZT_SIGNATURE_BUFFER_SIZE)||(!signature))
			return 0;
		const unsigned int chunkPayloadSize = (unsigned int)ptr - chunkPayloadStart;

		// Find existing or new slot for this update and its chunk(s).
		for(int i=0;i<ZT_NETWORK_MAX_INCOMING_UPDATES;++i) {
			if (_incomingConfigChunks[i].updateId == configUpdateId) {
				c = &(_incomingConfigChunks[i]);
				if (c->chunks.find(chunkIndex) != c->chunks.end())
					return 0; // we already have this chunk!
				break;
			} else if ((!c)||(_incomingConfigChunks[i].touchCtr < c->touchCtr)) {
				c = &(_incomingConfigChunks[i]);
			}
		}
		if (!c) // sanity check; should not be possible
			return 0;

		// Verify this chunk's signature
		const SharedPtr<Peer> controllerPeer(m_ctx.topology->get(tPtr,controller()));
		if ((!controllerPeer)||(!controllerPeer->identity().verify(chunk.data.bytes + chunkPayloadStart,chunkPayloadSize,signature,signatureSize)))
			return 0;

		// New properly verified chunks can be flooded "virally" through the network via an aggressive
		// exponential rumor mill algorithm.
		if (fastPropagate) {
			Mutex::Lock l2(_memberships_l);
			Address *a = nullptr;
			Membership *m = nullptr;
			Hashtable<Address,Membership>::Iterator i(_memberships);
			while (i.next(a,m)) {
				if ((*a != source->address())&&(*a != controller())) {
					ZT_GET_NEW_BUF(outp,Protocol::Header);

					outp->data.fields.packetId = Protocol::getPacketId();
					a->copyTo(outp->data.fields.destination);
					m_ctx.identity.address().copyTo(outp->data.fields.source);
					outp->data.fields.flags = 0;
					outp->data.fields.verb = Protocol::VERB_NETWORK_CONFIG;

					int outl = sizeof(Protocol::Header);
					outp->wB(outl,chunk.data.bytes + chunkPayloadStart,chunkPayloadSize);

					if (Buf<>::writeOverflow(outl)) // sanity check... it fit before!
						break;

					m_ctx.sw->send(tPtr,outp,true);
				}
			}
		}
	} else if ((!source)||(source->address() != this->controller())) {
		// Legacy support for OK(NETWORK_CONFIG_REQUEST) from older controllers that don't sign chunks and don't
		// support multiple chunks. Since old controllers don't sign chunks we only accept the message if it comes
		// directly from the controller.
		configUpdateId = packetId;
		totalLength = (int)chunkLen;
		if (totalLength > ZT_MAX_NETWORK_CONFIG_BYTES)
			return 0;

		for(int i=0;i<ZT_NETWORK_MAX_INCOMING_UPDATES;++i) {
			if ((!c)||(_incomingConfigChunks[i].touchCtr < c->touchCtr))
				c = &(_incomingConfigChunks[i]);
		}
	} else {
		// Not signed, not from controller -> reject.
		return 0;
	}

	try {
		++c->touchCtr;
		if (c->updateId != configUpdateId) {
			c->updateId = configUpdateId;
			c->chunks.clear();
		}
		c->chunks[chunkIndex].assign(chunkData,chunkData + chunkLen);

		int haveLength = 0;
		for(std::map< int,Vector<uint8_t> >::const_iterator ch(c->chunks.begin());ch!=c->chunks.end();++ch)
			haveLength += (int)ch->second.size();
		if (haveLength > ZT_MAX_NETWORK_CONFIG_BYTES) {
			c->touchCtr = 0;
			c->updateId = 0;
			c->chunks.clear();
			return 0;
		}

		if (haveLength == totalLength) {
			Vector<uint8_t> assembledConfig;
			for(std::map< int,Vector<uint8_t> >::const_iterator ch(c->chunks.begin());ch!=c->chunks.end();++ch)
				assembledConfig.insert(assembledConfig.end(),ch->second.begin(),ch->second.end());

			Dictionary dict;
			if (dict.decode(assembledConfig.data(),(unsigned int)assembledConfig.size())) {
				ScopedPtr<NetworkConfig> nc(new NetworkConfig());
				if (nc->fromDictionary(dict)) {
					this->setConfiguration(tPtr,*nc,true);
					return configUpdateId;
				}
			}
		}
	} catch (...) {}

	return 0;
#endif
}

int Network::setConfiguration(const CallContext &cc, const NetworkConfig &nconf, bool saveToDisk)
{
    if (m_destroyed)
        return 0;

    // _lock is NOT locked when this is called
    try {
        if ((nconf.issuedTo != m_ctx.identity.address()) || (nconf.networkId != m_id))
            return 0;   // invalid config that is not for us or not for this network
        if ((!Utils::allZero(nconf.issuedToFingerprintHash, ZT_FINGERPRINT_HASH_SIZE))
            && (memcmp(nconf.issuedToFingerprintHash, m_ctx.identity.fingerprint().hash, ZT_FINGERPRINT_HASH_SIZE)
                != 0))
            return 0;   // full identity hash is present and does not match

        if (m_config == nconf)
            return 1;   // OK config, but duplicate of what we already have

        ZT_VirtualNetworkConfig ctmp;
        bool oldPortInitialized;
        {   // do things that require lock here, but unlock before calling callbacks
            Mutex::Lock l1(m_config_l);

            m_config           = nconf;
            m_lastConfigUpdate = cc.ticks;
            _netconfFailure    = NETCONF_FAILURE_NONE;

            oldPortInitialized = m_portInitialized;
            m_portInitialized  = true;

            m_externalConfig(&ctmp);
        }

        m_ctx.cb.virtualNetworkConfigFunction(
            reinterpret_cast<ZT_Node *>(m_ctx.node), m_ctx.uPtr, cc.tPtr, nconf.networkId, &m_uPtr,
            (oldPortInitialized) ? ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE
                                 : ZT_VIRTUAL_NETWORK_CONFIG_OPERATION_UP,
            &ctmp);

        if (saveToDisk) {
            try {
                Dictionary d;
                if (nconf.toDictionary(d)) {
                    uint64_t tmp[2];
                    tmp[0] = m_id;
                    tmp[1] = 0;
                    Vector<uint8_t> d2;
                    d.encode(d2);
                    m_ctx.store->put(cc, ZT_STATE_OBJECT_NETWORK_CONFIG, tmp, 1, d2.data(), (unsigned int)d2.size());
                }
            }
            catch (...) {
            }
        }

        return 2;   // OK and configuration has changed
    }
    catch (...) {
    }   // ignore invalid configs
    return 0;
}

bool Network::gate(void *tPtr, const SharedPtr<Peer> &peer) noexcept
{
    Mutex::Lock lc(m_config_l);

    if (!m_config)
        return false;
    if (m_config.isPublic())
        return true;

    try {
        Mutex::Lock l(m_memberships_l);
        return m_memberships[peer->address()].certificateOfMembershipAgress(m_config.com, peer->identity());
    }
    catch (...) {
    }

    return false;
}

void Network::doPeriodicTasks(const CallContext &cc)
{
    if (m_destroyed)
        return;

    if ((cc.ticks - m_lastConfigUpdate) >= ZT_NETWORK_AUTOCONF_DELAY)
        m_requestConfiguration(cc);

    {
        Mutex::Lock l1(m_memberships_l);

        for (Map<Address, Member>::iterator i(m_memberships.begin()); i != m_memberships.end(); ++i)
            i->second.clean(m_config);

        {
            Mutex::Lock l2(m_myMulticastGroups_l);

            // TODO
            /*
            Hashtable< MulticastGroup,uint64_t >::Iterator i(_multicastGroupsBehindMe);
            MulticastGroup *mg = (MulticastGroup *)0;
            uint64_t *ts = (uint64_t *)0;
            while (i.next(mg,ts)) {
                if ((now - *ts) > (ZT_MULTICAST_LIKE_EXPIRE * 2))
                    _multicastGroupsBehindMe.erase(*mg);
            }

            _announceMulticastGroups(tPtr,false);
            */
        }
    }
}

void Network::learnBridgeRoute(const MAC &mac, const Address &addr)
{
    Mutex::Lock _l(m_remoteBridgeRoutes_l);
    m_remoteBridgeRoutes[mac] = addr;

    // Anti-DOS circuit breaker to prevent nodes from spamming us with absurd numbers of bridge routes
    while (m_remoteBridgeRoutes.size() > ZT_MAX_BRIDGE_ROUTES) {
        Map<Address, unsigned long> counts;
        Address maxAddr;
        unsigned long maxCount = 0;

        // Find the address responsible for the most entries
        for (Map<MAC, Address>::iterator i(m_remoteBridgeRoutes.begin()); i != m_remoteBridgeRoutes.end(); ++i) {
            const unsigned long c = ++counts[i->second];
            if (c > maxCount) {
                maxCount = c;
                maxAddr  = i->second;
            }
        }

        // Kill this address from our table, since it's most likely spamming us
        for (Map<MAC, Address>::iterator i(m_remoteBridgeRoutes.begin()); i != m_remoteBridgeRoutes.end();) {
            if (i->second == maxAddr)
                m_remoteBridgeRoutes.erase(i++);
            else
                ++i;
        }
    }
}

Member::AddCredentialResult
Network::addCredential(const CallContext &cc, const Identity &sourcePeerIdentity, const MembershipCredential &com)
{
    if (com.networkId() != m_id)
        return Member::ADD_REJECTED;
    Mutex::Lock _l(m_memberships_l);
    return m_memberships[com.issuedTo().address].addCredential(m_ctx, cc, sourcePeerIdentity, m_config, com);
}

Member::AddCredentialResult
Network::addCredential(const CallContext &cc, const Identity &sourcePeerIdentity, const CapabilityCredential &cap)
{
    if (cap.networkId() != m_id)
        return Member::ADD_REJECTED;
    Mutex::Lock _l(m_memberships_l);
    return m_memberships[cap.issuedTo()].addCredential(m_ctx, cc, sourcePeerIdentity, m_config, cap);
}

Member::AddCredentialResult
Network::addCredential(const CallContext &cc, const Identity &sourcePeerIdentity, const TagCredential &tag)
{
    if (tag.networkId() != m_id)
        return Member::ADD_REJECTED;
    Mutex::Lock _l(m_memberships_l);
    return m_memberships[tag.issuedTo()].addCredential(m_ctx, cc, sourcePeerIdentity, m_config, tag);
}

Member::AddCredentialResult
Network::addCredential(const CallContext &cc, const Identity &sourcePeerIdentity, const RevocationCredential &rev)
{
    if (rev.networkId() != m_id)
        return Member::ADD_REJECTED;

    Mutex::Lock l1(m_memberships_l);
    Member &m = m_memberships[rev.target()];

    const Member::AddCredentialResult result = m.addCredential(m_ctx, cc, sourcePeerIdentity, m_config, rev);

    if ((result == Member::ADD_ACCEPTED_NEW) && (rev.fastPropagate())) {
        // TODO
        /*
            Address *a = nullptr;
            Membership *m = nullptr;
            Hashtable<Address,Membership>::Iterator i(_memberships);
            while (i.next(a,m)) {
                if ((*a != sourcePeerIdentity.address())&&(*a != rev.signer())) {
                    Packet outp(*a,m_ctx.identity.address(),Packet::VERB_NETWORK_CREDENTIALS);
                    outp.append((uint8_t)0x00); // no COM
                    outp.append((uint16_t)0); // no capabilities
                    outp.append((uint16_t)0); // no tags
                    outp.append((uint16_t)1); // one revocation!
                    rev.serialize(outp);
                    outp.append((uint16_t)0); // no certificates of ownership
                    m_ctx.sw->send(tPtr,outp,true);
                        }
                    }
        */
    }

    return result;
}

Member::AddCredentialResult
Network::addCredential(const CallContext &cc, const Identity &sourcePeerIdentity, const OwnershipCredential &coo)
{
    if (coo.networkId() != m_id)
        return Member::ADD_REJECTED;
    Mutex::Lock _l(m_memberships_l);
    return m_memberships[coo.issuedTo()].addCredential(m_ctx, cc, sourcePeerIdentity, m_config, coo);
}

void Network::pushCredentials(const CallContext &cc, const SharedPtr<Peer> &to)
{
    const int64_t tout = std::min(m_config.credentialTimeMaxDelta, m_config.com.timestampMaxDelta());
    Mutex::Lock _l(m_memberships_l);
    Member &m = m_memberships[to->address()];
    if (((cc.ticks - m.lastPushedCredentials()) + 5000) >= tout)
        m.pushCredentials(m_ctx, cc, to, m_config);
}

void Network::destroy()
{
    m_memberships_l.lock();
    m_config_l.lock();
    m_destroyed = true;
    m_config_l.unlock();
    m_memberships_l.unlock();
}

void Network::externalConfig(ZT_VirtualNetworkConfig *ec) const
{
    Mutex::Lock _l(m_config_l);
    m_externalConfig(ec);
}

void Network::m_requestConfiguration(const CallContext &cc)
{
    if (m_destroyed)
        return;

    if ((m_id >> 56U) == 0xff) {
        if ((m_id & 0xffffffU) == 0) {
            const uint16_t startPortRange = (uint16_t)((m_id >> 40U) & 0xffff);
            const uint16_t endPortRange   = (uint16_t)((m_id >> 24U) & 0xffff);
            if (endPortRange >= startPortRange) {
                ScopedPtr<NetworkConfig> nconf(new NetworkConfig());

                nconf->networkId              = m_id;
                nconf->timestamp              = (cc.clock < 0) ? cc.ticks : cc.clock;
                nconf->credentialTimeMaxDelta = ZT_NETWORKCONFIG_DEFAULT_CREDENTIAL_TIME_MAX_MAX_DELTA;
                nconf->revision               = 1;
                nconf->issuedTo               = m_ctx.identity.address();
                nconf->flags                  = ZT_NETWORKCONFIG_FLAG_ENABLE_IPV6_NDP_EMULATION;
                nconf->mtu                    = ZT_DEFAULT_MTU;
                nconf->multicastLimit         = 0;
                nconf->staticIpCount          = 1;
                nconf->ruleCount              = 14;
                nconf->staticIps[0]           = InetAddress::makeIpv66plane(m_id, m_ctx.identity.address().toInt());

                // Drop everything but IPv6
                nconf->rules[0].t           = (uint8_t)ZT_NETWORK_RULE_MATCH_ETHERTYPE | 0x80U;   // NOT
                nconf->rules[0].v.etherType = 0x86dd;                                             // IPv6
                nconf->rules[1].t           = (uint8_t)ZT_NETWORK_RULE_ACTION_DROP;

                // Allow ICMPv6
                nconf->rules[2].t            = (uint8_t)ZT_NETWORK_RULE_MATCH_IP_PROTOCOL;
                nconf->rules[2].v.ipProtocol = 0x3a;   // ICMPv6
                nconf->rules[3].t            = (uint8_t)ZT_NETWORK_RULE_ACTION_ACCEPT;

                // Allow destination ports within range
                nconf->rules[4].t            = (uint8_t)ZT_NETWORK_RULE_MATCH_IP_PROTOCOL;
                nconf->rules[4].v.ipProtocol = 0x11;                                                 // UDP
                nconf->rules[5].t            = (uint8_t)ZT_NETWORK_RULE_MATCH_IP_PROTOCOL | 0x40U;   // OR
                nconf->rules[5].v.ipProtocol = 0x06;                                                 // TCP
                nconf->rules[6].t            = (uint8_t)ZT_NETWORK_RULE_MATCH_IP_DEST_PORT_RANGE;
                nconf->rules[6].v.port[0]    = startPortRange;
                nconf->rules[6].v.port[1]    = endPortRange;
                nconf->rules[7].t            = (uint8_t)ZT_NETWORK_RULE_ACTION_ACCEPT;

                // Allow non-SYN TCP packets to permit non-connection-initiating traffic
                nconf->rules[8].t                 = (uint8_t)ZT_NETWORK_RULE_MATCH_CHARACTERISTICS | 0x80U;   // NOT
                nconf->rules[8].v.characteristics = ZT_RULE_PACKET_CHARACTERISTICS_TCP_SYN;
                nconf->rules[9].t                 = (uint8_t)ZT_NETWORK_RULE_ACTION_ACCEPT;

                // Also allow SYN+ACK which are replies to SYN
                nconf->rules[10].t                 = (uint8_t)ZT_NETWORK_RULE_MATCH_CHARACTERISTICS;
                nconf->rules[10].v.characteristics = ZT_RULE_PACKET_CHARACTERISTICS_TCP_SYN;
                nconf->rules[11].t                 = (uint8_t)ZT_NETWORK_RULE_MATCH_CHARACTERISTICS;
                nconf->rules[11].v.characteristics = ZT_RULE_PACKET_CHARACTERISTICS_TCP_ACK;
                nconf->rules[12].t                 = (uint8_t)ZT_NETWORK_RULE_ACTION_ACCEPT;

                nconf->rules[13].t = (uint8_t)ZT_NETWORK_RULE_ACTION_DROP;

                nconf->type = ZT_NETWORK_TYPE_PUBLIC;

                nconf->name[0] = 'a';
                nconf->name[1] = 'd';
                nconf->name[2] = 'h';
                nconf->name[3] = 'o';
                nconf->name[4] = 'c';
                nconf->name[5] = '-';
                Utils::hex((uint16_t)startPortRange, nconf->name + 6);
                nconf->name[10] = '-';
                Utils::hex((uint16_t)endPortRange, nconf->name + 11);
                nconf->name[15] = (char)0;

                this->setConfiguration(cc, *nconf, false);
            }
            else {
                this->setNotFound();
            }
        }
        else if ((m_id & 0xffU) == 0x01) {
            // ffAAaaaaaaaaaa01 -- where AA is the IPv4 /8 to use and aaaaaaaaaa is the anchor node for multicast gather
            // and replication
            const uint64_t myAddress  = m_ctx.identity.address().toInt();
            const uint64_t networkHub = (m_id >> 8U) & 0xffffffffffULL;

            uint8_t ipv4[4];
            ipv4[0] = (uint8_t)(m_id >> 48U);
            ipv4[1] = (uint8_t)(myAddress >> 16U);
            ipv4[2] = (uint8_t)(myAddress >> 8U);
            ipv4[3] = (uint8_t)myAddress;

            char v4ascii[24];
            Utils::decimal(ipv4[0], v4ascii);

            ScopedPtr<NetworkConfig> nconf(new NetworkConfig());

            nconf->networkId              = m_id;
            nconf->timestamp              = (cc.clock < 0) ? cc.ticks : cc.clock;
            nconf->credentialTimeMaxDelta = ZT_NETWORKCONFIG_DEFAULT_CREDENTIAL_TIME_MAX_MAX_DELTA;
            nconf->revision               = 1;
            nconf->issuedTo               = m_ctx.identity.address();
            nconf->flags                  = ZT_NETWORKCONFIG_FLAG_ENABLE_IPV6_NDP_EMULATION;
            nconf->mtu                    = ZT_DEFAULT_MTU;
            nconf->multicastLimit         = 1024;
            nconf->specialistCount        = (networkHub == 0) ? 0 : 1;
            nconf->staticIpCount          = 2;
            nconf->ruleCount              = 1;

            if (networkHub != 0)
                nconf->specialists[0] = networkHub;

            nconf->staticIps[0] = InetAddress::makeIpv66plane(m_id, myAddress);
            nconf->staticIps[1].set(ipv4, 4, 8);

            nconf->rules[0].t = (uint8_t)ZT_NETWORK_RULE_ACTION_ACCEPT;

            nconf->type = ZT_NETWORK_TYPE_PUBLIC;

            nconf->name[0]   = 'a';
            nconf->name[1]   = 'd';
            nconf->name[2]   = 'h';
            nconf->name[3]   = 'o';
            nconf->name[4]   = 'c';
            nconf->name[5]   = '-';
            unsigned long nn = 6;
            while ((nconf->name[nn] = v4ascii[nn - 6]))
                ++nn;
            nconf->name[nn++] = '.';
            nconf->name[nn++] = '0';
            nconf->name[nn++] = '.';
            nconf->name[nn++] = '0';
            nconf->name[nn++] = '.';
            nconf->name[nn++] = '0';
            nconf->name[nn]   = (char)0;

            this->setConfiguration(cc, *nconf, false);
        }
        return;
    }

    const Address ctrl(controller());

    Dictionary rmd;
    rmd.add(
        ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_VENDOR,
        (uint64_t)1);   // 1 == ZeroTier, no other vendors at the moment
    rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_PROTOCOL_VERSION, (uint64_t)ZT_PROTO_VERSION);
    rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_MAJOR_VERSION, (uint64_t)ZEROTIER_VERSION_MAJOR);
    rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_MINOR_VERSION, (uint64_t)ZEROTIER_VERSION_MINOR);
    rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_NODE_REVISION, (uint64_t)ZEROTIER_VERSION_REVISION);
    rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_MAX_NETWORK_RULES, (uint64_t)ZT_MAX_NETWORK_RULES);
    rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_MAX_NETWORK_CAPABILITIES, (uint64_t)ZT_MAX_NETWORK_CAPABILITIES);
    rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_MAX_CAPABILITY_RULES, (uint64_t)ZT_MAX_CAPABILITY_RULES);
    rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_MAX_NETWORK_TAGS, (uint64_t)ZT_MAX_NETWORK_TAGS);
    rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_FLAGS, (uint64_t)0);
    rmd.add(ZT_NETWORKCONFIG_REQUEST_METADATA_KEY_RULES_ENGINE_REV, (uint64_t)ZT_RULES_ENGINE_REVISION);

    m_ctx.t->networkConfigRequestSent(cc, 0x335bb1a2, m_id);

    if (ctrl == m_ctx.identity.address()) {
        if (m_ctx.localNetworkController) {
            m_ctx.localNetworkController->request(m_id, InetAddress(), 0xffffffffffffffffULL, m_ctx.identity, rmd);
        }
        else {
            this->setNotFound();
        }
        return;
    }

    // TODO
    /*
    Packet outp(ctrl,m_ctx.identity.address(),Packet::VERB_NETWORK_CONFIG_REQUEST);
    outp.append((uint64_t)_id);
    const unsigned int rmdSize = rmd->sizeBytes();
    outp.append((uint16_t)rmdSize);
    outp.append((const void *)rmd->data(),rmdSize);
    if (_config) {
        outp.append((uint64_t)_config.revision);
        outp.append((uint64_t)_config.timestamp);
    } else {
        outp.append((unsigned char)0,16);
    }
    outp.compress();
    m_ctx.node->expectReplyTo(outp.packetId());
    m_ctx.sw->send(tPtr,outp,true);
    */
}

ZT_VirtualNetworkStatus Network::m_status() const
{
    switch (_netconfFailure) {
        case NETCONF_FAILURE_ACCESS_DENIED: return ZT_NETWORK_STATUS_ACCESS_DENIED;
        case NETCONF_FAILURE_NOT_FOUND: return ZT_NETWORK_STATUS_NOT_FOUND;
        case NETCONF_FAILURE_NONE:
            return ((m_config) ? ZT_NETWORK_STATUS_OK : ZT_NETWORK_STATUS_REQUESTING_CONFIGURATION);
        default: return ZT_NETWORK_STATUS_REQUESTING_CONFIGURATION;
    }
}

void Network::m_externalConfig(ZT_VirtualNetworkConfig *ec) const
{
    // assumes _config_l is locked
    ec->nwid = m_id;
    ec->mac  = m_mac.toInt();
    if (m_config)
        Utils::scopy(ec->name, sizeof(ec->name), m_config.name);
    else
        ec->name[0] = (char)0;
    ec->status = m_status();
    ec->type   = (m_config) ? (m_config.isPrivate() ? ZT_NETWORK_TYPE_PRIVATE : ZT_NETWORK_TYPE_PUBLIC)
                            : ZT_NETWORK_TYPE_PRIVATE;
    ec->mtu    = (m_config) ? m_config.mtu : ZT_DEFAULT_MTU;
    Vector<Address> ab;
    for (unsigned int i = 0; i < m_config.specialistCount; ++i) {
        if ((m_config.specialists[i] & ZT_NETWORKCONFIG_SPECIALIST_TYPE_ACTIVE_BRIDGE) != 0)
            ab.push_back(Address(m_config.specialists[i]));
    }
    ec->bridge           = (std::find(ab.begin(), ab.end(), m_ctx.identity.address()) != ab.end()) ? 1 : 0;
    ec->broadcastEnabled = (m_config) ? (m_config.enableBroadcast() ? 1 : 0) : 0;
    ec->netconfRevision  = (m_config) ? (unsigned long)m_config.revision : 0;

    ec->assignedAddressCount = 0;
    for (unsigned int i = 0; i < ZT_MAX_ZT_ASSIGNED_ADDRESSES; ++i) {
        if (i < m_config.staticIpCount) {
            Utils::copy<sizeof(struct sockaddr_storage)>(&(ec->assignedAddresses[i]), &(m_config.staticIps[i]));
            ++ec->assignedAddressCount;
        }
        else {
            Utils::zero<sizeof(struct sockaddr_storage)>(&(ec->assignedAddresses[i]));
        }
    }

    ec->routeCount = 0;
    for (unsigned int i = 0; i < ZT_MAX_NETWORK_ROUTES; ++i) {
        if (i < m_config.routeCount) {
            Utils::copy<sizeof(ZT_VirtualNetworkRoute)>(&(ec->routes[i]), &(m_config.routes[i]));
            ++ec->routeCount;
        }
        else {
            Utils::zero<sizeof(ZT_VirtualNetworkRoute)>(&(ec->routes[i]));
        }
    }
}

void Network::m_announceMulticastGroups(void *tPtr, bool force)
{
    // Assumes _myMulticastGroups_l and _memberships_l are locked
    const Vector<MulticastGroup> groups(m_allMulticastGroups());
    m_announceMulticastGroupsTo(tPtr, controller(), groups);

    // TODO
    /*
Address *a = nullptr;
Membership *m = nullptr;
Hashtable<Address,Membership>::Iterator i(_memberships);
while (i.next(a,m)) {
    bool announce = m->multicastLikeGate(now); // force this to be called even if 'force' is true since it updates last
push time if ((!announce)&&(force)) announce = true; if ((announce)&&(m->isAllowedOnNetwork(_config)))
        _announceMulticastGroupsTo(tPtr,*a,groups);
    }
    */
}

void Network::m_announceMulticastGroupsTo(
    void *tPtr, const Address &peer, const Vector<MulticastGroup> &allMulticastGroups)
{
#if 0
	// Assumes _myMulticastGroups_l and _memberships_l are locked
	ScopedPtr<Packet> outp(new Packet(peer,m_ctx.identity.address(),Packet::VERB_MULTICAST_LIKE));

	for(Vector<MulticastGroup>::const_iterator mg(allMulticastGroups.begin());mg!=allMulticastGroups.end();++mg) {
		if ((outp->size() + 24) >= ZT_PROTO_MAX_PACKET_LENGTH) {
			outp->compress();
			m_ctx.sw->send(tPtr,*outp,true);
			outp->reset(peer,m_ctx.identity.address(),Packet::VERB_MULTICAST_LIKE);
		}

		// network ID, MAC, ADI
		outp->append((uint64_t)_id);
		mg->mac().appendTo(*outp);
		outp->append((uint32_t)mg->adi());
	}

	if (outp->size() > ZT_PROTO_MIN_PACKET_LENGTH) {
		outp->compress();
		m_ctx.sw->send(tPtr,*outp,true);
	}
#endif
}

Vector<MulticastGroup> Network::m_allMulticastGroups() const
{
    // Assumes _myMulticastGroups_l is locked
    Vector<MulticastGroup> mgs;
    mgs.reserve(m_myMulticastGroups.size() + m_multicastGroupsBehindMe.size() + 1);
    mgs.insert(mgs.end(), m_myMulticastGroups.begin(), m_myMulticastGroups.end());
    for (Map<MulticastGroup, int64_t>::const_iterator i(m_multicastGroupsBehindMe.begin());
         i != m_multicastGroupsBehindMe.end(); ++i)
        mgs.push_back(i->first);
    if ((m_config) && (m_config.enableBroadcast()))
        mgs.push_back(Network::BROADCAST);
    std::sort(mgs.begin(), mgs.end());
    mgs.erase(std::unique(mgs.begin(), mgs.end()), mgs.end());
    return mgs;
}

}   // namespace ZeroTier
