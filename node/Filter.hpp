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

#ifndef _ZT_FILTER_HPP
#define _ZT_FILTER_HPP

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <utility>

#include "Mutex.hpp"
#include "Range.hpp"
 
/* Ethernet frame types that might be relevant to us */
#define ZT_ETHERTYPE_IPV4 0x0800
#define ZT_ETHERTYPE_ARP 0x0806
#define ZT_ETHERTYPE_RARP 0x8035
#define ZT_ETHERTYPE_ATALK 0x809b
#define ZT_ETHERTYPE_AARP 0x80f3
#define ZT_ETHERTYPE_IPX_A 0x8137
#define ZT_ETHERTYPE_IPX_B 0x8138
#define ZT_ETHERTYPE_IPV6 0x86dd

/* IP protocols we might care about */
#define ZT_IPPROTO_ICMP 0x01
#define ZT_IPPROTO_IGMP 0x02
#define ZT_IPPROTO_TCP 0x06
#define ZT_IPPROTO_UDP 0x11
#define ZT_IPPROTO_RDP 0x1b
#define ZT_IPPROTO_GRE 0x2f
#define ZT_IPPROTO_ESP 0x32
#define ZT_IPPROTO_AH 0x33
#define ZT_IPPROTO_ICMPV6 0x3a
#define ZT_IPPROTO_OSPF 0x59
#define ZT_IPPROTO_IPIP 0x5e
#define ZT_IPPROTO_IPCOMP 0x6c
#define ZT_IPPROTO_L2TP 0x73
#define ZT_IPPROTO_SCTP 0x84
#define ZT_IPPROTO_FC 0x85
#define ZT_IPPROTO_UDPLITE 0x88
#define ZT_IPPROTO_HIP 0x8b

namespace ZeroTier {

class RuntimeEnvironment;

/**
 * A simple Ethernet frame level filter supporting basic IP port DENY
 */
class Filter
{
public:
	/**
	 * A filter rule
	 *
	 * This behaves as an immutable value object.
	 */
	class Rule
	{
	public:
		Rule()
			throw() :
			_etherType(-1),
			_protocol(-1),
			_port(-1)
		{
		}

		/**
		 * Construct a new rule
		 *
		 * @param etype Ethernet type or negative for ANY
		 * @param prot Protocol or negative for ANY (meaning depends on ethertype, e.g. IP protocol numbers)
		 * @param prt Port or negative for ANY (only applies to some protocols)
		 */
		Rule(int etype,int prot,int prt)
			throw() :
			_etherType((etype >= 0) ? etype : -1),
			_protocol((prot >= 0) ? prot : -1),
			_port((prt >= 0) ? prt : -1)
		{
		}

		inline int etherType() const throw() { return _etherType; }
		inline int protocol() const throw() { return _protocol; }
		inline int port() const throw() { return _port; }

		/**
		 * Test this rule against a frame
		 *
		 * @param etype Type of ethernet frame
		 * @param data Ethernet frame data
		 * @param len Length of ethernet frame
		 * @return True if rule matches
		 */
		bool operator()(unsigned int etype,const void *data,unsigned int len) const;

		inline bool operator==(const Rule &r) const throw() { return ((_etherType == r._etherType)&&(_protocol == r._protocol)&&(_port == r._port)); }
		inline bool operator!=(const Rule &r) const throw() { return !(*this == r); }
		inline bool operator<(const Rule &r) const
			throw()
		{
			if (_etherType < r._etherType)
				return true;
			else if (_etherType == r._etherType) {
				if (_protocol < r._protocol)
					return true;
				else if (_protocol == r._protocol) {
					if (_port < r._port)
						return true;
				}
			}
			return false;
		}
		inline bool operator>(const Rule &r) const throw() { return (r < *this); }
		inline bool operator<=(const Rule &r) const throw() { return !(r < *this); }
		inline bool operator>=(const Rule &r) const throw() { return !(*this < r); }

	private:
		int _etherType;
		int _protocol;
		int _port;
	};

	/**
	 * Action if a rule matches
	 */
	enum Action
	{
		ACTION_DENY = 0,
		ACTION_ALLOW = 1,
		ACTION_LOG = 2
	};

	/**
	 * Entry in filter chain
	 */
	struct Entry
	{
		Entry() {}
		Entry(const Rule &r,const Action &a) :
			rule(r),
			action(a)
		{
		}

		Rule rule;
		Action action;
	};

	Filter(const RuntimeEnvironment *renv);
	~Filter();

	/**
	 * Remove all filter entries
	 */
	inline void clear()
	{
		Mutex::Lock _l(_chain_m);
		_chain.clear();
	}

	/**
	 * Append a rule/action pair to this chain
	 *
	 * If an identical rule already exists it is removed and a new entry is
	 * added to the end with the new action. (Two identical rules with the
	 * same action wouldn't make sense.)
	 *
	 * @param r Rule to add
	 * @param a Action if rule matches
	 */
	void add(const Rule &r,const Action &a);

	/**
	 * @return Number of rules in filter chain
	 */
	inline unsigned int length() const
		throw()
	{
		Mutex::Lock _l(_chain_m);
		return _chain.length();
	}

	/**
	 * @return Entry in filter chain or null entry if out of bounds
	 */
	inline Entry operator[](const unsigned int i) const
		throw()
	{
		Mutex::Lock _l(_chain_m);
		if (i < _chain.length())
			return _chain[i];
		return Entry();
	}

	/**
	 * @param etherType Ethernet type ID
	 * @return Name of Ethernet protocol (e.g. ARP, IPV4)
	 */
	static const char *etherTypeName(const unsigned int etherType)
		throw();

private:
	const RuntimeEnvironment *_r;

	std::vector<Entry> _chain;
	Mutex _chain_m;
};

} // namespace ZeroTier

#endif
