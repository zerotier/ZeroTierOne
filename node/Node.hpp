/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
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

#ifndef ZT_NODE_HPP
#define ZT_NODE_HPP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>

#include "Constants.hpp"

#include "../include/ZeroTierOne.h"

#include "InetAddress.hpp"
#include "Mutex.hpp"
#include "MAC.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * Implementation of Node object as defined in CAPI
 *
 * The pointer returned by ZT1_Node_new() is an instance of this class.
 */
class Node
{
public:
	Node(
		ZT1_DataStoreGetFunction *dataStoreGetFunction,
		ZT1_DataStorePutFunction *dataStorePutFunction,
		ZT1_VirtualNetworkConfigCallback *networkConfigCallback,
		ZT1_StatusCallback *statusCallback);

	~Node();

	// Public API Functions ----------------------------------------------------

	ZT1_ResultCode run(
		uint64_t now,
		const ZT1_WireMessage *inputWireMessages,
		unsigned int inputWireMessageCount,
		const ZT1_VirtualLanFrame *inputLanFrames,
		unsigned int inputLanFrameCount,
		const ZT1_WireMessage **outputWireMessages,
		unsigned int *outputWireMessageCount,
		const ZT1_VirtualNetworkFrame **outputFrames,
		unsigned int *outputLanFrameCount,
		unsigned long *maxNextInterval);

	ZT1_ResultCode join(uint64_t nwid);

	ZT1_ResultCode leave(uint64_t nwid);

	void status(ZT1_NodeStatus *status);

	ZT1_PeerList *peers();

	ZT1_VirtualNetworkConfig *networkConfig(uint64_t nwid);

	ZT1_VirtualNetworkList *listNetworks();

	void freeQueryResult(void *qr);

	ZT1_ResultCode setNetconfMaster(
		ZT1_Node *node,
		void *networkConfigMasterInstance);

	// Internal functions ------------------------------------------------------

	/**
	 * @return Time as of last call to run()
	 */
	inline uint64_t now() const throw() { return _now; }

	/**
	 * @return Current level of desperation
	 */
	inline int desperation() const throw() { return (int)((_now - _timeOfLastPrivilgedPacket) / ZT_DESPERATION_INCREMENT); }

	/**
	 * Enqueue a ZeroTier message to be sent
	 *
	 * @param addr Destination address
	 * @param data Packet data
	 * @param len Packet length
	 */
	inline void putPacket(const InetAddress &addr,const void *data,unsigned int len)
	{
		Mutex::Lock _l(_outputWireMessages_m);
		if (_outputWireMessageCount >= _outputWireMessageCapacity) {
			ZT1_WireMessage *old = _outputWireMessages;
			_outputWireMessages = new ZT1_WireMessage[_outputWireMessageCapacity *= 2];
			memcpy(_outputWireMessages,old,sizeof(ZT1_WireMessage) * _outputWireMessageCount);
			delete [] old;
		}
		ZT1_WireMessage &wm = _outputWireMessages[_outputWireMessageCount++];
		memcpy(&(wm.address),&addr,sizeof(ZT_SOCKADDR_STORAGE));
		wm.desperation = this->desperation();
		wm.spam = (int)((++_spamCounter % ZT_DESPERATION_SPAM_EVERY) == 0);
		memcpy(wm.packetData,data,len);
		wm.packetLength = len;
	}

	/**
	 * Enqueue a frame to be injected into a tap device (port)
	 *
	 * @param nwid Network ID
	 * @param source Source MAC
	 * @param dest Destination MAC
	 * @param etherType 16-bit ethernet type
	 * @param vlanId VLAN ID or 0 if none
	 * @param data Frame data
	 * @param len Frame length
	 */
	inline void putFrame(uint64_t nwid,const MAC &source,const MAC &dest,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len)
	{
		Mutex::Lock _l(_outputFrames_m);
		if (_outputFrameCount >= _outputFrameCapacity) {
			ZT1_VirtualNetworkFrame *old = _outputFrames;
			_outputFrames = new ZT1_VirtualNetworkFrame[_outputFrameCapacity *= 2];
			memcpy(_outputFrames,old,sizeof(ZT1_VirtualNetworkFrame) * _outputFrameCount);
			delete [] old;
		}
		ZT1_VirtualNetworkFrame &f = _outputFrames[_outputFrameCount++];
		f.nwid = nwid;
		f.sourceMac = source.toInt();
		f.destMac = dest.toInt();
		f.etherType = etherType;
		f.vlanId = vlanId;
		memcpy(f.frameData,data,len);
		f.frameLength = len;
	}

	/**
	 * @param nwid Network ID
	 * @return Network instance
	 */
	inline SharedPtr<Network> network(uint64_t nwid)
	{
		Mutex::Lock _l(_networks_m);
		std::map< uint64_t,Network >::iterator nw(_networks.find(nwid));
		return ((nw == _networks.end()) ? SharedPtr<Network>() : nw->second);
	}

private:
	RuntimeEnvironment *RR;

	ZT1_WireMessage *_outputWireMessages;
	unsigned long _outputWireMessageCount;
	unsigned long _outputWireMessageCapacity;
	Mutex _outputWireMessages_m;

	ZT1_VirtualNetworkFrame *_outputFrames;
	unsigned long _outputFrameCount;
	unsigned long _outputFrameCapacity;
	Mutex _outputFrames_m;

	ZT1_DataStoreGetFunction *_dataStoreGetFunction,
	ZT1_DataStorePutFunction *_dataStorePutFunction,
	ZT1_VirtualPortConfigCallback *_portConfigCallback,
	ZT1_StatusCallback *_statusCallback);

	//Dictionary _localConfig; // persisted as local.conf
	//Mutex _localConfig_m;

	std::map< uint64_t,SharedPtr<Network> > _networks;
	Mutex _networks_m;

	uint64_t _now; // time of last run()
	uint64_t _timeOfLastPacketReceived;
	uint64_t _timeOfLastPrivilgedPacket;
	unsigned int _spamCounter; // used to "spam" every Nth packet
};

} // namespace ZeroTier

#endif
