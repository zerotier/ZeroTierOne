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

#ifndef ZT_VL1_HPP
#define ZT_VL1_HPP

#include "Constants.hpp"
#include "Defragmenter.hpp"
#include "Buf.hpp"
#include "Address.hpp"
#include "Protocol.hpp"
#include "Mutex.hpp"
#include "FCV.hpp"
#include "Containers.hpp"

#define ZT_VL1_MAX_WHOIS_WAITING_PACKETS 32

#define ZT_VL1_AUTH_RESULT_FLAG_AUTHENTICATED  0x01U
#define ZT_VL1_AUTH_RESULT_FLAG_ENCRYPTED      0x02U
#define ZT_VL1_AUTH_RESULT_FLAG_FORWARD_SECRET 0x04U

namespace ZeroTier {

class RuntimeEnvironment;

class Peer;

class VL2;

/**
 * VL1 (virtual layer 1) packet I/O and messaging.
 *
 * This class is thread safe.
 */
class VL1
{
public:
	explicit VL1(const RuntimeEnvironment *renv);

	/**
	 * Called when a packet is received from the real network
	 *
	 * The packet data supplied to this method may be modified. Internal
	 * packet handler code may also take possession of it via atomic swap
	 * and leave the 'data' pointer NULL. The 'data' pointer and its
	 * contents should not be used after this call. Make a copy if the
	 * data might still be needed.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param localSocket Local I/O socket as supplied by external code
	 * @param fromAddr Internet IP address of origin
	 * @param data Packet data
	 * @param len Packet length
	 */
	void onRemotePacket(void *tPtr, int64_t localSocket, const InetAddress &fromAddr, SharedPtr< Buf > &data, unsigned int len) noexcept;

private:
	const RuntimeEnvironment *RR;

	void m_relay(void *tPtr, const SharedPtr< Path > &path, Address destination, SharedPtr< Buf > &pkt, int pktSize);

	void m_sendPendingWhois(void *tPtr, int64_t now);

	SharedPtr< Peer > m_HELLO(void *tPtr, const SharedPtr< Path > &path, Buf &pkt, int packetSize);

	bool m_ERROR(void *tPtr, uint64_t packetId, unsigned int auth, const SharedPtr< Path > &path, const SharedPtr< Peer > &peer, Buf &pkt, int packetSize, Protocol::Verb &inReVerb);

	bool m_OK(void *tPtr, uint64_t packetId, unsigned int auth, const SharedPtr< Path > &path, const SharedPtr< Peer > &peer, Buf &pkt, int packetSize, Protocol::Verb &inReVerb);

	bool m_WHOIS(void *tPtr, uint64_t packetId, unsigned int auth, const SharedPtr< Path > &path, const SharedPtr< Peer > &peer, Buf &pkt, int packetSize);

	bool m_RENDEZVOUS(void *tPtr, uint64_t packetId, unsigned int auth, const SharedPtr< Path > &path, const SharedPtr< Peer > &peer, Buf &pkt, int packetSize);

	bool m_ECHO(void *tPtr, uint64_t packetId, unsigned int auth, const SharedPtr< Path > &path, const SharedPtr< Peer > &peer, Buf &pkt, int packetSize);

	bool m_PUSH_DIRECT_PATHS(void *tPtr, uint64_t packetId, unsigned int auth, const SharedPtr< Path > &path, const SharedPtr< Peer > &peer, Buf &pkt, int packetSize);

	bool m_USER_MESSAGE(void *tPtr, uint64_t packetId, unsigned int auth, const SharedPtr< Path > &path, const SharedPtr< Peer > &peer, Buf &pkt, int packetSize);

	bool m_ENCAP(void *tPtr, uint64_t packetId, unsigned int auth, const SharedPtr< Path > &path, const SharedPtr< Peer > &peer, Buf &pkt, int packetSize);

	// Defragmentation engine for handling inbound packets with more than one fragment.
	Defragmenter< ZT_MAX_PACKET_FRAGMENTS > m_inputPacketAssembler;

	// Queue of outbound WHOIS reqeusts and packets waiting on them.
	struct p_WhoisQueueItem
	{
		ZT_INLINE p_WhoisQueueItem() : lastRetry(0), retries(0), waitingPacketCount(0)
		{}

		int64_t lastRetry;
		unsigned int retries;
		unsigned int waitingPacketCount;
		unsigned int waitingPacketSize[ZT_VL1_MAX_WHOIS_WAITING_PACKETS];
		SharedPtr< Buf > waitingPacket[ZT_VL1_MAX_WHOIS_WAITING_PACKETS];
	};

	Map< Address, p_WhoisQueueItem > m_whoisQueue;
	Mutex m_whoisQueue_l;
};

} // namespace ZeroTier

#endif
