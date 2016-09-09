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
#include "NonCopyable.hpp"
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

namespace ZeroTier {

class RuntimeEnvironment;
class Peer;

/**
 * A virtual LAN
 */
class Network : NonCopyable
{
	friend class SharedPtr<Network>;

public:
	/**
	 * Broadcast multicast group: ff:ff:ff:ff:ff:ff / 0
	 */
	static const MulticastGroup BROADCAST;

	/**
	 * Construct a new network
	 *
	 * Note that init() should be called immediately after the network is
	 * constructed to actually configure the port.
	 *
	 * @param renv Runtime environment
	 * @param nwid Network ID
	 * @param uptr Arbitrary pointer used by externally-facing API (for user use)
	 */
	Network(const RuntimeEnvironment *renv,uint64_t nwid,void *uptr);

	~Network();

	/**
	 * Apply filters to an outgoing packet
	 *
	 * This applies filters from our network config and, if that doesn't match,
	 * our capabilities in ascending order of capability ID. Additional actions
	 * such as TEE may be taken, and credentials may be pushed.
	 *
	 * @param noTee If true, do not TEE anything anywhere
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
		const bool noTee,
		const Address &ztSource,
		const Address &ztDest,
		const MAC &macSource,
		const MAC &macDest,
		const uint8_t *frameData,
		const unsigned int frameLen,
		const unsigned int etherType,
		const unsigned int vlanId);

	/**
	 * Apply filters to an incoming packet
	 *
	 * This applies filters from our network config and, if that doesn't match,
	 * the peer's capabilities in ascending order of capability ID. If there is
	 * a match certain actions may be taken such as sending a copy of the packet
	 * to a TEE or REDIRECT target.
	 *
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
		const SharedPtr<Peer> &sourcePeer,
		const Address &ztDest,
		const MAC &macSource,
		const MAC &macDest,
		const uint8_t *frameData,
		const unsigned int frameLen,
		const unsigned int etherType,
		const unsigned int vlanId);

	/**
	 * @return Network ID
	 */
	inline uint64_t id() const throw() { return _id; }

	/**
	 * @return Address of network's controller (most significant 40 bits of ID)
	 */
	inline Address controller() const throw() { return Address(_id >> 24); }

	/**
	 * @param nwid Network ID
	 * @return Address of network's controller
	 */
	static inline Address controllerFor(uint64_t nwid) throw() { return Address(nwid >> 24); }

	/**
	 * @return Multicast group memberships for this network's port (local, not learned via bridging)
	 */
	inline std::vector<MulticastGroup> multicastGroups() const
	{
		Mutex::Lock _l(_lock);
		return _myMulticastGroups;
	}

	/**
	 * @return All multicast groups including learned groups that are behind any bridges we're attached to
	 */
	inline std::vector<MulticastGroup> allMulticastGroups() const
	{
		Mutex::Lock _l(_lock);
		return _allMulticastGroups();
	}

	/**
	 * @param mg Multicast group
	 * @param includeBridgedGroups If true, also include any groups we've learned via bridging
	 * @return True if this network endpoint / peer is a member
	 */
	bool subscribedToMulticastGroup(const MulticastGroup &mg,bool includeBridgedGroups) const;

	/**
	 * Subscribe to a multicast group
	 *
	 * @param mg New multicast group
	 */
	void multicastSubscribe(const MulticastGroup &mg);

	/**
	 * Unsubscribe from a multicast group
	 *
	 * @param mg Multicast group
	 */
	void multicastUnsubscribe(const MulticastGroup &mg);

	/**
	 * Apply a NetworkConfig to this network
	 *
	 * @param conf Configuration in NetworkConfig form
	 * @return True if configuration was accepted
	 */
	bool applyConfiguration(const NetworkConfig &conf);

	/**
	 * Set or update this network's configuration
	 *
	 * @param nconf Network configuration
	 * @param saveToDisk IF true (default), write config to disk
	 * @return 0 -- rejected, 1 -- accepted but not new, 2 -- accepted new config
	 */
	int setConfiguration(const NetworkConfig &nconf,bool saveToDisk);

	/**
	 * Handle an inbound network config chunk
	 *
	 * Only chunks whose inRePacketId matches the packet ID of the last request
	 * are handled. If this chunk completes the config, it is decoded and
	 * setConfiguration() is called.
	 *
	 * @param inRePacketId In-re packet ID from OK(NETWORK_CONFIG_REQUEST)
	 * @param data Chunk data
	 * @param chunkSize Size of data[]
	 * @param chunkIndex Index of chunk in full config
	 * @param totalSize Total size of network config
	 */
	void handleInboundConfigChunk(const uint64_t inRePacketId,const void *data,unsigned int chunkSize,unsigned int chunkIndex,unsigned int totalSize);

	/**
	 * Set netconf failure to 'access denied' -- called in IncomingPacket when controller reports this
	 */
	inline void setAccessDenied()
	{
		Mutex::Lock _l(_lock);
		_netconfFailure = NETCONF_FAILURE_ACCESS_DENIED;
	}

	/**
	 * Set netconf failure to 'not found' -- called by PacketDecider when controller reports this
	 */
	inline void setNotFound()
	{
		Mutex::Lock _l(_lock);
		_netconfFailure = NETCONF_FAILURE_NOT_FOUND;
	}

	/**
	 * Causes this network to request an updated configuration from its master node now
	 *
	 * There is a circuit breaker here to prevent this from being done more often
	 * than once per second. This is to prevent things like NETWORK_CONFIG_REFRESH
	 * from causing multiple requests.
	 */
	void requestConfiguration();

	/**
	 * Membership check gate for incoming packets related to this network
	 *
	 * @param peer Peer to check
	 * @param verb Packet verb
	 * @param packetId Packet ID
	 * @return True if peer is allowed to communicate on this network
	 */
	bool gate(const SharedPtr<Peer> &peer,const Packet::Verb verb,const uint64_t packetId);

	/**
	 * @param peer Peer to check
	 * @return True if peer has recently been a valid member of this network
	 */
	bool recentlyAllowedOnNetwork(const SharedPtr<Peer> &peer) const;

	/**
	 * Perform cleanup and possibly save state
	 */
	void clean();

	/**
	 * Push state to members such as multicast group memberships and latest COM (if needed)
	 */
	inline void pushStateToMembers()
	{
		Mutex::Lock _l(_lock);
		_pushStateToMembers((const MulticastGroup *)0);
	}

	/**
	 * @return Time of last updated configuration or 0 if none
	 */
	inline uint64_t lastConfigUpdate() const throw() { return _lastConfigUpdate; }

	/**
	 * @return Status of this network
	 */
	inline ZT_VirtualNetworkStatus status() const
	{
		Mutex::Lock _l(_lock);
		return _status();
	}

	/**
	 * @param ec Buffer to fill with externally-visible network configuration
	 */
	inline void externalConfig(ZT_VirtualNetworkConfig *ec) const
	{
		Mutex::Lock _l(_lock);
		_externalConfig(ec);
	}

	/**
	 * Get current network config
	 *
	 * @return Network configuration (may be a null config if we don't have one yet)
	 */
	inline const NetworkConfig &config() const { return _config; }

	/**
	 * @return True if this network has a valid config
	 */
	inline bool hasConfig() const { return (_config); }

	/**
	 * @return Ethernet MAC address for this network's local interface
	 */
	inline const MAC &mac() const { return _mac; }

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
		if (br)
			return *br;
		return Address();
	}

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
	 * @param mg Multicast group
	 * @param now Current time
	 */
	void learnBridgedMulticastGroup(const MulticastGroup &mg,uint64_t now);

	/**
	 * @param com Certificate of membership
	 * @return 0 == OK, 1 == waiting for WHOIS, -1 == BAD signature or credential
	 */
	inline int addCredential(const CertificateOfMembership &com)
	{
		if (com.networkId() != _id)
			return -1;
		Mutex::Lock _l(_lock);
		return _membership(com.issuedTo()).addCredential(RR,com);
	}

	/**
	 * @param cap Capability
	 * @return 0 == OK, 1 == waiting for WHOIS, -1 == BAD signature or credential
	 */
	inline int addCredential(const Capability &cap)
	{
		if (cap.networkId() != _id)
			return -1;
		Mutex::Lock _l(_lock);
		return _membership(cap.issuedTo()).addCredential(RR,cap);
	}

	/**
	 * @param cap Tag
	 * @return 0 == OK, 1 == waiting for WHOIS, -1 == BAD signature or credential
	 */
	inline int addCredential(const Tag &tag)
	{
		if (tag.networkId() != _id)
			return -1;
		Mutex::Lock _l(_lock);
		return _membership(tag.issuedTo()).addCredential(RR,tag);
	}

	/**
	 * Blacklist COM, tags, and capabilities before this time
	 *
	 * @param ts Blacklist cutoff
	 */
	inline void blacklistBefore(const Address &peerAddress,const uint64_t ts)
	{
		Mutex::Lock _l(_lock);
		_membership(peerAddress).blacklistBefore(ts);
	}

	/**
	 * Destroy this network
	 *
	 * This causes the network to disable itself, destroy its tap device, and on
	 * delete to delete all trace of itself on disk and remove any persistent tap
	 * device instances. Call this when a network is being removed from the system.
	 */
	void destroy();

	/**
	 * @return Pointer to user PTR (modifiable user ptr used in API)
	 */
	inline void **userPtr() throw() { return &_uPtr; }

private:
	ZT_VirtualNetworkStatus _status() const;
	void _externalConfig(ZT_VirtualNetworkConfig *ec) const; // assumes _lock is locked
	bool _gate(const SharedPtr<Peer> &peer);
	void _pushStateToMembers(const MulticastGroup *const newMulticastGroup);
	void _announceMulticastGroupsTo(const Address &peer,const std::vector<MulticastGroup> &allMulticastGroups);
	std::vector<MulticastGroup> _allMulticastGroups() const;
	Membership &_membership(const Address &a);

	const RuntimeEnvironment *RR;
	void *_uPtr;
	uint64_t _id;
	uint64_t _lastAnnouncedMulticastGroupsUpstream;
	MAC _mac; // local MAC address
	volatile bool _portInitialized;

	std::vector< MulticastGroup > _myMulticastGroups; // multicast groups that we belong to (according to tap)
	Hashtable< MulticastGroup,uint64_t > _multicastGroupsBehindMe; // multicast groups that seem to be behind us and when we last saw them (if we are a bridge)
	Hashtable< MAC,Address > _remoteBridgeRoutes; // remote addresses where given MACs are reachable (for tracking devices behind remote bridges)

	uint64_t _inboundConfigPacketId;
	std::map<unsigned int,std::string> _inboundConfigChunks;

	NetworkConfig _config;
	volatile uint64_t _lastConfigUpdate;
	volatile uint64_t _lastRequestedConfiguration;

	volatile bool _destroyed;

	enum {
		NETCONF_FAILURE_NONE,
		NETCONF_FAILURE_ACCESS_DENIED,
		NETCONF_FAILURE_NOT_FOUND,
		NETCONF_FAILURE_INIT_FAILED
	} _netconfFailure;
	volatile int _portError; // return value from port config callback

	Hashtable<Address,Membership> _memberships;

	Mutex _lock;

	AtomicCounter __refCount;
};

} // naemspace ZeroTier

#endif
