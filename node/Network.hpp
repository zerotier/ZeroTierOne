/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2011-2014  ZeroTier Networks LLC
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

#include <string>
#include <set>
#include <map>
#include <vector>
#include <algorithm>
#include <stdexcept>

#include "Constants.hpp"
#include "NonCopyable.hpp"
#include "Utils.hpp"
#include "EthernetTap.hpp"
#include "Address.hpp"
#include "Mutex.hpp"
#include "SharedPtr.hpp"
#include "AtomicCounter.hpp"
#include "MulticastGroup.hpp"
#include "MAC.hpp"
#include "Dictionary.hpp"
#include "Identity.hpp"
#include "InetAddress.hpp"
#include "BandwidthAccount.hpp"
#include "MulticastTopology.hpp"
#include "NetworkConfig.hpp"
#include "CertificateOfMembership.hpp"
#include "Thread.hpp"

namespace ZeroTier {

class RuntimeEnvironment;
class NodeConfig;

/**
 * A virtual LAN
 *
 * Networks can be open or closed. Each network has an ID whose most
 * significant 40 bits are the ZeroTier address of the node that should
 * be contacted for network configuration. The least significant 24
 * bits are arbitrary, allowing up to 2^24 networks per managing
 * node.
 *
 * Open networks do not track membership. Anyone is allowed to communicate
 * over them. For closed networks, each peer must distribute a certificate
 * regularly that proves that they are allowed to communicate.
 */
class Network : NonCopyable
{
	friend class SharedPtr<Network>;
	friend class NodeConfig;

private:
	// Only NodeConfig can create, only SharedPtr can delete

	// Actual construction happens in newInstance()
	Network() throw() {}
	~Network();

	/**
	 * Create a new Network instance and restore any saved state
	 *
	 * If there is no saved state, a dummy .conf is created on disk to remember
	 * this network across restarts.
	 *
	 * @param renv Runtime environment
	 * @param nc Parent NodeConfig
	 * @param id Network ID
	 * @return Reference counted pointer to new network
	 * @throws std::runtime_error Unable to create tap device or other fatal error
	 */
	static SharedPtr<Network> newInstance(const RuntimeEnvironment *renv,NodeConfig *nc,uint64_t id);

public:
	/**
	 * Broadcast multicast group: ff:ff:ff:ff:ff:ff / 0
	 */
	static const MulticastGroup BROADCAST;

	/**
	 * Possible network states
	 */
	enum Status
	{
		NETWORK_INITIALIZING = 0,               // Creating tap device and setting up state
		NETWORK_WAITING_FOR_FIRST_AUTOCONF = 1, // Waiting for initial setup with netconf master
		NETWORK_OK = 2,                         // Network is up, seems to be working
		NETWORK_ACCESS_DENIED = 3,              // Netconf node reported permission denied
		NETWORK_NOT_FOUND = 4,                  // Netconf node reported network not found
		NETWORK_INITIALIZATION_FAILED = 5,      // Cannot initialize device (OS/installation problem?)
		NETWORK_NO_MORE_DEVICES = 6             // OS cannot create any more tap devices (some OSes have a limit)
	};

	/**
	 * @param s Status
	 * @return String description
	 */
	static const char *statusString(const Status s)
		throw();

	/**
	 * @return Network ID
	 */
	inline uint64_t id() const throw() { return _id; }

	/**
	 * @return Address of network's netconf master (most significant 40 bits of ID)
	 */
	inline Address controller() throw() { return Address(_id >> 24); }

	/**
	 * @return Network ID in hexadecimal form
	 */
	inline std::string idString()
	{
		char buf[64];
		Utils::snprintf(buf,sizeof(buf),"%.16llx",(unsigned long long)_id);
		return std::string(buf);
	}

	/**
	 * Update multicast groups for this network's tap
	 *
	 * @return True if internal multicast group set has changed since last update
	 */
	bool updateMulticastGroups();

	/**
	 * @return Latest set of multicast groups for this network's tap
	 */
	inline std::set<MulticastGroup> multicastGroups() const
	{
		Mutex::Lock _l(_lock);
		return _myMulticastGroups;
	}

	/**
	 * Set or update this network's configuration
	 *
	 * This is called in IncomingPacket when an update comes over the wire, or
	 * internally when an old config is reloaded from disk.
	 *
	 * This also cancels any netconf failure flags.
	 *
	 * The network can't accept configuration when in INITIALIZATION state,
	 * and so in that state this will just return false.
	 *
	 * @param conf Configuration in key/value dictionary form
	 * @param saveToDisk IF true (default), write config to disk
	 * @return True if configuration was accepted, false if still initializing or config was not valid
	 */
	bool setConfiguration(const Dictionary &conf,bool saveToDisk = true);

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
	 * This cert must have been signature checked first. Certs older than the
	 * cert on file are ignored and the newer cert remains in the database.
	 *
	 * @param cert Certificate of membership
	 */
	void addMembershipCertificate(const CertificateOfMembership &cert);

	/**
	 * Push our membership certificate to a peer
	 *
	 * @param peer Destination peer address
	 * @param force If true, push even if we've already done so within required time frame
	 * @param now Current time
	 */
	inline void pushMembershipCertificate(const Address &peer,bool force,uint64_t now)
	{
		Mutex::Lock _l(_lock);
		if ((_config)&&(!_config->isPublic())&&(_config->com()))
			_pushMembershipCertificate(peer,force,now);
	}

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
	Status status() const;

	/**
	 * Update and check multicast rate balance for a group
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
	 * Thread main method; do not call elsewhere
	 */
	void threadMain()
		throw();

	/**
	 * Inject a frame into tap (if it's created and network is enabled)
	 *
	 * @param from Origin MAC
	 * @param to Destination MC
	 * @param etherType Ethernet frame type
	 * @param data Frame data
	 * @param len Frame length
	 */
	inline void tapPut(const MAC &from,const MAC &to,unsigned int etherType,const void *data,unsigned int len)
	{
		if (!_enabled)
			return;
		EthernetTap *t = _tap;
		if (t)
			t->put(from,to,etherType,data,len);
	}

	/**
	 * @return Tap device name or empty string if still initializing
	 */
	inline std::string tapDeviceName() const
	{
		EthernetTap *t = _tap;
		if (t)
			return t->deviceName();
		else return std::string();
	}

	/**
	 * @return Ethernet MAC address for this network's local interface
	 */
	inline const MAC &mac() const
		throw()
	{
		return _mac;
	}

	/**
	 * @return Set of IPs currently assigned to interface
	 */
	inline std::set<InetAddress> ips() const
	{
		EthernetTap *t = _tap;
		if (t)
			return t->ips();
		return std::set<InetAddress>();
	}

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
	 * @param mac MAC address
	 * @return ZeroTier address of bridge to this MAC or null address if not found (also check result for self, since this can happen)
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
	 * @return Multicast topology for this network
	 */
	inline MulticastTopology &mcTopology() { return _multicastTopology; }
	inline const MulticastTopology &mcTopology() const { return _multicastTopology; }

	/**
	 * Destroy this network
	 *
	 * This causes the network to disable itself, destroy its tap device, and on
	 * delete to delete all trace of itself on disk and remove any persistent tap
	 * device instances. Call this when a network is being removed from the system.
	 */
	void destroy();

private:
	static void _CBhandleTapData(void *arg,const MAC &from,const MAC &to,unsigned int etherType,const Buffer<4096> &data);

	void _pushMembershipCertificate(const Address &peer,bool force,uint64_t now);
	void _restoreState();
	void _dumpMulticastCerts();

	inline void _mkNetworkFriendlyName(char *buf,unsigned int len)
	{
		// assumes _lock is locked
		if (_config)
			Utils::snprintf(buf,len,"ZeroTier One [%s]",_config->name().c_str());
		else Utils::snprintf(buf,len,"ZeroTier One [%.16llx]",(unsigned long long)_id);
	}

	uint64_t _id;
	NodeConfig *_nc; // parent NodeConfig object
	MAC _mac; // local MAC address
	const RuntimeEnvironment *RR;
	EthernetTap *volatile _tap; // tap device or NULL if not initialized yet
	volatile bool _enabled;

	std::set< MulticastGroup > _myMulticastGroups; // multicast groups that we belong to including those behind us (updated periodically)
	std::map< MulticastGroup,uint64_t > _multicastGroupsBehindMe; // multicast groups bridged to us and when we last saw activity on each
	std::map< MulticastGroup,BandwidthAccount > _multicastRateAccounts;
	MulticastTopology _multicastTopology;

	std::map<MAC,Address> _remoteBridgeRoutes; // remote addresses where given MACs are reachable

	std::map<Address,CertificateOfMembership> _membershipCertificates; // Other members' certificates of membership
	std::map<Address,uint64_t> _lastPushedMembershipCertificate; // When did we last push our certificate to each remote member?

	SharedPtr<NetworkConfig> _config; // Most recent network configuration, which is an immutable value-object
	volatile uint64_t _lastConfigUpdate;

	volatile bool _destroyed;

	volatile enum {
		NETCONF_FAILURE_NONE,
		NETCONF_FAILURE_ACCESS_DENIED,
		NETCONF_FAILURE_NOT_FOUND,
		NETCONF_FAILURE_INIT_FAILED
	} _netconfFailure;

	Thread _setupThread;

	Mutex _lock;

	AtomicCounter __refCount;
};

} // naemspace ZeroTier

#endif
