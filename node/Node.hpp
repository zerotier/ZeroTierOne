/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

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
#include "Hashtable.hpp"

// Bit mask for "expecting reply" hash
#define ZT_EXPECTING_REPLIES_BUCKET_MASK1 255
#define ZT_EXPECTING_REPLIES_BUCKET_MASK2 31

namespace ZeroTier {

/**
 * Implementation of Node object as defined in CAPI
 *
 * The pointer returned by ZT_Node_new() is an instance of this class.
 */
class Node : public NetworkController::Sender
{
public:
	Node(void *uptr,void *tptr,const struct ZT_Node_Callbacks *callbacks,int64_t now);
	virtual ~Node();

	// Get rid of alignment warnings on 32-bit Windows and possibly improve performance
#ifdef __WINDOWS__
	void * operator new(size_t i) { return _mm_malloc(i,16); }
	void operator delete(void* p) { _mm_free(p); }
#endif

	// Public API Functions ----------------------------------------------------

	ZT_ResultCode processWirePacket(
		void *tptr,
		int64_t now,
		int64_t localSocket,
		const struct sockaddr_storage *remoteAddress,
		const void *packetData,
		unsigned int packetLength,
		volatile int64_t *nextBackgroundTaskDeadline);
	ZT_ResultCode processVirtualNetworkFrame(
		void *tptr,
		int64_t now,
		uint64_t nwid,
		uint64_t sourceMac,
		uint64_t destMac,
		unsigned int etherType,
		unsigned int vlanId,
		const void *frameData,
		unsigned int frameLength,
		volatile int64_t *nextBackgroundTaskDeadline);
	ZT_ResultCode processBackgroundTasks(void *tptr,int64_t now,volatile int64_t *nextBackgroundTaskDeadline);
	void processDNSResult(
		void *tptr,
		uintptr_t dnsRequestID,
		const char *name,
		enum ZT_DNSRecordType recordType,
		const void *result,
		unsigned int resultLength,
		int resultIsString);
	ZT_ResultCode join(uint64_t nwid,void *uptr,void *tptr);
	ZT_ResultCode leave(uint64_t nwid,void **uptr,void *tptr);
	ZT_ResultCode multicastSubscribe(void *tptr,uint64_t nwid,uint64_t multicastGroup,unsigned long multicastAdi);
	ZT_ResultCode multicastUnsubscribe(uint64_t nwid,uint64_t multicastGroup,unsigned long multicastAdi);
	ZT_RootList *listRoots(int64_t now);
	enum ZT_ResultCode setRoot(const char *name,const void *locator,unsigned int locatorSize);
	enum ZT_ResultCode removeRoot(const char *name);
	uint64_t address() const;
	void status(ZT_NodeStatus *status) const;
	ZT_PeerList *peers() const;
	ZT_VirtualNetworkConfig *networkConfig(uint64_t nwid) const;
	ZT_VirtualNetworkList *networks() const;
	void setNetworkUserPtr(uint64_t nwid,void *ptr);
	void freeQueryResult(void *qr);
	int addLocalInterfaceAddress(const struct sockaddr_storage *addr);
	void clearLocalInterfaceAddresses();
	int sendUserMessage(void *tptr,uint64_t dest,uint64_t typeId,const void *data,unsigned int len);
	void setController(void *networkControllerInstance);

	// Internal functions ------------------------------------------------------

	ZT_ALWAYS_INLINE int64_t now() const { return _now; }

	ZT_ALWAYS_INLINE bool putPacket(void *tPtr,const int64_t localSocket,const InetAddress &addr,const void *data,unsigned int len,unsigned int ttl = 0)
	{
		return (_cb.wirePacketSendFunction(
			reinterpret_cast<ZT_Node *>(this),
			_uPtr,
			tPtr,
			localSocket,
			reinterpret_cast<const struct sockaddr_storage *>(&addr),
			data,
			len,
			ttl) == 0);
	}

	ZT_ALWAYS_INLINE void putFrame(void *tPtr,uint64_t nwid,void **nuptr,const MAC &source,const MAC &dest,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len)
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

	ZT_ALWAYS_INLINE SharedPtr<Network> network(uint64_t nwid) const
	{
		Mutex::Lock _l(_networks_m);
		const SharedPtr<Network> *n = _networks.get(nwid);
		if (n)
			return *n;
		return SharedPtr<Network>();
	}

	ZT_ALWAYS_INLINE bool belongsToNetwork(uint64_t nwid) const
	{
		Mutex::Lock _l(_networks_m);
		return _networks.contains(nwid);
	}

	ZT_ALWAYS_INLINE std::vector< SharedPtr<Network> > allNetworks() const
	{
		std::vector< SharedPtr<Network> > nw;
		Mutex::Lock _l(_networks_m);
		Hashtable< uint64_t,SharedPtr<Network> >::Iterator i(*const_cast< Hashtable< uint64_t,SharedPtr<Network> > * >(&_networks));
		uint64_t *k = (uint64_t *)0;
		SharedPtr<Network> *v = (SharedPtr<Network> *)0;
		while (i.next(k,v))
			nw.push_back(*v);
		return nw;
	}

	ZT_ALWAYS_INLINE std::vector<InetAddress> directPaths() const
	{
		Mutex::Lock _l(_localInterfaceAddresses_m);
		return _localInterfaceAddresses;
	}

	ZT_ALWAYS_INLINE void postEvent(void *tPtr,ZT_Event ev,const void *md = (const void *)0) { _cb.eventCallback(reinterpret_cast<ZT_Node *>(this),_uPtr,tPtr,ev,md); }
	ZT_ALWAYS_INLINE void configureVirtualNetworkPort(void *tPtr,uint64_t nwid,void **nuptr,ZT_VirtualNetworkConfigOperation op,const ZT_VirtualNetworkConfig *nc) { _cb.virtualNetworkConfigFunction(reinterpret_cast<ZT_Node *>(this),_uPtr,tPtr,nwid,nuptr,op,nc); }
	ZT_ALWAYS_INLINE bool online() const { return _online; }
	ZT_ALWAYS_INLINE int stateObjectGet(void *const tPtr,ZT_StateObjectType type,const uint64_t id[2],void *const data,const unsigned int maxlen) { return _cb.stateGetFunction(reinterpret_cast<ZT_Node *>(this),_uPtr,tPtr,type,id,data,maxlen); }
	ZT_ALWAYS_INLINE void stateObjectPut(void *const tPtr,ZT_StateObjectType type,const uint64_t id[2],const void *const data,const unsigned int len) { _cb.statePutFunction(reinterpret_cast<ZT_Node *>(this),_uPtr,tPtr,type,id,data,(int)len); }
	ZT_ALWAYS_INLINE void stateObjectDelete(void *const tPtr,ZT_StateObjectType type,const uint64_t id[2]) { _cb.statePutFunction(reinterpret_cast<ZT_Node *>(this),_uPtr,tPtr,type,id,(const void *)0,-1); }
	bool shouldUsePathForZeroTierTraffic(void *tPtr,const Address &ztaddr,const int64_t localSocket,const InetAddress &remoteAddress);
	ZT_ALWAYS_INLINE bool externalPathLookup(void *tPtr,const Address &ztaddr,int family,InetAddress &addr) { return ( (_cb.pathLookupFunction) ? (_cb.pathLookupFunction(reinterpret_cast<ZT_Node *>(this),_uPtr,tPtr,ztaddr.toInt(),family,reinterpret_cast<struct sockaddr_storage *>(&addr)) != 0) : false ); }
	ZT_ResultCode setPhysicalPathConfiguration(const struct sockaddr_storage *pathNetwork,const ZT_PhysicalPathConfiguration *pathConfig);
	ZT_ALWAYS_INLINE const Identity &identity() const { return _RR.identity; }

	/**
	 * Register that we are expecting a reply to a packet ID
	 *
	 * This only uses the most significant bits of the packet ID, both to save space
	 * and to avoid using the higher bits that can be modified during armor() to
	 * mask against the packet send counter used for QoS detection.
	 *
	 * @param packetId Packet ID to expect reply to
	 */
	ZT_ALWAYS_INLINE void expectReplyTo(const uint64_t packetId)
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
	ZT_ALWAYS_INLINE bool expectingReplyTo(const uint64_t packetId) const
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
	ZT_ALWAYS_INLINE bool rateGateIdentityVerification(const int64_t now,const InetAddress &from)
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

	ZT_ALWAYS_INLINE void setMultipathMode(uint8_t mode) { _multipathMode = mode; }
	ZT_ALWAYS_INLINE uint8_t getMultipathMode() { return _multipathMode; }

	ZT_ALWAYS_INLINE bool localControllerHasAuthorized(const int64_t now,const uint64_t nwid,const Address &addr) const
	{
		_localControllerAuthorizations_m.lock();
		const int64_t *const at = _localControllerAuthorizations.get(_LocalControllerAuth(nwid,addr));
		_localControllerAuthorizations_m.unlock();
		if (at)
			return ((now - *at) < (ZT_NETWORK_AUTOCONF_DELAY * 3));
		return false;
	}

private:
	RuntimeEnvironment _RR;
	RuntimeEnvironment *RR;
	void *_uPtr; // _uptr (lower case) is reserved in Visual Studio :P
	ZT_Node_Callbacks _cb;

	// For tracking packet IDs to filter out OK/ERROR replies to packets we did not send
	uint8_t _expectingRepliesToBucketPtr[ZT_EXPECTING_REPLIES_BUCKET_MASK1 + 1];
	uint32_t _expectingRepliesTo[ZT_EXPECTING_REPLIES_BUCKET_MASK1 + 1][ZT_EXPECTING_REPLIES_BUCKET_MASK2 + 1];

	// Time of last identity verification indexed by InetAddress.rateGateHash() -- used in IncomingPacket::_doHELLO() via rateGateIdentityVerification()
	int64_t _lastIdentityVerification[16384];

	/* Map that remembers if we have recently sent a network config to someone
	 * querying us as a controller. This is an optimization to allow network
	 * controllers to know whether to treat things like multicast queries the
	 * way authorized members would be treated without requiring an extra cert
	 * validation. */
	struct _LocalControllerAuth
	{
		uint64_t nwid,address;
		ZT_ALWAYS_INLINE _LocalControllerAuth(const uint64_t nwid_,const Address &address_) : nwid(nwid_),address(address_.toInt()) {}
		ZT_ALWAYS_INLINE unsigned long hashCode() const { return (unsigned long)(nwid ^ address); }
		ZT_ALWAYS_INLINE bool operator==(const _LocalControllerAuth &a) const { return ((a.nwid == nwid)&&(a.address == address)); }
		ZT_ALWAYS_INLINE bool operator!=(const _LocalControllerAuth &a) const { return ((a.nwid != nwid)||(a.address != address)); }
	};
	Hashtable< _LocalControllerAuth,int64_t > _localControllerAuthorizations;
	Mutex _localControllerAuthorizations_m;

	// Curreently joined networks
	Hashtable< uint64_t,SharedPtr<Network> > _networks;
	Mutex _networks_m;

	// Local interface addresses as reported by the code harnessing this Node
	std::vector<InetAddress> _localInterfaceAddresses;
	Mutex _localInterfaceAddresses_m;

	// Lock to ensure processBackgroundTasks never gets run concurrently
	Mutex _backgroundTasksLock;

	uint8_t _multipathMode;

	volatile int64_t _now;
	int64_t _lastPing;
	int64_t _lastHousekeepingRun;
	int64_t _lastNetworkHousekeepingRun;
	int64_t _lastDynamicRootUpdate;
	bool _online;
};

} // namespace ZeroTier

#endif
