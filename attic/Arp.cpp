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

#include <string.h>
#include <stdlib.h>

#include "Arp.hpp"
#include "OSUtils.hpp"

namespace ZeroTier {

static const uint8_t ARP_REQUEST_HEADER[8] = {0x00, 0x01, 0x08, 0x00, 0x06, 0x04, 0x00, 0x01};
static const uint8_t ARP_RESPONSE_HEADER[8] = {0x00, 0x01, 0x08, 0x00, 0x06, 0x04, 0x00, 0x02};

Arp::Arp() :
	_cache(),
	_lastCleaned(OSUtils::now())
{
}

void Arp::addLocal(uint32_t ip, const MAC &mac)
{
	_ArpEntry &e = _cache[ip];
	e.lastQuerySent = 0; // local IP
	e.lastResponseReceived = 0; // local IP
	e.mac = mac;
	e.local = true;
}

void Arp::remove(uint32_t ip)
{
	_cache.erase(ip);
}

uint32_t Arp::processIncomingArp(const void *arp, unsigned int len, void *response, unsigned int &responseLen, MAC &responseDest)
{
	const uint64_t now = OSUtils::now();
	uint32_t ip = 0;

	responseLen = 0;
	responseDest.zero();

	if (len >= 28) {
		if (!memcmp(arp, ARP_REQUEST_HEADER, 8)) {
			// Respond to ARP requests for locally-known IPs
			Map< uint32_t, Arp::_ArpEntry >::const_iterator targetEntry(_cache.find(reinterpret_cast<const uint32_t *>(arp)[6]));
			if ((targetEntry != _cache.end()) && (targetEntry->second.local)) {
				memcpy(response, ARP_RESPONSE_HEADER, 8);
				targetEntry->second.mac.copyTo(reinterpret_cast<uint8_t *>(response) + 8);
				memcpy(reinterpret_cast<uint8_t *>(response) + 14, reinterpret_cast<const uint8_t *>(arp) + 24, 4);
				memcpy(reinterpret_cast<uint8_t *>(response) + 18, reinterpret_cast<const uint8_t *>(arp) + 8, 10);
				responseLen = 28;
				responseDest.setTo(reinterpret_cast<const uint8_t *>(arp) + 8);
			}
		} else if (!memcmp(arp, ARP_RESPONSE_HEADER, 8)) {
			// Learn cache entries for remote IPs from relevant ARP replies
			uint32_t responseIp = 0;
			memcpy(&responseIp, reinterpret_cast<const uint8_t *>(arp) + 14, 4);
			Map< uint32_t, Arp::_ArpEntry >::iterator queryEntry(_cache.find(responseIp));
			if ((queryEntry != _cache.end()) && (!queryEntry->second.local) && ((now - queryEntry->second.lastQuerySent) <= ZT_ARP_QUERY_MAX_TTL)) {
				queryEntry->second.lastResponseReceived = now;
				queryEntry->second.mac.setTo(reinterpret_cast<const uint8_t *>(arp) + 8);
				ip = responseIp;
			}
		}
	}

	if ((now - _lastCleaned) >= ZT_ARP_EXPIRE) {
		_lastCleaned = now;
		for (Map< uint32_t, _ArpEntry >::iterator i(_cache.begin()); i != _cache.end();) {
			if ((!i->second.local) && ((now - i->second.lastResponseReceived) >= ZT_ARP_EXPIRE))
				_cache.erase(i++);
			else ++i;
		}
	}

	return ip;
}

MAC Arp::query(const MAC &localMac, uint32_t localIp, uint32_t targetIp, void *query, unsigned int &queryLen, MAC &queryDest)
{
	const uint64_t now = OSUtils::now();

	_ArpEntry &e = _cache[targetIp];

	if (((e.mac) && ((now - e.lastResponseReceived) >= (ZT_ARP_EXPIRE / 3))) ||
	    ((!e.mac) && ((now - e.lastQuerySent) >= ZT_ARP_QUERY_INTERVAL))) {
		e.lastQuerySent = now;

		uint8_t *q = reinterpret_cast<uint8_t *>(query);
		memcpy(q, ARP_REQUEST_HEADER, 8);
		q += 8; // ARP request header information, always the same
		localMac.copyTo(q);
		q += 6; // sending host MAC address
		memcpy(q, &localIp, 4);
		q += 4; // sending host IP (IP already in big-endian byte order)
		memset(q, 0, 6);
		q += 6; // sending zeros for target MAC address as thats what we want to find
		memcpy(q, &targetIp, 4); // target IP address for resolution (IP already in big-endian byte order)
		queryLen = 28;
		if (e.mac)
			queryDest = e.mac; // confirmation query, send directly to address holder
		else queryDest = (uint64_t)0xffffffffffffULL; // broadcast query
	} else {
		queryLen = 0;
		queryDest.zero();
	}

	return e.mac;
}

} // namespace ZeroTier
