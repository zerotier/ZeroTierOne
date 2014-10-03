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

#include "NetworkConfig.hpp"
#include "Utils.hpp"

namespace ZeroTier {

// This is fast enough for things like Apple's mDNS spam, so it should serve
// as a good default for your average network. It's 64 bytes per second, with
// a starting and max balance of 64k.
const NetworkConfig::MulticastRate NetworkConfig::DEFAULT_MULTICAST_RATE(32768,32768,64);

SharedPtr<NetworkConfig> NetworkConfig::createTestNetworkConfig(const Address &self)
{
	SharedPtr<NetworkConfig> nc(new NetworkConfig());

	memset(nc->_etWhitelist,0,sizeof(nc->_etWhitelist));
	nc->_etWhitelist[0] |= 1; // allow all
	nc->_nwid = ZT_TEST_NETWORK_ID;
	nc->_timestamp = Utils::now();
	nc->_issuedTo = self;
	nc->_multicastLimit = ZT_MULTICAST_DEFAULT_LIMIT;
	nc->_allowPassiveBridging = false;
	nc->_private = false;
	nc->_enableBroadcast = true;
	nc->_name = "ZT_TEST_NETWORK";
	nc->_description = "Built-in dummy test network";

	return nc;
}

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

void NetworkConfig::_fromDictionary(const Dictionary &d)
{
	static const std::string zero("0");
	static const std::string one("1");

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
	_multicastLimit = Utils::hexStrToUInt(d.get(ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_LIMIT,zero).c_str());
	if (_multicastLimit == 0) _multicastLimit = ZT_MULTICAST_DEFAULT_LIMIT;
	_allowPassiveBridging = (Utils::hexStrToUInt(d.get(ZT_NETWORKCONFIG_DICT_KEY_ALLOW_PASSIVE_BRIDGING,zero).c_str()) != 0);
	_private = (Utils::hexStrToUInt(d.get(ZT_NETWORKCONFIG_DICT_KEY_PRIVATE,one).c_str()) != 0);
	_enableBroadcast = (Utils::hexStrToUInt(d.get(ZT_NETWORKCONFIG_DICT_KEY_ENABLE_BROADCAST,one).c_str()) != 0);
	_name = d.get(ZT_NETWORKCONFIG_DICT_KEY_NAME);
	_description = d.get(ZT_NETWORKCONFIG_DICT_KEY_DESC,std::string());

	// In dictionary IPs are split into V4 and V6 addresses, but we don't really
	// need that so merge them here.
	std::string ipAddrs(d.get(ZT_NETWORKCONFIG_DICT_KEY_IPV4_STATIC,std::string()));
	{
		std::string v6s(d.get(ZT_NETWORKCONFIG_DICT_KEY_IPV6_STATIC,std::string()));
		if (v6s.length()) {
			if (ipAddrs.length())
				ipAddrs.push_back(',');
			ipAddrs.append(v6s);
		}
	}

	std::vector<std::string> ipAddrsSplit(Utils::split(ipAddrs.c_str(),",","",""));
	for(std::vector<std::string>::const_iterator ipstr(ipAddrsSplit.begin());ipstr!=ipAddrsSplit.end();++ipstr) {
		InetAddress addr(*ipstr);
		switch(addr.type()) {
			case InetAddress::TYPE_IPV4:
				if ((!addr.netmaskBits())||(addr.netmaskBits() > 32))
					continue;
				break;
			case InetAddress::TYPE_IPV6:
				if ((!addr.netmaskBits())||(addr.netmaskBits() > 128))
					continue;
				break;
			default: // ignore unrecognized address types or junk/empty fields
				continue;
		}
		_staticIps.insert(addr);
	}

	std::vector<std::string> activeBridgesSplit(Utils::split(d.get(ZT_NETWORKCONFIG_DICT_KEY_ACTIVE_BRIDGES,"").c_str(),",","",""));
	for(std::vector<std::string>::const_iterator a(activeBridgesSplit.begin());a!=activeBridgesSplit.end();++a) {
		if (a->length() == ZT_ADDRESS_LENGTH_HEX) { // ignore empty or garbage fields
			Address tmp(*a);
			if (!tmp.isReserved())
				_activeBridges.insert(tmp);
		}
	}

	Dictionary multicastRateEntries(d.get(ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_RATES,std::string()));
	for(Dictionary::const_iterator i(multicastRateEntries.begin());i!=multicastRateEntries.end();++i) {
		std::vector<std::string> params(Utils::split(i->second.c_str(),",","",""));
		if (params.size() >= 3)
			_multicastRates[MulticastGroup(i->first)] = MulticastRate(Utils::hexStrToUInt(params[0].c_str()),Utils::hexStrToUInt(params[1].c_str()),Utils::hexStrToUInt(params[2].c_str()));
	}

	_com.fromString(d.get(ZT_NETWORKCONFIG_DICT_KEY_CERTIFICATE_OF_MEMBERSHIP,std::string()));
}

} // namespace ZeroTier

