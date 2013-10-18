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

#include "NetworkConfig.hpp"
#include "Utils.hpp"

namespace ZeroTier {

// This is fast enough for things like Apple's mDNS spam, so it should serve
// as a good default for your average network. It's 64 bytes per second, with
// a starting and max balance of 64k.
const NetworkConfig::MulticastRate NetworkConfig::DEFAULT_MULTICAST_RATE(65535,65535,64);

std::set<unsigned int> NetworkConfig::allowedEtherTypes() const
{
	std::set<unsigned int> ets;
	for(unsigned int i=0;i<sizeof(_etWhitelist);++i) {
		if (_etWhitelist[i]) {
			unsigned char b = _etWhitelist[i];
			unsigned int et = i * 8;
			while (b) {
				if ((b & 1))
					ets.insert(et);
				b >>= 1;
				++et;
			}
		}
	}
	return ets;
}

const NetworkConfig::MulticastRate &NetworkConfig::multicastRate(const MulticastGroup &mg) const
	throw()
{
	std::map<MulticastGroup,MulticastRate>::const_iterator r(_multicastRates.find(mg));
	if (r == _multicastRates.end()) {
		r = _multicastRates.find(MulticastGroup()); // zero MG signifies network's default rate
		if (r == _multicastRates.end())
			return DEFAULT_MULTICAST_RATE; // neither specific nor default found in network config
	}
	return r->second;
}

static const std::string _zero("0");
void NetworkConfig::_fromDictionary(const Dictionary &d)
{
	// NOTE: d.get(name) throws if not found, d.get(name,default) returns default

	memset(_etWhitelist,0,sizeof(_etWhitelist));
	std::vector<std::string> ets(Utils::split(d.get(ZT_NETWORKCONFIG_DICT_KEY_ALLOWED_ETHERNET_TYPES).c_str(),",","",""));
	for(std::vector<std::string>::const_iterator et(ets.begin());et!=ets.end();++et) {
		unsigned int tmp = Utils::hexStrToUInt(et->c_str()) & 0xffff;
		_etWhitelist[tmp >> 3] |= (1 << (tmp & 7));
	}

	_nwid = Utils::hexStrToU64(d.get(ZT_NETWORKCONFIG_DICT_KEY_NETWORK_ID).c_str());
	if (!_nwid)
		throw std::invalid_argument("configuration contains zero network ID");
	_timestamp = Utils::hexStrToU64(d.get(ZT_NETWORKCONFIG_DICT_KEY_TIMESTAMP).c_str());
	_issuedTo = Address(d.get(ZT_NETWORKCONFIG_DICT_KEY_ISSUED_TO));
	_multicastPrefixBits = Utils::hexStrToUInt(d.get(ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_PREFIX_BITS,_zero).c_str());
	_multicastDepth = Utils::hexStrToUInt(d.get(ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_DEPTH,_zero).c_str());
	_arpCacheTtl = Utils::hexStrToUInt(d.get(ZT_NETWORKCONFIG_DICT_KEY_ARP_CACHE_TTL,_zero).c_str());
	_ndpCacheTtl = Utils::hexStrToUInt(d.get(ZT_NETWORKCONFIG_DICT_KEY_NDP_CACHE_TTL,_zero).c_str());
	_emulateArp = (Utils::hexStrToUInt(d.get(ZT_NETWORKCONFIG_DICT_KEY_EMULATE_ARP,_zero).c_str()) != 0);
	_emulateNdp = (Utils::hexStrToUInt(d.get(ZT_NETWORKCONFIG_DICT_KEY_EMULATE_NDP,_zero).c_str()) != 0);
	_isOpen = (Utils::hexStrToUInt(d.get(ZT_NETWORKCONFIG_DICT_KEY_IS_OPEN,_zero).c_str()) != 0);
	_name = d.get(ZT_NETWORKCONFIG_DICT_KEY_NAME);
	_description = d.get(ZT_NETWORKCONFIG_DICT_KEY_DESC,std::string());

	if (!_multicastPrefixBits)
		_multicastPrefixBits = ZT_DEFAULT_MULTICAST_PREFIX_BITS;
	if (!_multicastDepth)
		_multicastDepth = ZT_DEFAULT_MULTICAST_DEPTH;

	std::string ipAddrs(d.get(ZT_NETWORKCONFIG_DICT_KEY_IPV4_STATIC,std::string()));
	std::string v6s(d.get(ZT_NETWORKCONFIG_DICT_KEY_IPV6_STATIC,std::string()));
	if (v6s.length()) {
		if (ipAddrs.length())
			ipAddrs.push_back(',');
		ipAddrs.append(v6s);
	}
	std::vector<std::string> ipAddrs2(Utils::split(ipAddrs.c_str(),",","",""));
	for(std::vector<std::string>::const_iterator ipstr(ipAddrs2.begin());ipstr!=ipAddrs2.end();++ipstr) {
		InetAddress addr(*ipstr);
		switch(addr.type()) {
			case InetAddress::TYPE_IPV4:
				if ((!addr.netmaskBits())||(addr.netmaskBits() > 32))
					throw std::invalid_argument("static IP address fields contain one or more invalid IP/netmask entries");
				break;
			case InetAddress::TYPE_IPV6:
				if ((!addr.netmaskBits())||(addr.netmaskBits() > 128))
					throw std::invalid_argument("static IP address fields contain one or more invalid IP/netmask entries");
				break;
			default:
				throw std::invalid_argument("static IP address fields contain one or more invalid IP/netmask entries");
		}
		_staticIps.insert(addr);
	}

	Dictionary mr(d.get(ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_RATES,std::string()));
	for(Dictionary::const_iterator i(mr.begin());i!=mr.end();++i) {
		std::vector<std::string> params(Utils::split(i->second.c_str(),",","",""));
		if (params.size() >= 3)
			_multicastRates[MulticastGroup(i->first)] = MulticastRate(Utils::hexStrToUInt(params[0].c_str()),Utils::hexStrToUInt(params[1].c_str()),Utils::hexStrToUInt(params[2].c_str()));
	}
}

} // namespace ZeroTier

