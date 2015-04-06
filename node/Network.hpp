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
#include "Utils.hpp"
#include "Address.hpp"
#include "Mutex.hpp"
#include "SharedPtr.hpp"
#include "AtomicCounter.hpp"
#include "MulticastGroup.hpp"
#include "MAC.hpp"
#include "Dictionary.hpp"
#include "BandwidthAccount.hpp"
#include "Multicaster.hpp"
#include "NetworkConfig.hpp"
#include "CertificateOfMembership.hpp"

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * A virtual LAN
 */
class Network : NonCopyable
{
	friend class SharedPtr<Network>;

public:
	/**
	 * @param renv Runtime environment
	 * @param nwid Network ID
	 */
	Network(const RuntimeEnvironment *renv,uint64_t nwid);

	~Network();

	/**
	 * Broadcast multicast group: ff:ff:ff:ff:ff:ff / 0
	 */
	static const MulticastGroup BROADCAST;

	/**
	 * @return Network ID
	 */
	inline uint64_t id() const throw() { return _id; }

	/**
	 * @return Address of network's netconf master (most significant 40 bits of ID)
	 */
	inline Address controller() throw() { return Address(_id >> 24); }

	/**
	 * @return Latest list of multicast groups for this network's tap
	 */
	inline std::vector<MulticastGroup> multicastGroups() const
	{
		Mutex::Lock _l(_lock);
		return _myMulticastGroups;
	}

	/**
	 * @param mg Multicast group
	 * @return True if this network endpoint / peer is a member
	 */
	bool subscribedToMulticastGroup(const MulticastGroup &mg) const
	{
		Mutex::Lock _l(_lock);
		return (std::find(_myMulticastGroups.begin(),_myMulticastGroups.end(),mg) != _myMulticastGroups.end());
	}

	/**
	 * Apply a NetworkConfig to this network
	 *
	 * @param conf Configuration in NetworkConfig form
	 * @return True if configuration was accepted
	 */
	bool applyConfiguration(const SharedPtr<NetworkConfig> &conf);

	/**
	 * Set or update this network's configuration
	 *
	 * This decodes a network configuration in key=value dictionary form,
	 * applies it if valid, and persists it to disk if saveToDisk is true.
	 *
	 * @param conf Configuration in key/value dictionary form
	 * @param saveToDisk IF true (default), write config to disk
	 * @return 0 -- rejected, 1 -- accepted but not new, 2 -- accepted new config
	 */
	int setConfiguration(const Dictionary &conf,bool saveToDisk = true);

	/**
	 * Set netconf failure to 'access denied' -- called in IncomingPacket when netconf master reports this
	 */
	inline void setAccessDenied()
	{
		Mutex::Lock _l(_lock);
		_netconfFailure = NETCONF_FAILURE_ACCESS_DENIED;
	}

	/**
	 * Set netconf failure to 'not found' -- called by PacketDecider when netconf master reports this
	 */
	inline void setNotFound()
	{
		Mutex::Lock _l(_lock);
		_netconfFailure = NETCONF_FAILURE_NOT_FOUND;
	}

	/**
	 * Causes this network to request an updated configuration from its master node now
	 */
	void requestConfiguration();

	/**
	 * Add or update a membership certificate
	 *
	 * @param cert Certificate of membership
	 * @param forceAccept If true, accept without validating signature
	 */
	void addMembershipCertificate(const CertificateOfMembership &cert,bool forceAccept);

	/**
	 * Check if we should push membership certificate to a peer, and update last pushed
	 *
	 * If we haven't pushed a cert to this peer in a long enough time, this returns
	 * true and updates the last pushed time. Otherwise it returns false.
	 *
	 * This doesn't actually send anything, since COMs can hitch a ride with several
	 * different kinds of packets.
	 *
	 * @param to Destination peer
	 * @param now Current time
	 * @return True if we should include a COM with whatever we're currently sending
	 */
	bool peerNeedsOurMembershipCertificate(const Address &to,uint64_t now);

	/**
	 * @param peer Peer address to check
	 * @return True if peer is allowed to communicate on this network
	 */
	bool isAllowed(const Address &peer) const;

	/**
	 * Perform cleanup and possibly save state
	 */
	void clean();

	/**
	 * @return Time of last updated configuration or 0 if none
	 */
	inline uint64_t lastConfigUpdate() const throw() { return _lastConfigUpdate; }

	/**
	 * @return Status of this network
	 */
	inline ZT1_VirtualNetworkStatus status() const
	{
		Mutex::Lock _l(_lock);
		return _status();
	}

	/**
	 * @param ec Buffer to fill with externally-visible network configuration
	 */
	inline void externalConfig(ZT1_VirtualNetworkConfig *ec) const
	{
		Mutex::Lock _l(_lock);
		_externalConfig(ec);
	}

	/**
	 * Update and check multicast rate balance for a multicast group
	 *
	 * @param mg Multicast group
	 * @param bytes Size of packet
	 * @return True if packet is within budget
	 */
	inline bool updateAndCheckMulticastBalance(const MulticastGroup &mg,unsigned int bytes)
	{
		Mutex::Lock _l(_lock);
		if (!_config)
			return false;
		std::map< MulticastGroup,BandwidthAccount >::iterator bal(_multicastRateAccounts.find(mg));
		if (bal == _multicastRateAccounts.end()) {
			NetworkConfig::MulticastRate r(_config->multicastRate(mg));
			bal = _multicastRateAccounts.insert(std::pair< MulticastGroup,BandwidthAccount >(mg,BandwidthAccount(r.preload,r.maxBalance,r.accrual))).first;
		}
		return bal->second.deduct(bytes);
	}

	/**
	 * Get current network config or throw exception
	 *
	 * This version never returns null. Instead it throws a runtime error if
	 * there is no current configuration. Callers should check isUp() first or
	 * use config2() to get with the potential for null.
	 *
	 * Since it never returns null, it's safe to config()->whatever() inside
	 * a try/catch block.
	 *
	 * @return Network configuration (never null)
	 * @throws std::runtime_error Network configuration unavailable
	 */
	inline SharedPtr<NetworkConfig> config() const
	{
		Mutex::Lock _l(_lock);
		if (_config)
			return _config;
		throw std::runtime_error("no configuration");
	}

	/**
	 * Get current network config or return NULL
	 *
	 * @return Network configuration -- may be NULL
	 */
	inline SharedPtr<NetworkConfig> config2() const
		throw()
	{
		Mutex::Lock _l(_lock);
		return _config;
	}

	/**
	 * @return Ethernet MAC address for this network's local interface
	 */
	inline const MAC &mac() const throw() { return _mac; }

	/**
	 * Shortcut for config()->permitsBridging(), returns false if no config
	 *
	 * @param peer Peer address to check
	 * @return True if peer can bridge other Ethernet nodes into this network or network is in permissive bridging mode
	 */
	inline bool permitsBridging(const Address &peer) const
	{
		Mutex::Lock _l(_lock);
		if (_config)
			return _config->permitsBridging(peer);
		return false;
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
		std::map<MAC,Address>::const_iterator br(_remoteBridgeRoutes.find(mac));
		if (br == _remoteBridgeRoutes.end())
			return Address();
		return br->second;
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
	inline void learnBridgedMulticastGroup(const MulticastGroup &mg,uint64_t now)
	{
		Mutex::Lock _l(_lock);
		_multicastGroupsBehindMe[mg] = now;
	}

	/**
	 * @return True if traffic on this network's tap is enabled
	 */
	inline bool enabled() const throw() { return _enabled; }

	/**
	 * @param enabled Should traffic be allowed on this network?
	 */
	void setEnabled(bool enabled);

	/**
	 * Destroy this network
	 *
	 * This causes the network to disable itself, destroy its tap device, and on
	 * delete to delete all trace of itself on disk and remove any persistent tap
	 * device instances. Call this when a network is being removed from the system.
	 */
	void destroy();

private:
	ZT1_VirtualNetworkStatus _status() const;
	void _externalConfig(ZT1_VirtualNetworkConfig *ec) const; // assumes _lock is locked

	const RuntimeEnvironment *RR;
	uint64_t _id;
	MAC _mac; // local MAC address
	volatile bool _enabled;

	std::vector< MulticastGroup > _myMulticastGroups; // multicast groups that we belong to including those behind us (updated periodically)
	std::map< MulticastGroup,uint64_t > _multicastGroupsBehindMe; // multicast groups bridged to us and when we last saw activity on each
	std::map< MulticastGroup,BandwidthAccount > _multicastRateAccounts;

	std::map<MAC,Address> _remoteBridgeRoutes; // remote addresses where given MACs are reachable

	std::map<Address,CertificateOfMembership> _membershipCertificates; // Other members' certificates of membership
	std::map<Address,uint64_t> _lastPushedMembershipCertificate; // When did we last push our certificate to each remote member?

	SharedPtr<NetworkConfig> _config; // Most recent network configuration, which is an immutable value-object
	volatile uint64_t _lastConfigUpdate;

	volatile bool _destroyed;

	enum {
		NETCONF_FAILURE_NONE,
		NETCONF_FAILURE_ACCESS_DENIED,
		NETCONF_FAILURE_NOT_FOUND,
		NETCONF_FAILURE_INIT_FAILED
	} _netconfFailure;
	int _portError; // return value from port config callback

	Mutex _lock;

	AtomicCounter __refCount;
};

} // naemspace ZeroTier

#endif
