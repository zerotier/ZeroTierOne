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

#include <stdint.h>

#include <string>
#include <set>
#include <map>
#include <vector>
#include <algorithm>
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
#include "BandwidthAccount.hpp"

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
	 * Certificate of network membership
	 *
	 * The COM consists of a series of three-element 64-bit tuples. These values
	 * are an id, a value, and a maximum delta. The ID is arbitrary and should be
	 * assigned using a scheme that makes every ID globally unique for a given
	 * type of parameter. ID 0 is reserved for the always-present timestamp
	 * parameter. The value is parameter-specific. The maximum delta is the
	 * maximum difference that is permitted between two values for determining
	 * whether a certificate permits two peers to speak to one another. A value
	 * of zero indicates that the values must equal.
	 *
	 * Certificates of membership must be signed by the netconf master for the
	 * network in question. This permits members to verify these certs against
	 * the netconf master's public key before testing them.
	 */
	class CertificateOfMembership
	{
	public:
		CertificateOfMembership() throw() {}
		CertificateOfMembership(const char *s) { fromString(s); }
		CertificateOfMembership(const std::string &s) { fromString(s.c_str()); }

		/**
		 * Add a paramter to this certificate
		 *
		 * @param id Parameter ID
		 * @param value Parameter value
		 * @param maxDelta Parameter maximum difference with others
		 */
		void addParameter(uint64_t id,uint64_t value,uint64_t maxDelta);

		/**
		 * @return Hex-serialized representation of this certificate (minus signature)
		 */
		std::string toString() const;

		/**
		 * Set this certificate equal to the hex-serialized string
		 *
		 * Invalid strings will result in invalid or undefined certificate
		 * contents. These will subsequently fail validation and comparison.
		 *
		 * @param s String to deserialize
		 */
		void fromString(const char *s);
		inline void fromString(const std::string &s) { fromString(s.c_str()); }

		/**
		 * Compare two certificates for parameter agreement
		 *
		 * This compares this certificate with the other and returns true if all
		 * paramters in this cert are present in the other and if they agree to
		 * within this cert's max delta value for each given parameter.
		 *
		 * @param other Cert to compare with
		 * @return True if certs agree and 'other' may be communicated with
		 */
		bool compare(const CertificateOfMembership &other) const
			throw();

	private:
		struct _Parameter
		{
			_Parameter() throw() {}
			_Parameter(uint64_t i,uint64_t v,uint64_t m) throw() :
				id(i),
				value(v),
				maxDelta(m) {}
			uint64_t id;
			uint64_t value;
			uint64_t maxDelta;
		};

		// Used with std::sort to ensure that _params are sorted
		struct _SortByIdComparison
		{
			inline bool operator()(const _Parameter &a,const _Parameter &b) const
				throw()
			{
				return (a.id < b.id);
			}
		};

		std::vector<_Parameter> _params;
	};

	/**
	 * Preload and rates of accrual for multicast group bandwidth limits
	 *
	 * Key is multicast group in lower case hex format: MAC (without :s) /
	 * ADI (hex). Value is preload, maximum balance, and rate of accrual in
	 * hex.
	 */
	class MulticastRates : private Dictionary
	{
	public:
		/**
		 * Preload and accrual parameter tuple
		 */
		struct Rate
		{
			Rate() {}
			Rate(uint32_t pl,uint32_t maxb,uint32_t acc)
			{
				preload = pl;
				maxBalance = maxb;
				accrual = acc;
			}
			uint32_t preload;
			uint32_t maxBalance;
			uint32_t accrual;
		};

		MulticastRates() {}
		MulticastRates(const char *s) : Dictionary(s) {}
		MulticastRates(const std::string &s) : Dictionary(s) {}
		inline std::string toString() const { return Dictionary::toString(); }

		/**
		 * A very minimal default rate, fast enough for ARP
		 */
		static const Rate GLOBAL_DEFAULT_RATE;

		/**
		 * @return Default rate, or GLOBAL_DEFAULT_RATE if not specified
		 */
		inline Rate defaultRate() const
		{
			Rate r;
			const_iterator dfl(find("*"));
			if (dfl == end())
				return GLOBAL_DEFAULT_RATE;
			return _toRate(dfl->second);
		}

		/**
		 * Get the rate for a given multicast group
		 *
		 * @param mg Multicast group
		 * @return Rate or default() rate if not specified
		 */
		inline Rate get(const MulticastGroup &mg) const
		{
			const_iterator r(find(mg.toString()));
			if (r == end())
				return defaultRate();
			return _toRate(r->second);
		}

	private:
		static inline Rate _toRate(const std::string &s)
		{
			char tmp[16384];
			Utils::scopy(tmp,sizeof(tmp),s.c_str());
			Rate r(0,0,0);
			char *saveptr = (char *)0;
			unsigned int fn = 0;
			for(char *f=Utils::stok(tmp,",",&saveptr);(f);f=Utils::stok((char *)0,",",&saveptr)) {
				switch(fn++) {
					case 0:
						r.preload = (uint32_t)Utils::hexStrToULong(f);
						break;
					case 1:
						r.maxBalance = (uint32_t)Utils::hexStrToULong(f);
						break;
					case 2:
						r.accrual = (uint32_t)Utils::hexStrToULong(f);
						break;
				}
			}
			return r;
		}
	};

	/**
	 * A network configuration for a given node
	 *
	 * Configuration fields:
	 *
	 * nwid=<hex network ID> (required)
	 * name=short name
	 * desc=long(er) description
	 * com=Certificate (serialized dictionary)
	 * mr=MulticastRates (serialized dictionary)
	 * o=open network? (1 or 0, default false if missing)
	 * et=ethertype whitelist (comma-delimited list of ethertypes in decimal)
	 * v4s=IPv4 static assignments / netmasks (comma-delimited)
	 * v6s=IPv6 static assignments / netmasks (comma-delimited)
	 */
	class Config : private Dictionary
	{
	public:
		Config() {}
		Config(const char *s) : Dictionary(s) {}
		Config(const std::string &s) : Dictionary(s) {}
		inline std::string toString() const { return Dictionary::toString(); }

		/**
		 * @return True if configuration is valid and contains required fields
		 */
		inline operator bool() const throw() { return (find("nwid") != end()); }

		/**
		 * @return Network ID
		 * @throws std::invalid_argument Network ID field missing
		 */
		inline uint64_t networkId() const
			throw(std::invalid_argument)
		{
			return Utils::hexStrToU64(get("nwid").c_str());
		}

		/**
		 * Get this network's short name, or its ID in hex if unspecified
		 *
		 * @return Short name of this network (e.g. "earth")
		 */
		inline std::string name() const
		{
			const_iterator n(find("name"));
			if (n == end())
				return get("nwid");
			return n->second;
		}

		/**
		 * @return Long description of network or empty string if not present
		 */
		inline std::string desc() const
		{
			return get("desc",std::string());
		}

		/**
		 * @return Certificate of membership for this network, or empty cert if none
		 */
		inline CertificateOfMembership certificateOfMembership() const
		{
			const_iterator cm(find("com"));
			if (cm == end())
				return CertificateOfMembership();
			else return CertificateOfMembership(cm->second);
		}

		/**
		 * @return Multicast rates for this network
		 */
		inline MulticastRates multicastRates() const
		{
			const_iterator mr(find("mr"));
			if (mr == end())
				return MulticastRates();
			else return MulticastRates(mr->second);
		}

		/**
		 * @return True if this is an open non-access-controlled network
		 */
		inline bool isOpen() const
		{
			const_iterator o(find("o"));
			if (o == end())
				return false;
			else if (!o->second.length())
				return false;
			else return (o->second[0] == '1');
		}

		/**
		 * @return Network ethertype whitelist
		 */
		inline std::set<unsigned int> etherTypes() const
		{
			char tmp[16384];
			char *saveptr = (char *)0;
			std::set<unsigned int> et;
			if (!Utils::scopy(tmp,sizeof(tmp),get("et","").c_str()))
				return et; // sanity check, packet can't really be that big
			for(char *f=Utils::stok(tmp,",",&saveptr);(f);f=Utils::stok((char *)0,",",&saveptr)) {
				unsigned int t = Utils::hexStrToUInt(f);
				if (t)
					et.insert(t);
			}
			return et;
		}

		/**
		 * @return All static addresses / netmasks, IPv4 or IPv6
		 */
		inline std::set<InetAddress> staticAddresses() const
		{
			std::set<InetAddress> sa;
			std::vector<std::string> ips(Utils::split(get("v4s","").c_str(),",","",""));
			for(std::vector<std::string>::const_iterator i(ips.begin());i!=ips.end();++i)
				sa.insert(InetAddress(*i));
			ips = Utils::split(get("v6s","").c_str(),",","","");
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
		throw()
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
	inline std::string idString()
	{
		char buf[64];
		Utils::snprintf(buf,sizeof(buf),"%.16llx",(unsigned long long)_id);
		return std::string(buf);
	}

	/**
	 * @return True if network is open (no membership required)
	 */
	inline bool isOpen() const
		throw()
	{
		Mutex::Lock _l(_lock);
		return _isOpen;
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
	void addMembershipCertificate(const Address &peer,const CertificateOfMembership &cert);

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
	 * Determine whether frames of a given ethernet type are allowed on this network
	 *
	 * @param etherType Ethernet frame type
	 * @return True if network permits this type
	 */
	inline bool permitsEtherType(unsigned int etherType) const
		throw()
	{
		if (!etherType)
			return false;
		else if (etherType > 65535)
			return false;
		else return ((_etWhitelist[etherType / 8] & (unsigned char)(1 << (etherType % 8))) != 0);
	}

	/**
	 * Update multicast balance for an address and multicast group, return whether packet is allowed
	 *
	 * @param a Address that wants to send/relay packet
	 * @param mg Multicast group
	 * @param bytes Size of packet
	 * @return True if packet is within budget
	 */
	inline bool updateAndCheckMulticastBalance(const Address &a,const MulticastGroup &mg,unsigned int bytes)
	{
		Mutex::Lock _l(_lock);
		std::pair<Address,MulticastGroup> k(a,mg);
		std::map< std::pair<Address,MulticastGroup>,BandwidthAccount >::iterator bal(_multicastRateAccounts.find(k));
		if (bal == _multicastRateAccounts.end()) {
			MulticastRates::Rate r(_mcRates.get(mg));
			bal = _multicastRateAccounts.insert(std::pair< std::pair<Address,MulticastGroup>,BandwidthAccount >(k,BandwidthAccount(r.preload,r.maxBalance,r.accrual))).first;
		}
		return bal->second.deduct(bytes);
		//bool tmp = bal->second.deduct(bytes);
		//printf("%s: BAL: %u\n",mg.toString().c_str(),(unsigned int)bal->second.balance());
		//return tmp;
	}

private:
	static void _CBhandleTapData(void *arg,const MAC &from,const MAC &to,unsigned int etherType,const Buffer<4096> &data);
	void _restoreState();

	const RuntimeEnvironment *_r;

	// Multicast bandwidth accounting for peers on this network
	std::map< std::pair<Address,MulticastGroup>,BandwidthAccount > _multicastRateAccounts;

	// Tap and tap multicast memberships for this node on this network
	EthernetTap *_tap;
	std::set<MulticastGroup> _multicastGroups;

	// Membership certificates supplied by other peers on this network
	std::map<Address,CertificateOfMembership> _membershipCertificates;

	// Configuration from network master node
	Config _configuration;
	CertificateOfMembership _myCertificate; // memoized from _configuration
	MulticastRates _mcRates; // memoized from _configuration
	std::set<InetAddress> _staticAddresses; // memoized from _configuration
	bool _isOpen; // memoized from _configuration

	// Ethertype whitelist bit field, set from config, for really fast lookup
	unsigned char _etWhitelist[65536 / 8];

	uint64_t _id;
	volatile uint64_t _lastConfigUpdate;
	volatile bool _destroyOnDelete;
	volatile bool _ready;

	Mutex _lock;

	AtomicCounter __refCount;
};

} // naemspace ZeroTier

#endif
