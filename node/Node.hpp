/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2017  ZeroTier, Inc.  https://www.zerotier.com/
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
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifndef ZT_NODE_HPP
#define ZT_NODE_HPP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <vector>

#include "Constants.hpp"

#include "../include/ZeroTierOne.h"

#include "RuntimeEnvironment.hpp"
#include "InetAddress.hpp"
#include "Mutex.hpp"
#include "MAC.hpp"
#include "Network.hpp"
#include "Path.hpp"
#include "Salsa20.hpp"
#include "NetworkController.hpp"

#undef TRACE
#ifdef ZT_TRACE
#define TRACE(f,...) RR->node->postTrace(__FILE__,__LINE__,f,##__VA_ARGS__)
#else
#define TRACE(f,...) {}
#endif

// Bit mask for "expecting reply" hash
#define ZT_EXPECTING_REPLIES_BUCKET_MASK1 255
#define ZT_EXPECTING_REPLIES_BUCKET_MASK2 31

namespace ZeroTier {

class World;

/**
 * Implementation of Node object as defined in CAPI
 *
 * The pointer returned by ZT_Node_new() is an instance of this class.
 */
class Node : public NetworkController::Sender
{
public:
	Node(void *uptr,void *tptr,const struct ZT_Node_Callbacks *callbacks,uint64_t now);
	virtual ~Node();

	// Get rid of alignment warnings on 32-bit Windows and possibly improve performance
#ifdef __WINDOWS__
	void * operator new(size_t i) { return _mm_malloc(i,16); }
	void operator delete(void* p) { _mm_free(p); }
#endif

	// Public API Functions ----------------------------------------------------

	ZT_ResultCode processWirePacket(
		void *tptr,
		uint64_t now,
		const struct sockaddr_storage *localAddress,
		const struct sockaddr_storage *remoteAddress,
		const void *packetData,
		unsigned int packetLength,
		volatile uint64_t *nextBackgroundTaskDeadline);
	ZT_ResultCode processVirtualNetworkFrame(
		void *tptr,
		uint64_t now,
		uint64_t nwid,
		uint64_t sourceMac,
		uint64_t destMac,
		unsigned int etherType,
		unsigned int vlanId,
		const void *frameData,
		unsigned int frameLength,
		volatile uint64_t *nextBackgroundTaskDeadline);
	ZT_ResultCode processBackgroundTasks(void *tptr,uint64_t now,volatile uint64_t *nextBackgroundTaskDeadline);
	ZT_ResultCode join(uint64_t nwid,void *uptr,void *tptr);
	ZT_ResultCode leave(uint64_t nwid,void **uptr,void *tptr);
	ZT_ResultCode multicastSubscribe(void *tptr,uint64_t nwid,uint64_t multicastGroup,unsigned long multicastAdi);
	ZT_ResultCode multicastUnsubscribe(uint64_t nwid,uint64_t multicastGroup,unsigned long multicastAdi);
	ZT_ResultCode orbit(void *tptr,uint64_t moonWorldId,uint64_t moonSeed);
	ZT_ResultCode deorbit(void *tptr,uint64_t moonWorldId);
	uint64_t address() const;
	void status(ZT_NodeStatus *status) const;
	ZT_PeerList *peers() const;
	ZT_VirtualNetworkConfig *networkConfig(uint64_t nwid) const;
	ZT_VirtualNetworkList *networks() const;
	void freeQueryResult(void *qr);
	int addLocalInterfaceAddress(const struct sockaddr_storage *addr);
	void clearLocalInterfaceAddresses();
	int sendUserMessage(void *tptr,uint64_t dest,uint64_t typeId,const void *data,unsigned int len);
	void setNetconfMaster(void *networkControllerInstance);
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

	// Internal functions ------------------------------------------------------

	inline uint64_t now() const throw() { return _now; }

	inline bool putPacket(void *tPtr,const InetAddress &localAddress,const InetAddress &addr,const void *data,unsigned int len,unsigned int ttl = 0)
	{
		return (_cb.wirePacketSendFunction(
			reinterpret_cast<ZT_Node *>(this),
			_uPtr,
			tPtr,
			reinterpret_cast<const struct sockaddr_storage *>(&localAddress),
			reinterpret_cast<const struct sockaddr_storage *>(&addr),
			data,
			len,
			ttl) == 0);
	}

	inline void putFrame(void *tPtr,uint64_t nwid,void **nuptr,const MAC &source,const MAC &dest,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len)
	{
		_cb.virtualNetworkFrameFunction(
			reinterpret_cast<ZT_Node *>(this),
			_uPtr,
			tPtr,
			nwid,
			nuptr,
			source.toInt(),
			dest.toInt(),
			etherType,
			vlanId,
			data,
			len);
	}

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

	inline std::vector<InetAddress> directPaths() const
	{
		Mutex::Lock _l(_directPaths_m);
		return _directPaths;
	}

	inline bool dataStorePut(void *tPtr,const char *name,const void *data,unsigned int len,bool secure) { return (_cb.dataStorePutFunction(reinterpret_cast<ZT_Node *>(this),_uPtr,tPtr,name,data,len,(int)secure) == 0); }
	inline bool dataStorePut(void *tPtr,const char *name,const std::string &data,bool secure) { return dataStorePut(tPtr,name,(const void *)data.data(),(unsigned int)data.length(),secure); }
	inline void dataStoreDelete(void *tPtr,const char *name) { _cb.dataStorePutFunction(reinterpret_cast<ZT_Node *>(this),_uPtr,tPtr,name,(const void *)0,0,0); }
	std::string dataStoreGet(void *tPtr,const char *name);

	inline void postEvent(void *tPtr,ZT_Event ev,const void *md = (const void *)0) { _cb.eventCallback(reinterpret_cast<ZT_Node *>(this),_uPtr,tPtr,ev,md); }

	inline int configureVirtualNetworkPort(void *tPtr,uint64_t nwid,void **nuptr,ZT_VirtualNetworkConfigOperation op,const ZT_VirtualNetworkConfig *nc) { return _cb.virtualNetworkConfigFunction(reinterpret_cast<ZT_Node *>(this),_uPtr,tPtr,nwid,nuptr,op,nc); }

	inline bool online() const throw() { return _online; }

#ifdef ZT_TRACE
	void postTrace(const char *module,unsigned int line,const char *fmt,...);
#endif

	bool shouldUsePathForZeroTierTraffic(void *tPtr,const Address &ztaddr,const InetAddress &localAddress,const InetAddress &remoteAddress);
	inline bool externalPathLookup(void *tPtr,const Address &ztaddr,int family,InetAddress &addr) { return ( (_cb.pathLookupFunction) ? (_cb.pathLookupFunction(reinterpret_cast<ZT_Node *>(this),_uPtr,tPtr,ztaddr.toInt(),family,reinterpret_cast<struct sockaddr_storage *>(&addr)) != 0) : false ); }

	uint64_t prng();
	void setTrustedPaths(const struct sockaddr_storage *networks,const uint64_t *ids,unsigned int count);

	World planet() const;
	std::vector<World> moons() const;

	/**
	 * Register that we are expecting a reply to a packet ID
	 *
	 * This only uses the most significant bits of the packet ID, both to save space
	 * and to avoid using the higher bits that can be modified during armor() to
	 * mask against the packet send counter used for QoS detection.
	 *
	 * @param packetId Packet ID to expect reply to
	 */
	inline void expectReplyTo(const uint64_t packetId)
	{
		const unsigned long pid2 = (unsigned long)(packetId >> 32);
		const unsigned long bucket = (unsigned long)(pid2 & ZT_EXPECTING_REPLIES_BUCKET_MASK1);
		_expectingRepliesTo[bucket][_expectingRepliesToBucketPtr[bucket]++ & ZT_EXPECTING_REPLIES_BUCKET_MASK2] = (uint32_t)pid2;
	}

	/**
	 * Check whether a given packet ID is something we are expecting a reply to
	 *
	 * This only uses the most significant bits of the packet ID, both to save space
	 * and to avoid using the higher bits that can be modified during armor() to
	 * mask against the packet send counter used for QoS detection.
	 *
	 * @param packetId Packet ID to check
	 * @return True if we're expecting a reply
	 */
	inline bool expectingReplyTo(const uint64_t packetId) const
	{
		const uint32_t pid2 = (uint32_t)(packetId >> 32);
		const unsigned long bucket = (unsigned long)(pid2 & ZT_EXPECTING_REPLIES_BUCKET_MASK1);
		for(unsigned long i=0;i<=ZT_EXPECTING_REPLIES_BUCKET_MASK2;++i) {
			if (_expectingRepliesTo[bucket][i] == pid2)
				return true;
		}
		return false;
	}

	/**
	 * Check whether we should do potentially expensive identity verification (rate limit)
	 *
	 * @param now Current time
	 * @param from Source address of packet
	 * @return True if within rate limits
	 */
	inline bool rateGateIdentityVerification(const uint64_t now,const InetAddress &from)
	{
		unsigned long iph = from.rateGateHash();
		if ((now - _lastIdentityVerification[iph]) >= ZT_IDENTITY_VALIDATION_SOURCE_RATE_LIMIT) {
			_lastIdentityVerification[iph] = now;
			return true;
		}
		return false;
	}

	virtual void ncSendConfig(uint64_t nwid,uint64_t requestPacketId,const Address &destination,const NetworkConfig &nc,bool sendLegacyFormatConfig);
	virtual void ncSendRevocation(const Address &destination,const Revocation &rev);
	virtual void ncSendError(uint64_t nwid,uint64_t requestPacketId,const Address &destination,NetworkController::ErrorCode errorCode);

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
	ZT_Node_Callbacks _cb;

	// For tracking packet IDs to filter out OK/ERROR replies to packets we did not send
	uint8_t _expectingRepliesToBucketPtr[ZT_EXPECTING_REPLIES_BUCKET_MASK1 + 1];
	uint32_t _expectingRepliesTo[ZT_EXPECTING_REPLIES_BUCKET_MASK1 + 1][ZT_EXPECTING_REPLIES_BUCKET_MASK2 + 1];

	// Time of last identity verification indexed by InetAddress.rateGateHash() -- used in IncomingPacket::_doHELLO() via rateGateIdentityVerification()
	uint64_t _lastIdentityVerification[16384];

	std::vector< std::pair< uint64_t, SharedPtr<Network> > > _networks;
	Mutex _networks_m;

	std::vector<InetAddress> _directPaths;
	Mutex _directPaths_m;

	Mutex _backgroundTasksLock;

	uint64_t _now;
	uint64_t _lastPingCheck;
	uint64_t _lastHousekeepingRun;
	volatile uint64_t _prngState[2];
	bool _online;
};

} // namespace ZeroTier

#endif
