/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#ifndef _ZT_PACKETDECODER_HPP
#define _ZT_PACKETDECODER_HPP

#include <stdexcept>

#include "Packet.hpp"
#include "Demarc.hpp"
#include "InetAddress.hpp"
#include "Utils.hpp"
#include "SharedPtr.hpp"
#include "AtomicCounter.hpp"
#include "Peer.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Subclass of packet that handles the decoding of it
 */
class PacketDecoder : public Packet
{
	friend class SharedPtr<PacketDecoder>;

public:
	template<unsigned int C2>
	PacketDecoder(const Buffer<C2> &b,Demarc::Port localPort,const InetAddress &remoteAddress)
 		throw(std::out_of_range) :
 		Packet(b),
 		_receiveTime(Utils::now()),
 		_localPort(localPort),
 		_remoteAddress(remoteAddress),
 		_step(DECODE_STEP_WAITING_FOR_SENDER_LOOKUP),
 		__refCount()
	{
	}

	/**
	 * Attempt to decode this packet
	 *
	 * @param _r Runtime environment
	 * @return True if decoding and processing is complete, false on failure (try again)
	 */
	bool tryDecode(const RuntimeEnvironment *_r)
		throw(std::out_of_range,std::runtime_error);

	/**
	 * @return Time of packet receipt
	 */
	inline uint64_t receiveTime() const throw() { return _receiveTime; }

private:
	struct _CBaddPeerFromHello_Data
	{
		const RuntimeEnvironment *renv;
		Address source;
		InetAddress remoteAddress;
		int localPort;
		unsigned int vMajor,vMinor,vRevision;
		uint64_t helloPacketId;
		uint64_t helloTimestamp;
	};
	static void _CBaddPeerFromHello(
		void *arg, // _CBaddPeerFromHello_Data
		const SharedPtr<Peer> &p,
		Topology::PeerVerifyResult result);

	static void _CBaddPeerFromWhois(
		void *arg, // RuntimeEnvironment
		const SharedPtr<Peer> &p,
		Topology::PeerVerifyResult result);

	bool _doERROR(const RuntimeEnvironment *_r,const SharedPtr<Peer> &peer);
	bool _doHELLO(const RuntimeEnvironment *_r);
	bool _doOK(const RuntimeEnvironment *_r,const SharedPtr<Peer> &peer);
	bool _doWHOIS(const RuntimeEnvironment *_r,const SharedPtr<Peer> &peer);
	bool _doRENDEZVOUS(const RuntimeEnvironment *_r,const SharedPtr<Peer> &peer);
	bool _doFRAME(const RuntimeEnvironment *_r,const SharedPtr<Peer> &peer);
	bool _doMULTICAST_LIKE(const RuntimeEnvironment *_r,const SharedPtr<Peer> &peer);
	bool _doMULTICAST_FRAME(const RuntimeEnvironment *_r,const SharedPtr<Peer> &peer);

	uint64_t _receiveTime;
	Demarc::Port _localPort;
	InetAddress _remoteAddress;

	enum {
		DECODE_STEP_WAITING_FOR_SENDER_LOOKUP, // on initial receipt, we need peer's identity
		DECODE_STEP_WAITING_FOR_ORIGINAL_SUBMITTER_LOOKUP // this only applies to MULTICAST_FRAME
	} _step;

	AtomicCounter __refCount;
};

} // namespace ZeroTier

#endif
