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

#ifndef ZT_ARP_HPP
#define ZT_ARP_HPP

#include <stdint.h>

#include <utility>

#include "../node/Constants.hpp"
#include "../node/Hashtable.hpp"
#include "../node/MAC.hpp"

/**
 * Maximum possible ARP length
 *
 * ARPs are 28 bytes in length, but specify a 128 byte buffer since
 * some weird extensions we may support in the future can pad them
 * out to as long as 72 bytes.
 */
#define ZT_ARP_BUF_LENGTH 128

/**
 * Minimum permitted interval between sending ARP queries for a given IP
 */
#define ZT_ARP_QUERY_INTERVAL 2000

/**
 * Maximum time between query and response, otherwise responses are discarded to prevent poisoning
 */
#define ZT_ARP_QUERY_MAX_TTL 5000

/**
 * ARP expiration time
 */
#define ZT_ARP_EXPIRE 600000

namespace ZeroTier {

/**
 * ARP cache and resolver
 *
 * To implement ARP:
 *
 * (1) Call processIncomingArp() on all ARP packets received and then always
 * check responseLen after calling. If it is non-zero, send the contents
 * of response to responseDest.
 *
 * (2) Call query() to look up IP addresses, and then check queryLen. If it
 * is non-zero, send the contents of query to queryDest (usually broadcast).
 *
 * Note that either of these functions can technically generate a response or
 * a query at any time, so their result parameters for sending ARPs should
 * always be checked.
 *
 * This class is not thread-safe and must be guarded if used in multi-threaded
 * code.
 */
class Arp
{
public:
	Arp();

	/**
	 * Set a local IP entry that we should respond to ARPs for
	 *
	 * @param mac Our local MAC address
	 * @param ip IP in big-endian byte order (sin_addr.s_addr)
	 */
	void addLocal(uint32_t ip,const MAC &mac);

	/**
	 * Delete a local IP entry or a cached ARP entry
	 *
	 * @param ip IP in big-endian byte order (sin_addr.s_addr)
	 */
	void remove(uint32_t ip);

	/**
	 * Process ARP packets
	 *
	 * For ARP queries, a response is generated and responseLen is set to its
	 * frame payload length in bytes.
	 *
	 * For ARP responses, the cache is populated and the IP address entry that
	 * was learned is returned.
	 *
	 * @param arp ARP frame data
	 * @param len Length of ARP frame (usually 28)
	 * @param response Response buffer -- MUST be a minimum of ZT_ARP_BUF_LENGTH in size
	 * @param responseLen Response length, or set to 0 if no response
	 * @param responseDest Destination of response, or set to null if no response
	 * @return IP address learned or 0 if no new IPs in cache
	 */
	uint32_t processIncomingArp(const void *arp,unsigned int len,void *response,unsigned int &responseLen,MAC &responseDest);

	/**
	 * Get the MAC corresponding to an IP, generating a query if needed
	 *
	 * This returns a MAC for a remote IP. The local MAC is returned for local
	 * IPs as well. It may also generate a query if the IP is not known or the
	 * entry needs to be refreshed. In this case queryLen will be set to a
	 * non-zero value, so this should always be checked on return even if the
	 * MAC returned is non-null.
	 *
	 * @param localMac Local MAC address of host interface
     * @param localIp Local IP address of host interface
	 * @param targetIp IP to look up
	 * @param query Buffer for generated query -- MUST be a minimum of ZT_ARP_BUF_LENGTH in size
	 * @param queryLen Length of generated query, or set to 0 if no query generated
	 * @param queryDest Destination of query, or set to null if no query generated
	 * @return MAC or 0 if no cached entry for this IP
	 */
	MAC query(const MAC &localMac,uint32_t localIp,uint32_t targetIp,void *query,unsigned int &queryLen,MAC &queryDest);

private:
	struct _ArpEntry
	{
		_ArpEntry() : lastQuerySent(0),lastResponseReceived(0),mac(),local(false) {}
		uint64_t lastQuerySent; // Time last query was sent or 0 for local IP
		uint64_t lastResponseReceived; // Time of last ARP response or 0 for local IP
		MAC mac; // MAC address of device responsible for IP or null if not known yet
		bool local; // True if this is a local ARP entry
	};

	Hashtable< uint32_t,_ArpEntry > _cache;
	uint64_t _lastCleaned;
};

} // namespace ZeroTier

#endif
