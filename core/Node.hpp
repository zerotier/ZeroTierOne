/*
 * Copyright (c)2013-2021 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_NODE_HPP
#define ZT_NODE_HPP

#include "Constants.hpp"
#include "Context.hpp"
#include "InetAddress.hpp"
#include "Mutex.hpp"
#include "MAC.hpp"
#include "Network.hpp"
#include "Path.hpp"
#include "Salsa20.hpp"
#include "NetworkController.hpp"
#include "Buf.hpp"
#include "Containers.hpp"
#include "Store.hpp"
#include "CallContext.hpp"

namespace ZeroTier {

/**
 * Implementation of Node object as defined in CAPI
 *
 * The pointer returned by ZT_Node_new() is an instance of this class.
 */
class Node : public NetworkController::Sender
{
public:
	// Get rid of alignment warnings on 32-bit Windows
#ifdef __WINDOWS__
	void * operator new(size_t i) { return _mm_malloc(i,16); }
	void operator delete(void* p) { _mm_free(p); }
#endif

	Node(void *uPtr, const struct ZT_Node_Callbacks *callbacks, const CallContext &cc);

	virtual ~Node();

	void shutdown(const CallContext &cc);

	ZT_ResultCode processBackgroundTasks(
		const CallContext &cc,
		volatile int64_t *nextBackgroundTaskDeadline);

	ZT_ResultCode join(
		uint64_t nwid,
		const ZT_Fingerprint *controllerFingerprint,
		void *uptr,
		const CallContext &cc);

	ZT_ResultCode leave(
		uint64_t nwid,
		void **uptr,
		const CallContext &cc);

	ZT_ResultCode multicastSubscribe(
		const CallContext &cc,
		uint64_t nwid,
		uint64_t multicastGroup,
		unsigned long multicastAdi);

	ZT_ResultCode multicastUnsubscribe(
		const CallContext &cc,
		uint64_t nwid,
		uint64_t multicastGroup,
		unsigned long multicastAdi);

	void status(
		ZT_NodeStatus *status) const;

	ZT_PeerList *peers(
		const CallContext &cc) const;

	ZT_VirtualNetworkConfig *networkConfig(
		uint64_t nwid) const;

	ZT_VirtualNetworkList *networks() const;

	void setNetworkUserPtr(
		uint64_t nwid,
		void *ptr);

	void setInterfaceAddresses(
		const ZT_InterfaceAddress *addrs,
		unsigned int addrCount);

	ZT_CertificateError addCertificate(
		const CallContext &cc,
		unsigned int localTrust,
		const ZT_Certificate *cert,
		const void *certData,
		unsigned int certSize);

	ZT_ResultCode deleteCertificate(
		const CallContext &cc,
		const void *serialNo);

	ZT_CertificateList *listCertificates();

	int sendUserMessage(
		const CallContext &cc,
		uint64_t dest,
		uint64_t typeId,
		const void *data,
		unsigned int len);

	void setController(
		void *networkControllerInstance);

	/**
	 * Post an event via external callback
	 *
	 * @param tPtr Thread pointer
	 * @param ev Event object
	 * @param md Event data or NULL if none
	 * @param mdSize Size of event data
	 */
	ZT_INLINE void postEvent(void *tPtr, ZT_Event ev, const void *md = nullptr, const unsigned int mdSize = 0) noexcept
	{ m_ctx.cb.eventCallback(reinterpret_cast<ZT_Node *>(this), m_ctx.uPtr, tPtr, ev, md, mdSize); }

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
	bool shouldUsePathForZeroTierTraffic(void *tPtr, const Identity &id, int64_t localSocket, const InetAddress &remoteAddress);

	/**
	 * Query callback for a physical address for a peer
	 *
	 * @param tPtr Thread pointer
	 * @param id Full identity of ZeroTier node
	 * @param family Desired address family or -1 for any
	 * @param addr Buffer to store address (result paramter)
	 * @return True if addr was filled with something
	 */
	bool externalPathLookup(void *tPtr, const Identity &id, int family, InetAddress &addr);

	ZT_INLINE const Identity &identity() const noexcept
	{ return m_ctx.identity; }

	ZT_INLINE const Context &context() const noexcept
	{ return m_ctx; }

	// Implementation of NetworkController::Sender interface
	virtual void ncSendConfig(void *tPtr, int64_t clock, int64_t ticks, uint64_t nwid, uint64_t requestPacketId, const Address &destination, const NetworkConfig &nc, bool sendLegacyFormatConfig);
	virtual void ncSendRevocation(void *tPtr, int64_t clock, int64_t ticks, const Address &destination, const RevocationCredential &rev);
	virtual void ncSendError(void *tPtr, int64_t clock, int64_t ticks, uint64_t nwid, uint64_t requestPacketId, const Address &destination, NetworkController::ErrorCode errorCode);

private:
	Context m_ctx;

	// Data store wrapper
	Store m_store;

	// Pointer to a struct defined in Node that holds instances of core objects.
	void *m_objects;

	// This stores networks for rapid iteration, while RR->networks is the primary lookup.
	Vector< SharedPtr< Network > > m_allNetworks;
	Mutex m_allNetworks_l;

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
	int64_t m_lastTrustStoreUpdate;

	// True if at least one root appears reachable.
	std::atomic< bool > m_online;
};

} // namespace ZeroTier

#endif
