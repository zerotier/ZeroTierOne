/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2025-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_NETWORK_HPP
#define ZT_NETWORK_HPP

#include <stdint.h>

#include "../include/ZeroTierOne.h"

#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <stdexcept>

#include "Constants.hpp"
#include "Hashtable.hpp"
#include "Address.hpp"
#include "Mutex.hpp"
#include "SharedPtr.hpp"
#include "AtomicCounter.hpp"
#include "MulticastGroup.hpp"
#include "MAC.hpp"
#include "Dictionary.hpp"
#include "Multicaster.hpp"
#include "Membership.hpp"
#include "NetworkConfig.hpp"
#include "CertificateOfMembership.hpp"

#define ZT_NETWORK_MAX_INCOMING_UPDATES 3
#define ZT_NETWORK_MAX_UPDATE_CHUNKS ((ZT_NETWORKCONFIG_DICT_CAPACITY / 1024) + 1)

namespace ZeroTier {

class RuntimeEnvironment;
class Peer;

/**
 * A virtual LAN
 */
class Network
{
	friend class SharedPtr<Network>;

public:
	/**
	 * Broadcast multicast group: ff:ff:ff:ff:ff:ff / 0
	 */
	static const MulticastGroup BROADCAST;

	/**
	 * Compute primary controller device ID from network ID
	 */
	static inline Address controllerFor(uint64_t nwid) { return Address(nwid >> 24); }

	/**
	 * Construct a new network
	 *
	 * Note that init() should be called immediately after the network is
	 * constructed to actually configure the port.
	 *
	 * @param renv Runtime environment
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param nwid Network ID
	 * @param uptr Arbitrary pointer used by externally-facing API (for user use)
	 * @param nconf Network config, if known
	 */
	Network(const RuntimeEnvironment *renv,void *tPtr,uint64_t nwid,void *uptr,const NetworkConfig *nconf);

	~Network();

	inline uint64_t id() const { return _id; }
	inline Address controller() const { return Address(_id >> 24); }
	inline bool multicastEnabled() const { return (_config.multicastLimit > 0); }
	inline bool hasConfig() const { return (_config); }
	inline uint64_t lastConfigUpdate() const { return _lastConfigUpdate; }
	inline ZT_VirtualNetworkStatus status() const { Mutex::Lock _l(_lock); return _status(); }
	inline const NetworkConfig &config() const { return _config; }
	inline const MAC &mac() const { return _mac; }

	/**
	 * Apply filters to an outgoing packet
	 *
	 * This applies filters from our network config and, if that doesn't match,
	 * our capabilities in ascending order of capability ID. Additional actions
	 * such as TEE may be taken, and credentials may be pushed, so this is not
	 * side-effect-free. It's basically step one in sending something over VL2.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param noTee If true, do not TEE anything anywhere (for two-pass filtering as done with multicast and bridging)
	 * @param ztSource Source ZeroTier address
	 * @param ztDest Destination ZeroTier address
	 * @param macSource Ethernet layer source address
	 * @param macDest Ethernet layer destination address
	 * @param frameData Ethernet frame data
	 * @param frameLen Ethernet frame payload length
	 * @param etherType 16-bit ethernet type ID
	 * @param vlanId 16-bit VLAN ID
	 * @return True if packet should be sent, false if dropped or redirected
	 */
	bool filterOutgoingPacket(
		void *tPtr,
		const bool noTee,
		const Address &ztSource,
		const Address &ztDest,
		const MAC &macSource,
		const MAC &macDest,
		const uint8_t *frameData,
		const unsigned int frameLen,
		const unsigned int etherType,
		const unsigned int vlanId,
		uint8_t &qosBucket);

	/**
	 * Apply filters to an incoming packet
	 *
	 * This applies filters from our network config and, if that doesn't match,
	 * the peer's capabilities in ascending order of capability ID. If there is
	 * a match certain actions may be taken such as sending a copy of the packet
	 * to a TEE or REDIRECT target.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param sourcePeer Source Peer
	 * @param ztDest Destination ZeroTier address
	 * @param macSource Ethernet layer source address
	 * @param macDest Ethernet layer destination address
	 * @param frameData Ethernet frame data
	 * @param frameLen Ethernet frame payload length
	 * @param etherType 16-bit ethernet type ID
	 * @param vlanId 16-bit VLAN ID
	 * @return 0 == drop, 1 == accept, 2 == accept even if bridged
	 */
	int filterIncomingPacket(
		void *tPtr,
		const SharedPtr<Peer> &sourcePeer,
		const Address &ztDest,
		const MAC &macSource,
		const MAC &macDest,
		const uint8_t *frameData,
		const unsigned int frameLen,
		const unsigned int etherType,
		const unsigned int vlanId);

	/**
	 * Check whether we are subscribed to a multicast group
	 *
	 * @param mg Multicast group
	 * @param includeBridgedGroups If true, also check groups we've learned via bridging
	 * @return True if this network endpoint / peer is a member
	 */
	bool subscribedToMulticastGroup(const MulticastGroup &mg,bool includeBridgedGroups) const;

	/**
	 * Subscribe to a multicast group
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param mg New multicast group
	 */
	void multicastSubscribe(void *tPtr,const MulticastGroup &mg);

	/**
	 * Unsubscribe from a multicast group
	 *
	 * @param mg Multicast group
	 */
	void multicastUnsubscribe(const MulticastGroup &mg);

	/**
	 * Handle an inbound network config chunk
	 *
	 * This is called from IncomingPacket to handle incoming network config
	 * chunks via OK(NETWORK_CONFIG_REQUEST) or NETWORK_CONFIG. It verifies
	 * each chunk and once assembled applies the configuration.
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param packetId Packet ID or 0 if none (e.g. via cluster path)
	 * @param source Address of sender of chunk or NULL if none (e.g. via cluster path)
	 * @param chunk Buffer containing chunk
	 * @param ptr Index of chunk and related fields in packet
	 * @return Update ID if update was fully assembled and accepted or 0 otherwise
	 */
	uint64_t handleConfigChunk(void *tPtr,const uint64_t packetId,const Address &source,const Buffer<ZT_PROTO_MAX_PACKET_LENGTH> &chunk,unsigned int ptr);

	/**
	 * Set network configuration
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param nconf Network configuration
	 * @param saveToDisk Save to disk? Used during loading, should usually be true otherwise.
	 * @return 0 == bad, 1 == accepted but duplicate/unchanged, 2 == accepted and new
	 */
	int setConfiguration(void *tPtr,const NetworkConfig &nconf,bool saveToDisk);

	/**
	 * Set netconf failure to 'access denied' -- called in IncomingPacket when controller reports this
	 */
	inline void setAccessDenied()
	{
		Mutex::Lock _l(_lock);
		_netconfFailure = NETCONF_FAILURE_ACCESS_DENIED;
	}

	/**
	 * Set netconf failure to 'not found' -- called by IncomingPacket when controller reports this
	 */
	inline void setNotFound()
	{
		Mutex::Lock _l(_lock);
		_netconfFailure = NETCONF_FAILURE_NOT_FOUND;
	}

	/**
	 * Set netconf failure to 'authentication required' possibly with an authorization URL
	 */
	inline void setAuthenticationRequired(const char *url)
	{
		Mutex::Lock _l(_lock);
		_netconfFailure = NETCONF_FAILURE_AUTHENTICATION_REQUIRED;
		_authenticationURL = (url) ? url : "";
		_config.ssoEnabled = true;
	}	

	/**
	 * Causes this network to request an updated configuration from its master node now
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 */
	void requestConfiguration(void *tPtr);

	/**
	 * Determine whether this peer is permitted to communicate on this network
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param peer Peer to check
	 */
	bool gate(void *tPtr,const SharedPtr<Peer> &peer);

	/**
	 * Check whether a given peer has recently had an association with this network
	 *
	 * This checks whether a peer has communicated with us recently about this
	 * network and has possessed a valid certificate of membership. This may return
	 * true even if the peer has been offline for a while or no longer has a valid
	 * certificate of membership but had one recently.
	 *
	 * @param addr Peer address
	 * @return True if peer has recently associated
	 */
	bool recentlyAssociatedWith(const Address &addr);

	/**
	 * Do periodic cleanup and housekeeping tasks
	 */
	void clean();

	/**
	 * Push state to members such as multicast group memberships and latest COM (if needed)
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 */
	inline void sendUpdatesToMembers(void *tPtr)
	{
		Mutex::Lock _l(_lock);
		_sendUpdatesToMembers(tPtr,(const MulticastGroup *)0);
	}

	/**
	 * Find the node on this network that has this MAC behind it (if any)
	 *
	 * @param mac MAC address
	 * @return ZeroTier address of bridge to this MAC
	 */
	inline Address findBridgeTo(const MAC &mac) const
	{
		Mutex::Lock _l(_lock);
		const Address *const br = _remoteBridgeRoutes.get(mac);
		return ((br) ? *br : Address());
	}

	/**
	 * @return True if QoS is in effect for this network
	 */
	inline bool qosEnabled() { return false; }

	/**
	 * Set a bridge route
	 *
	 * @param mac MAC address of destination
	 * @param addr Bridge this MAC is reachable behind
	 */
	void learnBridgeRoute(const MAC &mac,const Address &addr);

	/**
	 * Learn a multicast group that is bridged to our tap device
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param mg Multicast group
	 * @param now Current time
	 */
	void learnBridgedMulticastGroup(void *tPtr,const MulticastGroup &mg,int64_t now);

	/**
	 * Validate a credential and learn it if it passes certificate and other checks
	 */
	Membership::AddCredentialResult addCredential(void *tPtr,const CertificateOfMembership &com);

	/**
	 * Validate a credential and learn it if it passes certificate and other checks
	 */
	inline Membership::AddCredentialResult addCredential(void *tPtr,const Capability &cap)
	{
		if (cap.networkId() != _id)
			return Membership::ADD_REJECTED;
		Mutex::Lock _l(_lock);
		return _membership(cap.issuedTo()).addCredential(RR,tPtr,_config,cap);
	}

	/**
	 * Validate a credential and learn it if it passes certificate and other checks
	 */
	inline Membership::AddCredentialResult addCredential(void *tPtr,const Tag &tag)
	{
		if (tag.networkId() != _id)
			return Membership::ADD_REJECTED;
		Mutex::Lock _l(_lock);
		return _membership(tag.issuedTo()).addCredential(RR,tPtr,_config,tag);
	}

	/**
	 * Validate a credential and learn it if it passes certificate and other checks
	 */
	Membership::AddCredentialResult addCredential(void *tPtr,const Address &sentFrom,const Revocation &rev);

	/**
	 * Validate a credential and learn it if it passes certificate and other checks
	 */
	inline Membership::AddCredentialResult addCredential(void *tPtr,const CertificateOfOwnership &coo)
	{
		if (coo.networkId() != _id)
			return Membership::ADD_REJECTED;
		Mutex::Lock _l(_lock);
		return _membership(coo.issuedTo()).addCredential(RR,tPtr,_config,coo);
	}

	/**
	 * Force push credentials (COM, etc.) to a peer now
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param to Destination peer address
	 * @param now Current time
	 */
	inline void pushCredentialsNow(void *tPtr,const Address &to,const int64_t now)
	{
		Mutex::Lock _l(_lock);
		_membership(to).pushCredentials(RR,tPtr,now,to,_config);
	}

	/**
	 * Push credentials if we haven't done so in a very long time
	 *
	 * @param tPtr Thread pointer to be handed through to any callbacks called as a result of this call
	 * @param to Destination peer address
	 * @param now Current time
	 */
	inline void pushCredentialsIfNeeded(void *tPtr,const Address &to,const int64_t now)
	{
		Mutex::Lock _l(_lock);
		Membership &m = _membership(to);
		if (m.shouldPushCredentials(now))
			m.pushCredentials(RR,tPtr,now,to,_config);
	}

	/**
	 * Destroy this network
	 *
	 * This sets the network to completely remove itself on delete. This also prevents the
	 * call of the normal port shutdown event on delete.
	 */
	void destroy();

	/**
	 * Get this network's config for export via the ZT core API
	 *
	 * @param ec Buffer to fill with externally-visible network configuration
	 */
	inline void externalConfig(ZT_VirtualNetworkConfig *ec) const
	{
		Mutex::Lock _l(_lock);
		_externalConfig(ec);
	}

	/**
	 * @return Externally usable pointer-to-pointer exported via the core API
	 */
	inline void **userPtr() { return &_uPtr; }

private:
	ZT_VirtualNetworkStatus _status() const;
	void _externalConfig(ZT_VirtualNetworkConfig *ec) const; // assumes _lock is locked
	bool _gate(const SharedPtr<Peer> &peer);
	void _sendUpdatesToMembers(void *tPtr,const MulticastGroup *const newMulticastGroup);
	void _announceMulticastGroupsTo(void *tPtr,const Address &peer,const std::vector<MulticastGroup> &allMulticastGroups);
	std::vector<MulticastGroup> _allMulticastGroups() const;
	Membership &_membership(const Address &a);

	const RuntimeEnvironment *const RR;
	void *_uPtr;
	const uint64_t _id;
	uint64_t _lastAnnouncedMulticastGroupsUpstream;
	MAC _mac; // local MAC address
	bool _portInitialized;

	std::vector< MulticastGroup > _myMulticastGroups; // multicast groups that we belong to (according to tap)
	Hashtable< MulticastGroup,uint64_t > _multicastGroupsBehindMe; // multicast groups that seem to be behind us and when we last saw them (if we are a bridge)
	Hashtable< MAC,Address > _remoteBridgeRoutes; // remote addresses where given MACs are reachable (for tracking devices behind remote bridges)

	NetworkConfig _config;
	uint64_t _lastConfigUpdate;

	struct _IncomingConfigChunk
	{
		_IncomingConfigChunk() { memset(this,0,sizeof(_IncomingConfigChunk)); }
		uint64_t ts;
		uint64_t updateId;
		uint64_t haveChunkIds[ZT_NETWORK_MAX_UPDATE_CHUNKS];
		unsigned long haveChunks;
		unsigned long haveBytes;
		Dictionary<ZT_NETWORKCONFIG_DICT_CAPACITY> data;
	};
	_IncomingConfigChunk _incomingConfigChunks[ZT_NETWORK_MAX_INCOMING_UPDATES];

	bool _destroyed;

	enum {
		NETCONF_FAILURE_NONE,
		NETCONF_FAILURE_ACCESS_DENIED,
		NETCONF_FAILURE_NOT_FOUND,
		NETCONF_FAILURE_INIT_FAILED,
		NETCONF_FAILURE_AUTHENTICATION_REQUIRED
	} _netconfFailure;
	int _portError; // return value from port config callback
	std::string _authenticationURL;

	Hashtable<Address,Membership> _memberships;

	Mutex _lock;

	AtomicCounter __refCount;
};

} // namespace ZeroTier

#endif
