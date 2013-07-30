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
	 * A certificate of network membership
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

		/**
		 * @return Read-only underlying dictionary
		 */
		inline const Dictionary &dictionary() const { return *this; }

		inline void setNetworkId(uint64_t id)
		{
			char buf[32];
			sprintf(buf,"%llu",id);
			(*this)["nwid"] = buf;
		}

		inline uint64_t networkId() const
			throw(std::invalid_argument)
		{
			return strtoull(get("nwid").c_str(),(char **)0,10);
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
			sprintf(foo,"%llu",ts);
			(*this)["ts"] = foo;
			sprintf(foo,"%llu",maxDelta);
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

		inline void setNetworkId(uint64_t id)
		{
			char buf[32];
			sprintf(buf,"%llu",id);
			(*this)["nwid"] = buf;
		}

		inline uint64_t networkId() const
			throw(std::invalid_argument)
		{
			return strtoull(get("nwid").c_str(),(char **)0,10);
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
		 * @return Certificate of membership for this network, or empty cert if none
		 */
		inline Certificate certificateOfMembership() const
		{
			return Certificate(get("com",""));
		}

		/**
		 * @return True if this is an open non-access-controlled network
		 */
		inline bool isOpen() const
		{
			return (get("isOpen") == "1");
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

		/**
		 * Set static IPv4 and IPv6 addresses
		 *
		 * This sets the ipv4Static and ipv6Static fields to comma-delimited
		 * lists of assignments. The port field in InetAddress must be the
		 * number of bits in the netmask.
		 *
		 * @param begin Start of container or array of addresses (InetAddress)
		 * @param end End of container or array of addresses (InetAddress)
		 * @tparam I Type of container or array
		 */
		template<typename I>
		inline void setStaticInetAddresses(const I &begin,const I &end)
		{
			std::string v4;
			std::string v6;
			for(I i(begin);i!=end;++i) {
				if (i->isV4()) {
					if (v4.length())
						v4.push_back(',');
					v4.append(i->toString());
				} else if (i->isV6()) {
					if (v6.length())
						v6.push_back(',');
					v6.append(i->toString());
				}
			}
			if (v4.length())
				(*this)["ipv4Static"] = v4;
			else erase("ipv4Static");
			if (v6.length())
				(*this)["ipv6Static"] = v6;
			else erase("ipv6Static");
		}
	};

private:
	// Only NodeConfig can create, only SharedPtr can delete
	Network(const RuntimeEnvironment *renv,uint64_t id)
		throw(std::runtime_error);

	~Network();

public:
	/**
	 * @return Network ID
	 */
	inline uint64_t id() const throw() { return _id; }

	/**
	 * @return Ethernet tap
	 */
	inline EthernetTap &tap() throw() { return _tap; }

	/**
	 * @return Address of network's controlling node
	 */
	inline Address controller() throw() { return Address(_id >> 24); }

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
		return _tap.updateMulticastGroups(_multicastGroups);
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
	inline void addMembershipCertificate(const Address &peer,const Certificate &cert)
	{
		Mutex::Lock _l(_lock);
		_membershipCertificates[peer] = cert;
	}

	bool isAllowed(const Address &peer) const;

	/**
	 * Perform periodic database cleaning such as removing expired membership certificates
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

private:
	static void _CBhandleTapData(void *arg,const MAC &from,const MAC &to,unsigned int etherType,const Buffer<4096> &data);

	const RuntimeEnvironment *_r;

	EthernetTap _tap;
	std::set<MulticastGroup> _multicastGroups;
	std::map<Address,Certificate> _membershipCertificates;
	Config _configuration;
	Certificate _myCertificate;
	uint64_t _lastConfigUpdate;
	uint64_t _id;
	Mutex _lock;

	AtomicCounter __refCount;
};

} // naemspace ZeroTier

#endif
