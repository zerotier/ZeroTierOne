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

#include "Constants.hpp"
#include "RuntimeEnvironment.hpp"
#include "InetAddress.hpp"
#include "Mutex.hpp"
#include "MAC.hpp"
#include "Network.hpp"
#include "Path.hpp"
#include "Salsa20.hpp"
#include "NetworkController.hpp"
#include "Buf.hpp"
#include "Containers.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <map>

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

	// Get rid of alignment warnings on 32-bit Windows
#ifdef __WINDOWS__
	void * operator new(size_t i) { return _mm_malloc(i,16); }
	void operator delete(void* p) { _mm_free(p); }
#endif

	// Public API Functions ---------------------------------------------------------------------------------------------

	ZT_ResultCode processWirePacket(
		void *tPtr,
		int64_t now,
		int64_t localSocket,
		const struct sockaddr_storage *remoteAddress,
		SharedPtr<Buf> &packetData,
		unsigned int packetLength,
		volatile int64_t *nextBackgroundTaskDeadline);
	ZT_ResultCode processVirtualNetworkFrame(
		void *tPtr,
		int64_t now,
		uint64_t nwid,
		uint64_t sourceMac,
		uint64_t destMac,
		unsigned int etherType,
		unsigned int vlanId,
		SharedPtr<Buf> &frameData,
		unsigned int frameLength,
		volatile int64_t *nextBackgroundTaskDeadline);
	ZT_ResultCode processBackgroundTasks(void *tPtr, int64_t now, volatile int64_t *nextBackgroundTaskDeadline);
	ZT_ResultCode join(uint64_t nwid,const ZT_Fingerprint *controllerFingerprint,void *uptr,void *tptr);
	ZT_ResultCode leave(uint64_t nwid,void **uptr,void *tptr);
	ZT_ResultCode multicastSubscribe(void *tPtr,uint64_t nwid,uint64_t multicastGroup,unsigned long multicastAdi);
	ZT_ResultCode multicastUnsubscribe(uint64_t nwid,uint64_t multicastGroup,unsigned long multicastAdi);
	ZT_ResultCode addRoot(void *tptr,const void *rdef,unsigned int rdeflen);
	ZT_ResultCode removeRoot(void *tptr,const ZT_Fingerprint *fp);
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

	// Internal functions -----------------------------------------------------------------------------------------------

	/**
	 * @return Most recent time value supplied to core via API
	 */
	ZT_INLINE int64_t now() const noexcept { return m_now; }

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
	ZT_INLINE bool putPacket(void *tPtr,const int64_t localSocket,const InetAddress &addr,const void *data,unsigned int len,unsigned int ttl = 0) noexcept
	{
		return (m_cb.wirePacketSendFunction(
			reinterpret_cast<ZT_Node *>(this),
			m_uPtr,
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
	ZT_INLINE void putFrame(void *tPtr,uint64_t nwid,void **nuptr,const MAC &source,const MAC &dest,unsigned int etherType,unsigned int vlanId,const void *data,unsigned int len) noexcept
	{
		m_cb.virtualNetworkFrameFunction(
			reinterpret_cast<ZT_Node *>(this),
			m_uPtr,
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
	ZT_INLINE SharedPtr<Network> network(uint64_t nwid) const noexcept
	{
		RWMutex::RLock l(m_networks_l);
		const SharedPtr<Network> *const n = m_networks.get(nwid);
		if (n)
			return *n;
		return SharedPtr<Network>();
	}

	/**
	 * @return Known local interface addresses for this node
	 */
	ZT_INLINE Vector<ZT_InterfaceAddress> localInterfaceAddresses() const
	{
		Mutex::Lock _l(m_localInterfaceAddresses_m);
		return m_localInterfaceAddresses;
	}

	/**
	 * Post an event via external callback
	 *
	 * @param tPtr Thread pointer
	 * @param ev Event object
	 * @param md Event data or NULL if none
	 */
	ZT_INLINE void postEvent(void *tPtr,ZT_Event ev,const void *md = nullptr) noexcept
	{
		m_cb.eventCallback(reinterpret_cast<ZT_Node *>(this), m_uPtr, tPtr, ev, md);
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
	ZT_INLINE void configureVirtualNetworkPort(void *tPtr,uint64_t nwid,void **nuptr,ZT_VirtualNetworkConfigOperation op,const ZT_VirtualNetworkConfig *nc) noexcept
	{
		m_cb.virtualNetworkConfigFunction(reinterpret_cast<ZT_Node *>(this), m_uPtr, tPtr, nwid, nuptr, op, nc);
	}

	/**
	 * @return True if node appears online
	 */
	ZT_INLINE bool online() const noexcept { return m_online; }

	/**
	 * Get a state object
	 *
	 * @param tPtr Thread pointer
	 * @param type Object type to get
	 * @param id Object ID
	 * @return Vector containing data or empty vector if not found or empty
	 */
	Vector<uint8_t> stateObjectGet(void *tPtr,ZT_StateObjectType type,const uint64_t id[2]);

	/**
	 * Store a state object
	 *
	 * @param tPtr Thread pointer
	 * @param type Object type to get
	 * @param id Object ID
	 * @param data Data to store
	 * @param len Length of data
	 */
	ZT_INLINE void stateObjectPut(void *const tPtr,ZT_StateObjectType type,const uint64_t id[2],const void *const data,const unsigned int len) noexcept
	{
		if (m_cb.statePutFunction)
			m_cb.statePutFunction(reinterpret_cast<ZT_Node *>(this), m_uPtr, tPtr, type, id, data, (int)len);
	}

	/**
	 * Delete a state object
	 *
	 * @param tPtr Thread pointer
	 * @param type Object type to delete
	 * @param id Object ID
	 */
	ZT_INLINE void stateObjectDelete(void *const tPtr,ZT_StateObjectType type,const uint64_t id[2]) noexcept
	{
		if (m_cb.statePutFunction)
			m_cb.statePutFunction(reinterpret_cast<ZT_Node *>(this), m_uPtr, tPtr, type, id, nullptr, -1);
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
	ZT_INLINE const Identity &identity() const noexcept { return m_RR.identity; }

	/**
	 * @return True if aggressive NAT-traversal mechanisms like scanning of <1024 ports are enabled
	 */
	ZT_INLINE bool natMustDie() const noexcept { return m_natMustDie; }

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
	virtual void ncSendConfig(uint64_t nwid,uint64_t requestPacketId,const Address &destination,const NetworkConfig &nc,bool sendLegacyFormatConfig); // NOLINT(cppcoreguidelines-explicit-virtual-functions,hicpp-use-override,modernize-use-override)
	virtual void ncSendRevocation(const Address &destination,const Revocation &rev); // NOLINT(cppcoreguidelines-explicit-virtual-functions,hicpp-use-override,modernize-use-override)
	virtual void ncSendError(uint64_t nwid,uint64_t requestPacketId,const Address &destination,NetworkController::ErrorCode errorCode); // NOLINT(cppcoreguidelines-explicit-virtual-functions,hicpp-use-override,modernize-use-override)

private:
	RuntimeEnvironment m_RR;
	RuntimeEnvironment *const RR;

	// Pointer to a struct defined in Node that holds instances of core objects.
	void *m_objects;

	// Function pointers to C callbacks supplied via the API.
	ZT_Node_Callbacks m_cb;

	// A user-specified opaque pointer passed back via API callbacks.
	void *m_uPtr;

	// Cache that remembers whether or not the locally running network controller (if any) has authorized
	// someone on their most recent query. This is used by the network controller as a memoization optimization
	// to elide unnecessary signature verifications. It might get moved in the future since this is sort of a
	// weird place to put it.
	struct p_LocalControllerAuth
	{
		uint64_t nwid,address;
		ZT_INLINE p_LocalControllerAuth(const uint64_t nwid_, const Address &address_)  noexcept: nwid(nwid_), address(address_.toInt()) {}
		ZT_INLINE unsigned long hashCode() const noexcept { return (unsigned long)(nwid + address); }
		ZT_INLINE bool operator==(const p_LocalControllerAuth &a) const noexcept { return ((a.nwid == nwid) && (a.address == address)); }
		ZT_INLINE bool operator!=(const p_LocalControllerAuth &a) const noexcept { return ((a.nwid != nwid) || (a.address != address)); }
		ZT_INLINE bool operator<(const p_LocalControllerAuth &a) const noexcept { return ((a.nwid < nwid) || ((a.nwid == nwid) && (a.address < address))); }
	};
	Map<p_LocalControllerAuth,int64_t> m_localControllerAuthorizations;
	Mutex m_localControllerAuthorizations_l;

	// Locally joined networks by network ID.
	Map< uint64_t,SharedPtr<Network> > m_networks;
	RWMutex m_networks_l;

	// These are local interface addresses that have been configured via the API
	// and can be pushed to other nodes.
	Vector< ZT_InterfaceAddress > m_localInterfaceAddresses;
	Mutex m_localInterfaceAddresses_m;

	// This is locked while running processBackgroundTasks().
	Mutex m_backgroundTasksLock;

	// These are locked via _backgroundTasksLock as they're only checked and modified in processBackgroundTasks().
	int64_t m_lastPeerPulse;
	int64_t m_lastHousekeepingRun;
	int64_t m_lastNetworkHousekeepingRun;

	// This is the most recent value for time passed in via any of the core API methods.
	std::atomic<int64_t> m_now;

	// True if we are to use really intensive NAT-busting measures.
	std::atomic<bool> m_natMustDie;

	// True if at least one root appears reachable.
	std::atomic<bool> m_online;
};

} // namespace ZeroTier

#endif
