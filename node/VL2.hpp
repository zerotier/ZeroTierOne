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

#ifndef ZT_VL2_HPP
#define ZT_VL2_HPP

#include "Constants.hpp"
#include "Buf.hpp"
#include "Address.hpp"
#include "Protocol.hpp"
#include "Mutex.hpp"
#include "FCV.hpp"
#include "Containers.hpp"

namespace ZeroTier {

class Path;
class Peer;
class RuntimeEnvironment;
class VL1;
class Network;
class MAC;

class VL2
{
	friend class VL1;

public:
	explicit VL2(const RuntimeEnvironment *renv);

	/**
	 * Called when a packet comes from a local Ethernet tap
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param network Which network's TAP did this packet come from?
	 * @param from Originating MAC address
	 * @param to Destination MAC address
	 * @param etherType Ethernet packet type
	 * @param vlanId VLAN ID or 0 if none
	 * @param data Ethernet payload
	 * @param len Frame length
	 */
	void onLocalEthernet(void *tPtr,const SharedPtr<Network> &network,const MAC &from,const MAC &to,unsigned int etherType,unsigned int vlanId,SharedPtr<Buf> &data,unsigned int len);

protected:
	bool m_FRAME(void *tPtr, uint64_t packetId, unsigned int auth, const SharedPtr<Path> &path, SharedPtr<Peer> &peer, Buf &pkt, int packetSize);
	bool m_EXT_FRAME(void *tPtr, uint64_t packetId, unsigned int auth, const SharedPtr<Path> &path, SharedPtr<Peer> &peer, Buf &pkt, int packetSize);
	bool m_MULTICAST_LIKE(void *tPtr, uint64_t packetId, unsigned int auth, const SharedPtr<Path> &path, SharedPtr<Peer> &peer, Buf &pkt, int packetSize);
	bool m_NETWORK_CREDENTIALS(void *tPtr, uint64_t packetId, unsigned int auth, const SharedPtr<Path> &path, SharedPtr<Peer> &peer, Buf &pkt, int packetSize);
	bool m_NETWORK_CONFIG_REQUEST(void *tPtr, uint64_t packetId, unsigned int auth, const SharedPtr<Path> &path, SharedPtr<Peer> &peer, Buf &pkt, int packetSize);
	bool m_NETWORK_CONFIG(void *tPtr, uint64_t packetId, unsigned int auth, const SharedPtr<Path> &path, SharedPtr<Peer> &peer, Buf &pkt, int packetSize);
	bool m_MULTICAST_GATHER(void *tPtr, uint64_t packetId, unsigned int auth, const SharedPtr<Path> &path, SharedPtr<Peer> &peer, Buf &pkt, int packetSize);
	bool m_MULTICAST_FRAME_deprecated(void *tPtr, uint64_t packetId, unsigned int auth, const SharedPtr<Path> &path, SharedPtr<Peer> &peer, Buf &pkt, int packetSize);
	bool m_MULTICAST(void *tPtr, uint64_t packetId, unsigned int auth, const SharedPtr<Path> &path, SharedPtr<Peer> &peer, Buf &pkt, int packetSize);

private:
};

} // namespace ZeroTier

#endif
