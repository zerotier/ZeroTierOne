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
#include "Network.hpp"

#undef TRACE
#ifdef ZT_TRACE
#define TRACE(f,...) RR->node->postTrace(__FILE__,__LINE__,f,##__VA_ARGS__)
#else
#define TRACE(f,...) {}
#endif

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
		uint64_t now,
		void *uptr,
		ZT1_DataStoreGetFunction dataStoreGetFunction,
		ZT1_DataStorePutFunction dataStorePutFunction,
		ZT1_WirePacketSendFunction wirePacketSendFunction,
		ZT1_VirtualNetworkFrameFunction virtualNetworkFrameFunction,
		ZT1_VirtualNetworkConfigFunction virtualNetworkConfigFunction,
		ZT1_EventCallback eventCallback,
		const char *overrideRootTopology);

	~Node();

	// Public API Functions ----------------------------------------------------

	ZT1_ResultCode processWirePacket(
		uint64_t now,
		const struct sockaddr_storage *remoteAddress,
		unsigned int linkDesperation,
		const void *packetData,
		unsigned int packetLength,
		volatile uint64_t *nextBackgroundTaskDeadline);
	ZT1_ResultCode processVirtualNetworkFrame(
		uint64_t now,
		uint64_t nwid,
		uint64_t sourceMac,
		uint64_t destMac,
		unsigned int etherType,
		unsigned int vlanId,
		const void *frameData,
		unsigned int frameLength,
		volatile uint64_t *nextBackgroundTaskDeadline);
	ZT1_ResultCode processBackgroundTasks(uint64_t now,volatile uint64_t *nextBackgroundTaskDeadline);
	ZT1_ResultCode join(uint64_t nwid);
	ZT1_ResultCode leave(uint64_t nwid);
	ZT1_ResultCode multicastSubscribe(uint64_t nwid,uint64_t multicastGroup,unsigned long multicastAdi);
	ZT1_ResultCode multicastUnsubscribe(uint64_t nwid,uint64_t multicastGroup,unsigned long multicastAdi);
	uint64_t address() const;
	void status(ZT1_NodeStatus *status) const;
	ZT1_PeerList *peers() const;
	ZT1_VirtualNetworkConfig *networkConfig(uint64_t nwid) const;
	ZT1_VirtualNetworkList *networks() const;
	void freeQueryResult(void *qr);
	void setNetconfMaster(void *networkControllerInstance);

	// Internal functions ------------------------------------------------------

	/**
	 * @return Time as of last call to run()
	 */
	inline uint64_t now() const throw() { return _now; }

	/**
	 * Enqueue a ZeroTier message to be sent
	 *
	 * @param addr Destination address
	 * @param data Packet data
	 * @param len Packet length
	 * @param desperation Link desperation for reaching this address
	 * @return True if packet appears to have been sent
	 */
	inline bool putPacket(const InetAddress &addr,const void *data,unsigned int len,unsigned int desperation)
	{
		return (_wirePacketSendFunction(
			reinterpret_cast<ZT1_Node *>(this),
			_uptr,
			reinterpret_cast<const struct sockaddr_storage *>(&addr),
			desperation,
			data,
			len) == 0);
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
		_virtualNetworkFrameFunction(
			reinterpret_cast<ZT1_Node *>(this),
			_uptr,
			nwid,
			source.toInt(),
			dest.toInt(),
			etherType,
			vlanId,
			data,
			len);
	}

	inline SharedPtr<Network> network(uint64_t nwid)
	{
		Mutex::Lock _l(_networks_m);
		std::map< uint64_t,SharedPtr<Network> >::iterator nw(_networks.find(nwid));
		return ((nw == _networks.end()) ? SharedPtr<Network>() : nw->second);
	}

	inline std::vector< SharedPtr<Network> > allNetworks() const
	{
		Mutex::Lock _l(_networks_m);
		std::vector< SharedPtr<Network> > nw;
		for(std::map< uint64_t,SharedPtr<Network> >::const_iterator n(_networks.begin());n!=_networks.end();++n)
			nw.push_back(n->second);
		return nw;
	}

	/**
	 * Get an overall current level of desperation
	 *
	 * The current level of desperation is based on how recently an upstream
	 * (a.k.a. supernode) peer has spoken to us. As such, it will change and
	 * return to 0 once something like tunneling (higher desperation link) is
	 * active. As a result, actual link desperation for outgoing messages
	 * should be the max of either this or the most recent link desperation
	 * for an incoming message from a given address. See Path.hpp and Peer.hpp.
	 *
	 * In other words think of this as 'the desperation we should try to
	 * escalate to right now.'
	 *
	 * @return Overall system level of desperation
	 */
	inline unsigned int coreDesperation() const throw() { return _coreDesperation; }

	inline bool dataStorePut(const char *name,const void *data,unsigned int len,bool secure) { return (_dataStorePutFunction(reinterpret_cast<ZT1_Node *>(this),_uptr,name,data,len,(int)secure) == 0); }
	inline bool dataStorePut(const char *name,const std::string &data,bool secure) { return dataStorePut(name,(const void *)data.data(),(unsigned int)data.length(),secure); }
	inline void dataStoreDelete(const char *name) { _dataStorePutFunction(reinterpret_cast<ZT1_Node *>(this),_uptr,name,(const void *)0,0,0); }
	std::string dataStoreGet(const char *name);

	/**
	 * Post an event to the external user
	 *
	 * @param ev Event type
	 * @param md Meta-data (default: NULL/none)
	 */
	inline void postEvent(ZT1_Event ev,const void *md = (const void *)0) { _eventCallback(reinterpret_cast<ZT1_Node *>(this),_uptr,ev,md); }

	/**
	 * Update virtual network port configuration
	 *
	 * @param nwid Network ID
	 * @param op Configuration operation
	 * @param nc Network configuration
	 */
	inline int configureVirtualNetworkPort(uint64_t nwid,ZT1_VirtualNetworkConfigOperation op,const ZT1_VirtualNetworkConfig *nc) { return _virtualNetworkConfigFunction(reinterpret_cast<ZT1_Node *>(this),_uptr,nwid,op,nc); }

	/**
	 * @return True if we appear to be online
	 */
	inline bool online() const throw() { return _online; }

	/**
	 * If this version is newer than the newest we've seen, post a new version seen event
	 */
	void postNewerVersionIfNewer(unsigned int major,unsigned int minor,unsigned int rev);

#ifdef ZT_TRACE
	void postTrace(const char *module,unsigned int line,const char *fmt,...);
#endif

private:
	RuntimeEnvironment *RR;

	void *_uptr;

	ZT1_DataStoreGetFunction _dataStoreGetFunction;
	ZT1_DataStorePutFunction _dataStorePutFunction;
	ZT1_WirePacketSendFunction _wirePacketSendFunction;
	ZT1_VirtualNetworkFrameFunction _virtualNetworkFrameFunction;
	ZT1_VirtualNetworkConfigFunction _virtualNetworkConfigFunction;
	ZT1_EventCallback _eventCallback;

	//Dictionary _localConfig; // persisted as local.conf
	//Mutex _localConfig_m;

	std::map< uint64_t,SharedPtr<Network> > _networks;
	Mutex _networks_m;

	Mutex _backgroundTasksLock;

	uint64_t _now;
	uint64_t _startTimeAfterInactivity;
	uint64_t _lastPingCheck;
	uint64_t _lastHousekeepingRun;
	uint64_t _lastBeacon;
	unsigned int _coreDesperation;
	unsigned int _newestVersionSeen[3]; // major, minor, revision
	bool _online;
};

} // namespace ZeroTier

#endif
