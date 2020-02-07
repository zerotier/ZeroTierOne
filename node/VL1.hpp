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

#ifndef ZT_VL1_HPP
#define ZT_VL1_HPP

#include "Constants.hpp"
#include "Defragmenter.hpp"
#include "Buf.hpp"
#include "Address.hpp"
#include "Protocol.hpp"
#include "Hashtable.hpp"
#include "Mutex.hpp"
#include "FCV.hpp"

#include <vector>

namespace ZeroTier {

class RuntimeEnvironment;
class Peer;
class VL2;

/**
 * VL1 (virtual layer 1) packet I/O and messaging
 */
class VL1
{
public:
	explicit VL1(const RuntimeEnvironment *renv);
	~VL1();

	/**
	 * Called when a packet is received from the real network
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param localSocket Local I/O socket as supplied by external code
	 * @param fromAddr Internet IP address of origin
	 * @param data Packet data
	 * @param len Packet length
	 */
	void onRemotePacket(void *tPtr,int64_t localSocket,const InetAddress &fromAddr,SharedPtr<Buf> &data,unsigned int len);

private:
	void _relay(void *tPtr,const SharedPtr<Path> &path,const Address &destination,SharedPtr<Buf> &data,unsigned int len);
	void _sendPendingWhois();

	// Handlers for VL1 verbs
	void _HELLO(void *tPtr,const SharedPtr<Path> &path,SharedPtr<Peer> &peer,Buf &pkt,int packetSize,bool authenticated);
	void _ERROR(void *tPtr,const SharedPtr<Path> &path,const SharedPtr<Peer> &peer,Buf &pkt,int packetSize,bool authenticated);
	void _OK(void *tPtr,const SharedPtr<Path> &path,const SharedPtr<Peer> &peer,Buf &pkt,int packetSize,bool authenticated);
	void _WHOIS(void *tPtr,const SharedPtr<Path> &path,const SharedPtr<Peer> &peer,Buf &pkt,int packetSize,bool authenticated);
	void _RENDEZVOUS(void *tPtr,const SharedPtr<Path> &path,const SharedPtr<Peer> &peer,Buf &pkt,int packetSize,bool authenticated);
	void _ECHO(void *tPtr,const SharedPtr<Path> &path,const SharedPtr<Peer> &peer,Buf &pkt,int packetSize,bool authenticated);
	void _PUSH_DIRECT_PATHS(void *tPtr,const SharedPtr<Path> &path,const SharedPtr<Peer> &peer,Buf &pkt,int packetSize,bool authenticated);
	void _USER_MESSAGE(void *tPtr,const SharedPtr<Path> &path,const SharedPtr<Peer> &peer,Buf &pkt,int packetSize,bool authenticated);
	void _ENCAP(void *tPtr,const SharedPtr<Path> &path,const SharedPtr<Peer> &peer,Buf &pkt,int packetSize,bool authenticated);

	const RuntimeEnvironment *RR;
	VL2 *const _vl2;

	struct _WhoisQueueItem
	{
		ZT_ALWAYS_INLINE _WhoisQueueItem() : lastRetry(0),inboundPackets(),retries(0) {}
		int64_t lastRetry;
		FCV<Buf::Slice,32> inboundPackets; // capacity can be changed but this should be plenty
		unsigned int retries;
	};

	Defragmenter<ZT_MAX_PACKET_FRAGMENTS> _inputPacketAssembler;

	Hashtable<Address,_WhoisQueueItem> _whoisQueue;
	Mutex _whoisQueue_l;
};

} // namespace ZeroTier

#endif
