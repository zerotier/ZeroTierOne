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

#ifndef ZT_NODE_HPP
#define ZT_NODE_HPP

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <vector>

#include "Constants.hpp"
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

class Locator;

/**
 * Implementation of Node object as defined in CAPI
 *
 * The pointer returned by ZT_Node_new() is an instance of this class.
 */
class Node : public NetworkController::Sender
{
public:
	Node(void *uPtr,void *tPtr,const struct ZT_Node_Callbacks *callbacks,int64_t now);
	virtual ~Node();

	/**
	 * Perform any operations that should be done prior to deleting a Node
	 *
	 * This is technically optional but recommended.
	 *
	 * @param tPtr Thread pointer to pass through to callbacks
	 */
	void shutdown(void *tPtr);

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
	ZT_ResultCode processBackgroundTasks(void *tPtr, int64_t now, volatile int64_t *nextBackgroundTaskDeadline);
	ZT_ResultCode join(uint64_t nwid,void *uptr,void *tptr);
	ZT_ResultCode leave(uint64_t nwid,void **uptr,void *tptr);
	ZT_ResultCode multicastSubscribe(void *tptr,uint64_t nwid,uint64_t multicastGroup,unsigned long multicastAdi);
	ZT_ResultCode multicastUnsubscribe(uint64_t nwid,uint64_t multicastGroup,unsigned long multicastAdi);
	ZT_ResultCode addRoot(void *tptr,const ZT_Identity *identity,const sockaddr_storage *bootstrap);
	ZT_ResultCode removeRoot(void *tptr,const ZT_Identity *identity);
	uint64_t address() const;
	void status(ZT_NodeStatus *status) const;
	ZT_PeerList *peers() const;
	ZT_VirtualNetworkConfig *networkConfig(uint64_t nwid) const;
	ZT_VirtualNetworkList *networks() const;
	void setNetworkUserPtr(uint64_t nwid,void *ptr);
	void freeQueryResult(void *qr);
	void setInterfaceAddresses(const ZT_InterfaceAddress *addrs,unsigned int addrCount);
	int sendUserMessage(void *tptr,uint64_t dest,uint64_t typeId,const void *data,unsigned int len);
	void setController(void *networkControllerInstance);

	// Internal functions ------------------------------------------------------

	/**
	 * @return Most recent time value supplied to core via API
	 */
	ZT_ALWAYS_INLINE int64_t now() const { return _now; }

	/**
	 * Send packet to to the physical wire via callback
	 *
	 * @param tPtr Thread pointer
	 * @param localSocket Local socket or -1 to use all/any
	 * @param addr Destination address
	 * @param data Data to send
	 * @param len Length in bytes
	 * @param ttl TTL or 0 for default/max
	 * @return True if send appears successful
	 */
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

	/**
	 * Inject frame into virtual Ethernet tap
	 *
	 * @param tPtr Thread pointer
	 * @param nwid Network ID
	 * @param nuptr Network-associated user pointer
	 * @param source Source MAC address
	 * @param dest Destination MAC address
	 * @param etherType 16-bit Ethernet type
	 * @param vlanId Ethernet VLAN ID (currently unused)
	 * @param data Ethernet frame data
	 * @param len Ethernet frame length in bytes
	 */
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

	/**
	 * @param nwid Network ID
	 * @return Network associated with ID
	 */
	ZT_ALWAYS_INLINE SharedPtr<Network> network(uint64_t nwid) const
	{
		RWMutex::RLock l(_networks_m);
		return _networks[(unsigned long)((nwid + (nwid >> 32U)) & _networksMask)];
	}

	/**
	 * @return Known local interface addresses for this node
	 */
	ZT_ALWAYS_INLINE std::vector<ZT_InterfaceAddress> localInterfaceAddresses() const
	{
		Mutex::Lock _l(_localInterfaceAddresses_m);
		return _localInterfaceAddresses;
	}

	/**
	 * Post an event via external callback
	 *
	 * @param tPtr Thread pointer
	 * @param ev Event object
	 * @param md Event data or NULL if none
	 */
	ZT_ALWAYS_INLINE void postEvent(void *tPtr,ZT_Event ev,const void *md = (const void *)0)
	{
		_cb.eventCallback(reinterpret_cast<ZT_Node *>(this),_uPtr,tPtr,ev,md);
	}

	/**
	 * Post network port configuration via external callback
	 *
	 * @param tPtr Thread pointer
	 * @param nwid Network ID
	 * @param nuptr Network-associated user pointer
	 * @param op Config operation or event type
	 * @param nc Network config info
	 */
	ZT_ALWAYS_INLINE void configureVirtualNetworkPort(void *tPtr,uint64_t nwid,void **nuptr,ZT_VirtualNetworkConfigOperation op,const ZT_VirtualNetworkConfig *nc)
	{
		_cb.virtualNetworkConfigFunction(reinterpret_cast<ZT_Node *>(this),_uPtr,tPtr,nwid,nuptr,op,nc);
	}

	/**
	 * @return True if node appears online
	 */
	ZT_ALWAYS_INLINE bool online() const { return _online; }

	/**
	 * Get a state object
	 *
	 * @param tPtr Thread pointer
	 * @param type Object type to get
	 * @param id Object ID
	 * @return Vector containing data or empty vector if not found or empty
	 */
	std::vector<uint8_t> stateObjectGet(void *const tPtr,ZT_StateObjectType type,const uint64_t id[2]);

	/**
	 * Store a state object
	 *
	 * @param tPtr Thread pointer
	 * @param type Object type to get
	 * @param id Object ID
	 * @param data Data to store
	 * @param len Length of data
	 */
	ZT_ALWAYS_INLINE void stateObjectPut(void *const tPtr,ZT_StateObjectType type,const uint64_t id[2],const void *const data,const unsigned int len)
	{
		if (_cb.statePutFunction)
			_cb.statePutFunction(reinterpret_cast<ZT_Node *>(this),_uPtr,tPtr,type,id,data,(int)len);
	}

	/**
	 * Delete a state object
	 *
	 * @param tPtr Thread pointer
	 * @param type Object type to delete
	 * @param id Object ID
	 */
	ZT_ALWAYS_INLINE void stateObjectDelete(void *const tPtr,ZT_StateObjectType type,const uint64_t id[2])
	{
		if (_cb.statePutFunction)
			_cb.statePutFunction(reinterpret_cast<ZT_Node *>(this),_uPtr,tPtr,type,id,(const void *)0,-1);
	}

	/**
	 * Check whether a path should be used for ZeroTier traffic
	 *
	 * This performs internal checks and also calls out to an external callback if one is defined.
	 *
	 * @param tPtr Thread pointer
	 * @param id Identity of peer
	 * @param localSocket Local socket or -1 if unknown
	 * @param remoteAddress Remote address
	 * @return True if path should be used
	 */
	bool shouldUsePathForZeroTierTraffic(void *tPtr,const Identity &id,int64_t localSocket,const InetAddress &remoteAddress);

	/**
	 * Query callback for a physical address for a peer
	 *
	 * @param tPtr Thread pointer
	 * @param id Full identity of ZeroTier node
	 * @param family Desired address family or -1 for any
	 * @param addr Buffer to store address (result paramter)
	 * @return True if addr was filled with something
	 */
	bool externalPathLookup(void *tPtr,const Identity &id,int family,InetAddress &addr);

	/**
	 * Set physical path configuration
	 *
	 * @param pathNetwork Physical path network/netmask bits (CIDR notation)
	 * @param pathConfig Path configuration
	 * @return Return to pass through to external API
	 */
	ZT_ResultCode setPhysicalPathConfiguration(const struct sockaddr_storage *pathNetwork,const ZT_PhysicalPathConfiguration *pathConfig);

	/**
	 * @return This node's identity
	 */
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
		const unsigned long pid2 = (unsigned long)(packetId >> 32U);
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

	/**
	 * Check whether a local controller has authorized a member on a network
	 *
	 * This is used by controllers to avoid needless certificate checks when we already
	 * know if this has occurred. It's a bit of a hack but saves a massive amount of
	 * controller CPU. It's easiest to put this here, and it imposes no overhead on
	 * non-controllers.
	 *
	 * @param now Current time
	 * @param nwid Network ID
	 * @param addr Member address to check
	 * @return True if member has been authorized
	 */
	bool localControllerHasAuthorized(int64_t now,uint64_t nwid,const Address &addr) const;

	// Implementation of NetworkController::Sender interface
	virtual void ncSendConfig(uint64_t nwid,uint64_t requestPacketId,const Address &destination,const NetworkConfig &nc,bool sendLegacyFormatConfig);
	virtual void ncSendRevocation(const Address &destination,const Revocation &rev);
	virtual void ncSendError(uint64_t nwid,uint64_t requestPacketId,const Address &destination,NetworkController::ErrorCode errorCode);

private:
	RuntimeEnvironment _RR;
	RuntimeEnvironment *RR;
	ZT_Node_Callbacks _cb;
	void *_uPtr; // _uptr (lower case) is reserved in Visual Studio :P

	// For tracking packet IDs to filter out OK/ERROR replies to packets we did not send
	volatile uint8_t _expectingRepliesToBucketPtr[ZT_EXPECTING_REPLIES_BUCKET_MASK1 + 1];
	volatile uint32_t _expectingRepliesTo[ZT_EXPECTING_REPLIES_BUCKET_MASK1 + 1][ZT_EXPECTING_REPLIES_BUCKET_MASK2 + 1];

	// Time of last identity verification indexed by InetAddress.rateGateHash() -- used in IncomingPacket::_doHELLO() via rateGateIdentityVerification()
	volatile int64_t _lastIdentityVerification[16384];

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

	// Networks are stored in a flat hash table that is resized on any network ID collision. This makes
	// network lookup by network ID a few bitwise ops and an array index.
	std::vector< SharedPtr<Network> > _networks;
	uint64_t _networksMask;

	std::vector< ZT_InterfaceAddress > _localInterfaceAddresses;

	Mutex _localControllerAuthorizations_m;
	RWMutex _networks_m;
	Mutex _localInterfaceAddresses_m;
	Mutex _backgroundTasksLock;

	volatile int64_t _now;
	volatile int64_t _lastPing;
	volatile int64_t _lastHousekeepingRun;
	volatile int64_t _lastNetworkHousekeepingRun;
	volatile int64_t _lastPathKeepaliveCheck;
	volatile bool _online;
};

} // namespace ZeroTier

#endif
