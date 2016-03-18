/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2016  ZeroTier, Inc.  https://www.zerotier.com/
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
 */

#ifndef ZT_INCOMINGPACKET_HPP
#define ZT_INCOMINGPACKET_HPP

#include <stdexcept>

#include "Packet.hpp"
#include "InetAddress.hpp"
#include "Utils.hpp"
#include "MulticastGroup.hpp"
#include "Peer.hpp"

/*
 * The big picture:
 *
 * tryDecode gets called for a given fully-assembled packet until it returns
 * true or the packet's time to live has been exceeded, in which case it is
 * discarded as failed decode. Any exception thrown by tryDecode also causes
 * the packet to be discarded.
 *
 * Thus a return of false from tryDecode() indicates that it should be called
 * again. Logic is very simple as to when, and it's in doAnythingWaitingForPeer
 * in Switch. This might be expanded to be more fine grained in the future.
 *
 * A return value of true indicates that the packet is done. tryDecode must
 * never be called again after that.
 */

namespace ZeroTier {

class RuntimeEnvironment;
class Network;

/**
 * Subclass of packet that handles the decoding of it
 */
class IncomingPacket : public Packet
{
public:
	IncomingPacket() :
		Packet(),
		_receiveTime(0),
		_localAddress(),
		_remoteAddress()
	{
	}

	IncomingPacket(const IncomingPacket &p)
	{
		// All fields including InetAddress are memcpy'able
		memcpy(this,&p,sizeof(IncomingPacket));
	}

	/**
	 * Create a new packet-in-decode
	 *
	 * @param data Packet data
	 * @param len Packet length
	 * @param localAddress Local interface address
	 * @param remoteAddress Address from which packet came
	 * @param now Current time
	 * @throws std::out_of_range Range error processing packet
	 */
	IncomingPacket(const void *data,unsigned int len,const InetAddress &localAddress,const InetAddress &remoteAddress,uint64_t now) :
 		Packet(data,len),
 		_receiveTime(now),
 		_localAddress(localAddress),
 		_remoteAddress(remoteAddress)
	{
	}

	inline IncomingPacket &operator=(const IncomingPacket &p)
	{
		// All fields including InetAddress are memcpy'able
		memcpy(this,&p,sizeof(IncomingPacket));
		return *this;
	}

	/**
	 * Init packet-in-decode in place
	 *
	 * @param data Packet data
	 * @param len Packet length
	 * @param localAddress Local interface address
	 * @param remoteAddress Address from which packet came
	 * @param now Current time
	 * @throws std::out_of_range Range error processing packet
	 */
	inline void init(const void *data,unsigned int len,const InetAddress &localAddress,const InetAddress &remoteAddress,uint64_t now)
	{
		copyFrom(data,len);
		_receiveTime = now;
		_localAddress = localAddress;
		_remoteAddress = remoteAddress;
	}

	/**
	 * Attempt to decode this packet
	 *
	 * Note that this returns 'true' if processing is complete. This says nothing
	 * about whether the packet was valid. A rejection is 'complete.'
	 *
	 * Once true is returned, this must not be called again. The packet's state
	 * may no longer be valid. The only exception is deferred decoding. In this
	 * case true is returned to indicate to the normal decode path that it is
	 * finished with the packet. The packet will have added itself to the
	 * deferred queue and will expect tryDecode() to be called one more time
	 * with deferred set to true.
	 *
	 * Deferred decoding is performed by DeferredPackets.cpp and should not be
	 * done elsewhere. Under deferred decoding packets only get one shot and
	 * so the return value of tryDecode() is ignored.
	 *
	 * @param RR Runtime environment
	 * @param deferred If true, this is a deferred decode and the return is ignored
	 * @return True if decoding and processing is complete, false if caller should try again
	 */
	bool tryDecode(const RuntimeEnvironment *RR,bool deferred);

	/**
	 * @return Time of packet receipt / start of decode
	 */
	inline uint64_t receiveTime() const throw() { return _receiveTime; }

	/**
	 * Compute the Salsa20/12+SHA512 proof of work function
	 *
	 * @param difficulty Difficulty in bits (max: 64)
	 * @param challenge Challenge string
	 * @param challengeLength Length of challenge in bytes (max allowed: ZT_PROTO_MAX_PACKET_LENGTH)
	 * @param result Buffer to fill with 16-byte result
	 */
	static void computeSalsa2012Sha512ProofOfWork(unsigned int difficulty,const void *challenge,unsigned int challengeLength,unsigned char result[16]);

	/**
	 * Verify the result of Salsa20/12+SHA512 proof of work
	 *
	 * @param difficulty Difficulty in bits (max: 64)
	 * @param challenge Challenge bytes
	 * @param challengeLength Length of challenge in bytes (max allowed: ZT_PROTO_MAX_PACKET_LENGTH)
	 * @param proposedResult Result supplied by client
	 * @return True if result is valid
	 */
	static bool testSalsa2012Sha512ProofOfWorkResult(unsigned int difficulty,const void *challenge,unsigned int challengeLength,const unsigned char proposedResult[16]);

private:
	// These are called internally to handle packet contents once it has
	// been authenticated, decrypted, decompressed, and classified.
	bool _doERROR(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer);
	bool _doHELLO(const RuntimeEnvironment *RR,SharedPtr<Peer> &peer); // can be called with NULL peer, while all others cannot
	bool _doOK(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer);
	bool _doWHOIS(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer);
	bool _doRENDEZVOUS(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer);
	bool _doFRAME(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer);
	bool _doEXT_FRAME(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer);
	bool _doECHO(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer);
	bool _doMULTICAST_LIKE(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer);
	bool _doNETWORK_MEMBERSHIP_CERTIFICATE(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer);
	bool _doNETWORK_CONFIG_REQUEST(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer);
	bool _doNETWORK_CONFIG_REFRESH(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer);
	bool _doMULTICAST_GATHER(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer);
	bool _doMULTICAST_FRAME(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer);
	bool _doPUSH_DIRECT_PATHS(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer);
	bool _doCIRCUIT_TEST(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer);
	bool _doCIRCUIT_TEST_REPORT(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer);
	bool _doREQUEST_PROOF_OF_WORK(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer);

	// Send an ERROR_NEED_MEMBERSHIP_CERTIFICATE to a peer indicating that an updated cert is needed to communicate
	void _sendErrorNeedCertificate(const RuntimeEnvironment *RR,const SharedPtr<Peer> &peer,uint64_t nwid);

	uint64_t _receiveTime;
	InetAddress _localAddress;
	InetAddress _remoteAddress;
};

} // namespace ZeroTier

#endif
