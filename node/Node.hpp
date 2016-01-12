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

#ifndef ZT_NODE_HPP
#define ZT_NODE_HPP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>

#include "Constants.hpp"

#include "../include/ZeroTierOne.h"

#include "RuntimeEnvironment.hpp"
#include "InetAddress.hpp"
#include "Mutex.hpp"
#include "MAC.hpp"
#include "Network.hpp"
#include "Path.hpp"
#include "Salsa20.hpp"

#undef TRACE
#ifdef ZT_TRACE
#define TRACE(f,...) RR->node->postTrace(__FILE__,__LINE__,f,##__VA_ARGS__)
#else
#define TRACE(f,...) {}
#endif

namespace ZeroTier {

/**
 * Implementation of Node object as defined in CAPI
 *
 * The pointer returned by ZT_Node_new() is an instance of this class.
 */
class Node
{
public:
	Node(
		uint64_t now,
		void *uptr,
		ZT_DataStoreGetFunction dataStoreGetFunction,
		ZT_DataStorePutFunction dataStorePutFunction,
		ZT_WirePacketSendFunction wirePacketSendFunction,
		ZT_VirtualNetworkFrameFunction virtualNetworkFrameFunction,
		ZT_VirtualNetworkConfigFunction virtualNetworkConfigFunction,
		ZT_PathCheckFunction pathCheckFunction,
		ZT_EventCallback eventCallback);

	~Node();

	// Public API Functions ----------------------------------------------------

	ZT_ResultCode processWirePacket(
		uint64_t now,
		const struct sockaddr_storage *localAddress,
		const struct sockaddr_storage *remoteAddress,
		const void *packetData,
		unsigned int packetLength,
		volatile uint64_t *nextBackgroundTaskDeadline);
	ZT_ResultCode processVirtualNetworkFrame(
		uint64_t now,
		uint64_t nwid,
		uint64_t sourceMac,
		uint64_t destMac,
		unsigned int etherType,
		unsigned int vlanId,
		const void *frameData,
		unsigned int frameLength,
		volatile uint64_t *nextBackgroundTaskDeadline);
	ZT_ResultCode processBackgroundTasks(uint64_t now,volatile uint64_t *nextBackgroundTaskDeadline);
	ZT_ResultCode join(uint64_t nwid,void *uptr);
	ZT_ResultCode leave(uint64_t nwid,void **uptr);
	ZT_ResultCode multicastSubscribe(uint64_t nwid,uint64_t multicastGroup,unsigned long multicastAdi);
	ZT_ResultCode multicastUnsubscribe(uint64_t nwid,uint64_t multicastGroup,unsigned long multicastAdi);
	uint64_t address() const;
	void status(ZT_NodeStatus *status) const;
	ZT_PeerList *peers() const;
	ZT_VirtualNetworkConfig *networkConfig(uint64_t nwid) const;
	ZT_VirtualNetworkList *networks() const;
	void freeQueryResult(void *qr);
	int addLocalInterfaceAddress(const struct sockaddr_storage *addr);
	void clearLocalInterfaceAddresses();
	void setNetconfMaster(void *networkControllerInstance);
	ZT_ResultCode circuitTestBegin(ZT_CircuitTest *test,void (*reportCallback)(ZT_Node *,ZT_CircuitTest *,const ZT_CircuitTestReport *));
	void circuitTestEnd(ZT_CircuitTest *test);
	ZT_ResultCode clusterInit(
		unsigned int myId,
		const struct sockaddr_storage *zeroTierPhysicalEndpoints,
		unsigned int numZeroTierPhysicalEndpoints,
		int x,
		int y,
		int z,
		void (*sendFunction)(void *,unsigned int,const void *,unsigned int),
		void *sendFunctionArg,
		int (*addressToLocationFunction)(void *,const struct sockaddr_storage *,int *,int *,int *),
		void *addressToLocationFunctionArg);
	ZT_ResultCode clusterAddMember(unsigned int memberId);
	void clusterRemoveMember(unsigned int memberId);
	void clusterHandleIncomingMessage(const void *msg,unsigned int len);
	void clusterStatus(ZT_ClusterStatus *cs);
	void backgroundThreadMain();

	// Internal functions ------------------------------------------------------

	/**
	 * Convenience threadMain() for easy background thread launch
	 *
	 * This allows background threads to be launched with Thread::start
	 * that will run against this node.
	 */
	inline void threadMain() throw() { this->backgroundThreadMain(); }

	/**
	 * @return Time as of last call to run()
	 */
	inline uint64_t now() const throw() { return _now; }

	/**
	 * Enqueue a ZeroTier message to be sent
	 *
	 * @param localAddress Local address
	 * @param addr Destination address
	 * @param data Packet data
	 * @param len Packet length
	 * @param ttl Desired TTL (default: 0 for unchanged/default TTL)
	 * @return True if packet appears to have been sent
	 */
	inline bool putPacket(const InetAddress &localAddress,const InetAddress &addr,const void *data,unsigned int len,unsigned int ttl = 0)
	{
		return (_wirePacketSendFunction(
			reinterpret_cast<ZT_Node *>(this),
			_uPtr,
			reinterpret_cast<const struct sockaddr_storage *>(&localAddress),
			reinterpret_cast<const struct sockaddr_storage *>(&addr),
			data,
			len,
			ttl) == 0);
	}

	/**
	 * Enqueue a frame to be injected into a tap device (port)
	 *
	 * @param nwid Network ID
	 * @param nuptr Network user ptr
	 * @param source Source MAC
	 * @param dest Destination MAC
	 * @param etherType 16-bit ethernet type
	 * @param vlanId VLAN ID or 0 if none
	 * @param data Frame data
	 * @param len Frame length
	 */
	inline void putFrame(uint64_t nwid,void **nuptr,const MAC &source,const MAC &dest,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len)
	{
		_virtualNetworkFrameFunction(
			reinterpret_cast<ZT_Node *>(this),
			_uPtr,
			nwid,
			nuptr,
			source.toInt(),
			dest.toInt(),
			etherType,
			vlanId,
			data,
			len);
	}

	/**
	 * @param localAddress Local address
	 * @param remoteAddress Remote address
	 * @return True if path should be used
	 */
	bool shouldUsePathForZeroTierTraffic(const InetAddress &localAddress,const InetAddress &remoteAddress);

	inline SharedPtr<Network> network(uint64_t nwid) const
	{
		Mutex::Lock _l(_networks_m);
		return _network(nwid);
	}

	inline bool belongsToNetwork(uint64_t nwid) const
	{
		Mutex::Lock _l(_networks_m);
		for(std::vector< std::pair< uint64_t, SharedPtr<Network> > >::const_iterator i=_networks.begin();i!=_networks.end();++i) {
			if (i->first == nwid)
				return true;
		}
		return false;
	}

	inline std::vector< SharedPtr<Network> > allNetworks() const
	{
		std::vector< SharedPtr<Network> > nw;
		Mutex::Lock _l(_networks_m);
		nw.reserve(_networks.size());
		for(std::vector< std::pair< uint64_t, SharedPtr<Network> > >::const_iterator i=_networks.begin();i!=_networks.end();++i)
			nw.push_back(i->second);
		return nw;
	}

	/**
	 * @return Potential direct paths to me a.k.a. local interface addresses
	 */
	inline std::vector<InetAddress> directPaths() const
	{
		Mutex::Lock _l(_directPaths_m);
		return _directPaths;
	}

	inline bool dataStorePut(const char *name,const void *data,unsigned int len,bool secure) { return (_dataStorePutFunction(reinterpret_cast<ZT_Node *>(this),_uPtr,name,data,len,(int)secure) == 0); }
	inline bool dataStorePut(const char *name,const std::string &data,bool secure) { return dataStorePut(name,(const void *)data.data(),(unsigned int)data.length(),secure); }
	inline void dataStoreDelete(const char *name) { _dataStorePutFunction(reinterpret_cast<ZT_Node *>(this),_uPtr,name,(const void *)0,0,0); }
	std::string dataStoreGet(const char *name);

	/**
	 * Post an event to the external user
	 *
	 * @param ev Event type
	 * @param md Meta-data (default: NULL/none)
	 */
	inline void postEvent(ZT_Event ev,const void *md = (const void *)0) { _eventCallback(reinterpret_cast<ZT_Node *>(this),_uPtr,ev,md); }

	/**
	 * Update virtual network port configuration
	 *
	 * @param nwid Network ID
	 * @param nuptr Network user ptr
	 * @param op Configuration operation
	 * @param nc Network configuration
	 */
	inline int configureVirtualNetworkPort(uint64_t nwid,void **nuptr,ZT_VirtualNetworkConfigOperation op,const ZT_VirtualNetworkConfig *nc) { return _virtualNetworkConfigFunction(reinterpret_cast<ZT_Node *>(this),_uPtr,nwid,nuptr,op,nc); }

	/**
	 * @return True if we appear to be online
	 */
	inline bool online() const throw() { return _online; }

#ifdef ZT_TRACE
	void postTrace(const char *module,unsigned int line,const char *fmt,...);
#endif

	/**
	 * @return Next 64-bit random number (not for cryptographic use)
	 */
	uint64_t prng();

	/**
	 * Post a circuit test report to any listeners for a given test ID
	 *
	 * @param report Report (includes test ID)
	 */
	void postCircuitTestReport(const ZT_CircuitTestReport *report);

private:
	inline SharedPtr<Network> _network(uint64_t nwid) const
	{
		// assumes _networks_m is locked
		for(std::vector< std::pair< uint64_t, SharedPtr<Network> > >::const_iterator i=_networks.begin();i!=_networks.end();++i) {
			if (i->first == nwid)
				return i->second;
		}
		return SharedPtr<Network>();
	}

	RuntimeEnvironment _RR;
	RuntimeEnvironment *RR;

	void *_uPtr; // _uptr (lower case) is reserved in Visual Studio :P

	ZT_DataStoreGetFunction _dataStoreGetFunction;
	ZT_DataStorePutFunction _dataStorePutFunction;
	ZT_WirePacketSendFunction _wirePacketSendFunction;
	ZT_VirtualNetworkFrameFunction _virtualNetworkFrameFunction;
	ZT_VirtualNetworkConfigFunction _virtualNetworkConfigFunction;
	ZT_PathCheckFunction _pathCheckFunction;
	ZT_EventCallback _eventCallback;

	std::vector< std::pair< uint64_t, SharedPtr<Network> > > _networks;
	Mutex _networks_m;

	std::vector< ZT_CircuitTest * > _circuitTests;
	Mutex _circuitTests_m;

	std::vector<InetAddress> _directPaths;
	Mutex _directPaths_m;

	Mutex _backgroundTasksLock;

	unsigned int _prngStreamPtr;
	Salsa20 _prng;
	uint64_t _prngStream[16]; // repeatedly encrypted with _prng to yield a high-quality non-crypto PRNG stream

	uint64_t _now;
	uint64_t _lastPingCheck;
	uint64_t _lastHousekeepingRun;
	bool _online;
};

} // namespace ZeroTier

#endif
