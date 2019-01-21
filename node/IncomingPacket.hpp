/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifndef ZT_INCOMINGPACKET_HPP
#define ZT_INCOMINGPACKET_HPP

#include <stdexcept>

#include "Packet.hpp"
#include "Path.hpp"
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
		_receiveTime(0)
	{
	}

	/**
	 * Create a new packet-in-decode
	 *
	 * @param data Packet data
	 * @param len Packet length
	 * @param path Path over which packet arrived
	 * @param now Current time
	 * @throws std::out_of_range Range error processing packet
	 */
	IncomingPacket(const void *data,unsigned int len,const SharedPtr<Path> &path,int64_t now) :
		Packet(data,len),
		_receiveTime(now),
		_path(path)
	{
	}

	/**
	 * Init packet-in-decode in place
	 *
	 * @param data Packet data
	 * @param len Packet length
	 * @param path Path over which packet arrived
	 * @param now Current time
	 * @throws std::out_of_range Range error processing packet
	 */
	inline void init(const void *data,unsigned int len,const SharedPtr<Path> &path,int64_t now)
	{
		copyFrom(data,len);
		_receiveTime = now;
		_path = path;
	}

	/**
	 * Attempt to decode this packet
	 *
	 * Note that this returns 'true' if processing is complete. This says nothing
	 * about whether the packet was valid. A rejection is 'complete.'
	 *
	 * Once true is returned, this must not be called again. The packet's state
	 * may no longer be valid.
	 *
	 * @param RR Runtime environment
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @return True if decoding and processing is complete, false if caller should try again
	 */
	bool tryDecode(const RuntimeEnvironment *RR,void *tPtr);

	/**
	 * @return Time of packet receipt / start of decode
	 */
	inline uint64_t receiveTime() const { return _receiveTime; }

private:
	// These are called internally to handle packet contents once it has
	// been authenticated, decrypted, decompressed, and classified.
	bool _doERROR(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer);
	bool _doHELLO(const RuntimeEnvironment *RR,void *tPtr,const bool alreadyAuthenticated);
	bool _doACK(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer);
	bool _doQOS_MEASUREMENT(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer);
	bool _doOK(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer);
	bool _doWHOIS(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer);
	bool _doRENDEZVOUS(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer);
	bool _doFRAME(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer);
	bool _doEXT_FRAME(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer);
	bool _doECHO(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer);
	bool _doMULTICAST_LIKE(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer);
	bool _doNETWORK_CREDENTIALS(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer);
	bool _doNETWORK_CONFIG_REQUEST(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer);
	bool _doNETWORK_CONFIG(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer);
	bool _doMULTICAST_GATHER(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer);
	bool _doMULTICAST_FRAME(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer);
	bool _doPUSH_DIRECT_PATHS(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer);
	bool _doUSER_MESSAGE(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer);
	bool _doREMOTE_TRACE(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer);

	void _sendErrorNeedCredentials(const RuntimeEnvironment *RR,void *tPtr,const SharedPtr<Peer> &peer,const uint64_t nwid);

	uint64_t _receiveTime;
	SharedPtr<Path> _path;
};

} // namespace ZeroTier

#endif
