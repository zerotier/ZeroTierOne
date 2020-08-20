/*
 * Copyright (c)2019 ZeroTier, Inc.
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

#include "NeighborDiscovery.hpp"
#include "OSUtils.hpp"

#include "../include/ZeroTierOne.h"

#include <assert.h>

namespace ZeroTier {

uint16_t calc_checksum (uint16_t *addr, int len)
{
    int count = len;
    uint32_t sum = 0;
    uint16_t answer = 0;

    // Sum up 2-byte values until none or only one byte left.
    while (count > 1) {
        sum += *(addr++);
        count -= 2;
    }

    // Add left-over byte, if any.
    if (count > 0) {
        sum += *(uint8_t *) addr;
    }

    // Fold 32-bit sum into 16 bits; we lose information by doing this,
    // increasing the chances of a collision.
    // sum = (lower 16 bits) + (upper 16 bits shifted right 16 bits)
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }

    // Checksum is one's compliment of sum.
    answer = ~sum;

    return (answer);
}

struct _pseudo_header {
    uint8_t sourceAddr[16];
    uint8_t targetAddr[16];
    uint32_t length;
    uint8_t zeros[3];
    uint8_t next;  // 58
};

struct _option {
    _option(int optionType)
        : type(optionType)
        , length(8)
    {
        memset(mac, 0, sizeof(mac));
    }

    uint8_t type;
    uint8_t length;
    uint8_t mac[6];
};

struct _neighbor_solicitation {
    _neighbor_solicitation()
        : type(135)
        , code(0)
        , checksum(0)
        , option(1)
    {
        memset(&reserved, 0, sizeof(reserved));
        memset(target, 0, sizeof(target));
    }

    void calculateChecksum(const sockaddr_storage &sourceIp, const sockaddr_storage &destIp) {
        _pseudo_header ph;
        memset(&ph, 0, sizeof(_pseudo_header));
        const sockaddr_in6 *src = (const sockaddr_in6*)&sourceIp;
        const sockaddr_in6 *dest = (const sockaddr_in6*)&destIp;

        memcpy(ph.sourceAddr, &src->sin6_addr, sizeof(struct in6_addr));
        memcpy(ph.targetAddr, &dest->sin6_addr, sizeof(struct in6_addr));
        ph.next = 58;
        ph.length = htonl(sizeof(_neighbor_solicitation));

        size_t len = sizeof(_pseudo_header) + sizeof(_neighbor_solicitation);
        uint8_t *tmp = (uint8_t*)malloc(len);
        memcpy(tmp, &ph, sizeof(_pseudo_header));
        memcpy(tmp+sizeof(_pseudo_header), this, sizeof(_neighbor_solicitation));

        checksum = calc_checksum((uint16_t*)tmp, (int)len);

        free(tmp);
        tmp = NULL;
    }

    uint8_t type; // 135
    uint8_t code; // 0
    uint16_t checksum;
    uint32_t reserved;
    uint8_t target[16];
    _option option;
};

struct _neighbor_advertisement {
    _neighbor_advertisement()
        : type(136)
        , code(0)
        , checksum(0)
        , rso(0x40)
        , option(2)
    {
        memset(padding, 0, sizeof(padding));
        memset(target, 0, sizeof(target));
    }

    void calculateChecksum(const sockaddr_storage &sourceIp, const sockaddr_storage &destIp) {
        _pseudo_header ph;
        memset(&ph, 0, sizeof(_pseudo_header));
        const sockaddr_in6 *src = (const sockaddr_in6*)&sourceIp;
        const sockaddr_in6 *dest = (const sockaddr_in6*)&destIp;

        memcpy(ph.sourceAddr, &src->sin6_addr, sizeof(struct in6_addr));
        memcpy(ph.targetAddr, &dest->sin6_addr, sizeof(struct in6_addr));
        ph.next = 58;
        ph.length = htonl(sizeof(_neighbor_advertisement));

        size_t len = sizeof(_pseudo_header) + sizeof(_neighbor_advertisement);
        uint8_t *tmp = (uint8_t*)malloc(len);
        memcpy(tmp, &ph, sizeof(_pseudo_header));
        memcpy(tmp+sizeof(_pseudo_header), this, sizeof(_neighbor_advertisement));

        checksum = calc_checksum((uint16_t*)tmp, (int)len);

        free(tmp);
        tmp = NULL;
    }

    uint8_t type; // 136
    uint8_t code; // 0
    uint16_t checksum;
    uint8_t rso;
    uint8_t padding[3];
    uint8_t target[16];
    _option option;
};

NeighborDiscovery::NeighborDiscovery()
    : _cache(256)
    , _lastCleaned(OSUtils::now())
{}

void NeighborDiscovery::addLocal(const sockaddr_storage &address, const MAC &mac)
{
    _NDEntry &e = _cache[InetAddress(address)];
    e.lastQuerySent = 0;
    e.lastResponseReceived = 0;
    e.mac = mac;
    e.local = true;
}

void NeighborDiscovery::remove(const sockaddr_storage &address)
{
    _cache.erase(InetAddress(address));
}

sockaddr_storage NeighborDiscovery::processIncomingND(const uint8_t *nd, unsigned int len, const sockaddr_storage &localIp, uint8_t *response, unsigned int &responseLen, MAC &responseDest)
{
    assert(sizeof(_neighbor_solicitation) == 28);
    assert(sizeof(_neighbor_advertisement) == 32);

    const uint64_t now = OSUtils::now();
    sockaddr_storage ip = {0};

    if (len >= sizeof(_neighbor_solicitation) && nd[0] == 0x87) {
        // respond to Neighbor Solicitation request for local address
        _neighbor_solicitation solicitation;
        memcpy(&solicitation, nd, len);
        InetAddress targetAddress(solicitation.target, 16, 0);
        _NDEntry *targetEntry = _cache.get(targetAddress);
        if (targetEntry && targetEntry->local) {
            _neighbor_advertisement adv;
            targetEntry->mac.copyTo(adv.option.mac, 6);
            memcpy(adv.target, solicitation.target, 16);
            adv.calculateChecksum(localIp, targetAddress);
            memcpy(response, &adv, sizeof(_neighbor_advertisement));
            responseLen = sizeof(_neighbor_advertisement);
            responseDest.setTo(solicitation.option.mac, 6);
        }
    } else if (len >= sizeof(_neighbor_advertisement) && nd[0] == 0x88) {
        _neighbor_advertisement adv;
        memcpy(&adv, nd, len);
        InetAddress responseAddress(adv.target, 16, 0);
        _NDEntry *queryEntry = _cache.get(responseAddress);
        if(queryEntry && !queryEntry->local && (now - queryEntry->lastQuerySent <= ZT_ND_QUERY_MAX_TTL)) {
            queryEntry->lastResponseReceived = now;
            queryEntry->mac.setTo(adv.option.mac, 6);
            ip = responseAddress;
        }
    }

    if ((now - _lastCleaned) >= ZT_ND_EXPIRE) {
        _lastCleaned = now;
        Hashtable<InetAddress, _NDEntry>::Iterator i(_cache);
        InetAddress *k = NULL;
        _NDEntry *v = NULL;
        while (i.next(k, v)) {
            if(!v->local && (now - v->lastResponseReceived) >= ZT_ND_EXPIRE) {
                _cache.erase(*k);
            }
        }
    }

    return ip;
}

MAC NeighborDiscovery::query(const MAC &localMac, const sockaddr_storage &localIp, const sockaddr_storage &targetIp, uint8_t *query, unsigned int &queryLen, MAC &queryDest)
{
    const uint64_t now = OSUtils::now();

    InetAddress localAddress(localIp);
    localAddress.setPort(0);
    InetAddress targetAddress(targetIp);
    targetAddress.setPort(0);

    _NDEntry &e = _cache[targetAddress];

    if ( (e.mac && ((now - e.lastResponseReceived) >= (ZT_ND_EXPIRE / 3))) ||
         (!e.mac && ((now - e.lastQuerySent) >= ZT_ND_QUERY_INTERVAL))) {
        e.lastQuerySent = now;

        _neighbor_solicitation ns;
        memcpy(ns.target, targetAddress.rawIpData(), 16);
        localMac.copyTo(ns.option.mac, 6);
        ns.calculateChecksum(localIp, targetIp);
        if (e.mac) {
            queryDest = e.mac;
        } else {
            queryDest = (uint64_t)0xffffffffffffULL;
        }
    } else {
        queryLen = 0;
        queryDest.zero();
    }

    return e.mac;
}

}
