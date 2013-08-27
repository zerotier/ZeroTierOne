/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
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

#ifndef _ZT_NETWORK_HPP
#define _ZT_NETWORK_HPP

#include <string>
#include <set>
#include <map>
#include <vector>
#include <stdexcept>

#include "Constants.hpp"
#include "Utils.hpp"
#include "EthernetTap.hpp"
#include "Address.hpp"
#include "Mutex.hpp"
#include "SharedPtr.hpp"
#include "AtomicCounter.hpp"
#include "MulticastGroup.hpp"
#include "NonCopyable.hpp"
#include "MAC.hpp"
#include "Dictionary.hpp"
#include "Identity.hpp"
#include "InetAddress.hpp"
#include "RateLimiter.hpp"

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
 * over them.
 *
 * Closed networks track membership by way of timestamped signatures. When
 * the network requests its configuration, one of the fields returned is
 * a signature for the identity of the peer on the network. This signature
 * includes a timestamp. When a peer communicates with other peers on a
 * closed network, it periodically (and pre-emptively) propagates this
 * signature to the peers with which it is communicating. Peers reject
 * packets with an error if no recent signature is on file.
 */
class Network : NonCopyable
{
	friend class SharedPtr<Network>;
	friend class NodeConfig;

public:
	/**
	 * A certificate of network membership for private network participation
	 */
	class Certificate : private Dictionary
	{
	public:
		Certificate()
		{
		}

		Certificate(const char *s) :
			Dictionary(s)
		{
		}

		Certificate(const std::string &s) :
			Dictionary(s)
		{
		}

		inline std::string toString() const
		{
			return Dictionary::toString();
		}

		inline void setNetworkId(uint64_t id)
		{
			char buf[32];
			sprintf(buf,"%.16llx",(unsigned long long)id);
			(*this)["nwid"] = buf;
		}

		inline uint64_t networkId() const
			throw(std::invalid_argument)
		{
#ifdef __WINDOWS__
			return _strtoui64(get("nwid").c_str(),(char **)0,16);
#else
			return strtoull(get("nwid").c_str(),(char **)0,16);
#endif
		}

		inline void setPeerAddress(Address &a)
		{
			(*this)["peer"] = a.toString();
		}

		inline Address peerAddress() const
			throw(std::invalid_argument)
		{
			return Address(get("peer"));
		}

		/**
		 * Set the timestamp and timestamp max-delta
		 *
		 * @param ts Timestamp in ms since epoch
		 * @param maxDelta Maximum difference between two peers on the same network
		 */
		inline void setTimestamp(uint64_t ts,uint64_t maxDelta)
		{
			char foo[32];
			sprintf(foo,"%llu",(unsigned long long)ts);
			(*this)["ts"] = foo;
			sprintf(foo,"%llu",(unsigned long long)maxDelta);
			(*this)["~ts"] = foo;
		}

		/**
		 * Sign this certificate
		 *
		 * @param with Signing identity -- the identity of this network's controller
		 * @return Signature or empty string on failure
		 */
		inline std::string sign(const Identity &with) const
		{
			unsigned char dig[32];
			_shaForSignature(dig);
			return with.sign(dig);
		}

		/**
		 * Verify this certificate's signature
		 *
		 * @param with Signing identity -- the identity of this network's controller
		 * @param sig Signature
		 * @param siglen Length of signature in bytes
		 */
		inline bool verify(const Identity &with,const void *sig,unsigned int siglen) const
		{
			unsigned char dig[32];
			_shaForSignature(dig);
			return with.verifySignature(dig,sig,siglen);
		}

		/**
		 * Check if another peer is indeed a current member of this network
		 *
		 * Fields with companion ~fields are compared with the defined maximum
		 * delta in this certificate. Fields without ~fields are compared for
		 * equality.
		 *
		 * This does not verify the certificate's signature!
		 * 
		 * @param mc Peer membership certificate
		 * @return True if mc's membership in this network is current
		 */
		bool qualifyMembership(const Certificate &mc) const;

	private:
		void _shaForSignature(unsigned char *dig) const;
	};

	/**
	 * A network configuration for a given node
	 */
	class Config : private Dictionary
	{
	public:
		Config()
		{
		}

		Config(const char *s) :
			Dictionary(s)
		{
		}

		Config(const std::string &s) :
			Dictionary(s)
		{
		}

		inline bool containsAllFields() const
		{
			return (contains("nwid")&&contains("peer"));
		}

		inline std::string toString() const
		{
			return Dictionary::toString();
		}

		inline uint64_t networkId() const
			throw(std::invalid_argument)
		{
#ifdef __WINDOWS__
			return _strtoui64(get("nwid").c_str(),(char **)0,16);
#else
			return strtoull(get("nwid").c_str(),(char **)0,16);
#endif
		}

		inline std::string name() const
		{
			if (contains("name"))
				return get("name");
			char buf[32];
			sprintf(buf,"%.16llx",(unsigned long long)networkId());
			return std::string(buf);
		}

		inline Address peerAddress() const
			throw(std::invalid_argument)
		{
			return Address(get("peer"));
		}

		/**
		 * @return Certificate of membership for this network, or empty cert if none
		 */
		inline Certificate certificateOfMembership() const
		{
			const_iterator cm(find("com"));
			if (cm == end())
				return Certificate();
			else return Certificate(cm->second);
		}

		/**
		 * @return True if this is an open non-access-controlled network
		 */
		inline bool isOpen() const
		{
			return (get("isOpen","0") == "1");
		}

		/**
		 * @return All static addresses / netmasks, IPv4 or IPv6
		 */
		inline std::set<InetAddress> staticAddresses() const
		{
			std::set<InetAddress> sa;
			std::vector<std::string> ips(Utils::split(get("ipv4Static","").c_str(),",","",""));
			for(std::vector<std::string>::const_iterator i(ips.begin());i!=ips.end();++i)
				sa.insert(InetAddress(*i));
			ips = Utils::split(get("ipv6Static","").c_str(),",","","");
			for(std::vector<std::string>::const_iterator i(ips.begin());i!=ips.end();++i)
				sa.insert(InetAddress(*i));
			return sa;
		}
	};

	/**
	 * Status for networks
	 */
	enum Status
	{
		NETWORK_WAITING_FOR_FIRST_AUTOCONF,
		NETWORK_OK,
		NETWORK_ACCESS_DENIED
	};

	/**
	 * @param s Status
	 * @return String description
	 */
	static const char *statusString(const Status s)
		throw();

private:
	// Only NodeConfig can create, only SharedPtr can delete

	// Actual construction happens in newInstance()
	Network()
		throw() :
		_tap((EthernetTap *)0)
	{
	}

	~Network();

	/**
	 * Create a new Network instance and restore any saved state
	 *
	 * If there is no saved state, a dummy .conf is created on disk to remember
	 * this network across restarts.
	 *
	 * @param renv Runtime environment
	 * @param id Network ID
	 * @return Reference counted pointer to new network
	 * @throws std::runtime_error Unable to create tap device or other fatal error
	 */
	static SharedPtr<Network> newInstance(const RuntimeEnvironment *renv,uint64_t id)
		throw(std::runtime_error);

	/**
	 * Causes all persistent disk presence to be erased on delete
	 */
	inline void destroyOnDelete()
	{
		_destroyOnDelete = true;
	}

public:
	/**
	 * @return Network ID
	 */
	inline uint64_t id() const throw() { return _id; }

	/**
	 * @return Ethernet tap
	 */
	inline EthernetTap &tap() throw() { return *_tap; }

	/**
	 * @return Address of network's controlling node
	 */
	inline Address controller() throw() { return Address(_id >> 24); }

	/**
	 * @return Network ID in hexadecimal form
	 */
	inline std::string toString()
	{
		char buf[64];
		sprintf(buf,"%.16llx",(unsigned long long)_id);
		return std::string(buf);
	}

	/**
	 * @return True if network is open (no membership required)
	 */
	inline bool isOpen() const
		throw()
	{
		try {
			Mutex::Lock _l(_lock);
			return _configuration.isOpen();
		} catch ( ... ) {
			return false;
		}
	}

	/**
	 * Update multicast groups for this network's tap
	 *
	 * @return True if internal multicast group set has changed
	 */
	inline bool updateMulticastGroups()
	{
		Mutex::Lock _l(_lock);
		return _tap->updateMulticastGroups(_multicastGroups);
	}

	/**
	 * @return Latest set of multicast groups for this network's tap
	 */
	inline std::set<MulticastGroup> multicastGroups() const
	{
		Mutex::Lock _l(_lock);
		return _multicastGroups;
	}

	/**
	 * Set or update this network's configuration
	 *
	 * This is called by PacketDecoder when an update comes over the wire, or
	 * internally when an old config is reloaded from disk.
	 *
	 * @param conf Configuration in key/value dictionary form
	 */
	void setConfiguration(const Config &conf);

	/**
	 * Causes this network to request an updated configuration from its master node now
	 */
	void requestConfiguration();

	/**
	 * Add or update a peer's membership certificate
	 *
	 * The certificate must already have been validated via signature checking.
	 *
	 * @param peer Peer that owns certificate
	 * @param cert Certificate itself
	 */
	void addMembershipCertificate(const Address &peer,const Certificate &cert);

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
	inline uint64_t lastConfigUpdate() const
		throw()
	{
		return _lastConfigUpdate;
	}

	/**
	 * @return Status of this network
	 */
	Status status() const;

	/**
	 * Invoke multicast rate limiter gate for a given address
	 *
	 * @param addr Address to check
	 * @param bytes Bytes address wishes to send us / propagate
	 * @return True if allowed, false if overshot rate limit
	 */
	inline bool multicastRateGate(const Address &addr,unsigned int bytes)
	{
		Mutex::Lock _l(_lock);
		std::map<Address,RateLimiter>::iterator rl(_multicastRateLimiters.find(addr));
		if (rl == _multicastRateLimiters.end()) {
			RateLimiter &newrl = _multicastRateLimiters[addr];
			newrl.init(ZT_MULTICAST_DEFAULT_RATE_PRELOAD);
			return newrl.gate(_rlLimit,(double)bytes);
		}
		return rl->second.gate(_rlLimit,(double)bytes);
	}

private:
	static void _CBhandleTapData(void *arg,const MAC &from,const MAC &to,unsigned int etherType,const Buffer<4096> &data);
	void _restoreState();

	const RuntimeEnvironment *_r;

	// Rate limits for this network
	RateLimiter::Limit _rlLimit;

	// Tap and tap multicast memberships
	EthernetTap *_tap;
	std::set<MulticastGroup> _multicastGroups;

	// Membership certificates supplied by peers
	std::map<Address,Certificate> _membershipCertificates;

	// Rate limiters for each multicasting peer
	std::map<Address,RateLimiter> _multicastRateLimiters;

	// Configuration from network master node
	Config _configuration;
	Certificate _myCertificate;

	uint64_t _id;
	volatile uint64_t _lastConfigUpdate;
	volatile bool _destroyOnDelete;
	volatile bool _ready;

	Mutex _lock;

	AtomicCounter __refCount;
};

} // naemspace ZeroTier

#endif
