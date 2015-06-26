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

#include <stdint.h>

#include "NetworkConfig.hpp"
#include "Utils.hpp"

namespace ZeroTier {

SharedPtr<NetworkConfig> NetworkConfig::createTestNetworkConfig(const Address &self)
{
	SharedPtr<NetworkConfig> nc(new NetworkConfig());

	memset(nc->_etWhitelist,0,sizeof(nc->_etWhitelist));
	nc->_etWhitelist[0] |= 1; // allow all
	nc->_nwid = ZT_TEST_NETWORK_ID;
	nc->_timestamp = 1;
	nc->_revision = 1;
	nc->_issuedTo = self;
	nc->_multicastLimit = ZT_MULTICAST_DEFAULT_LIMIT;
	nc->_allowPassiveBridging = false;
	nc->_private = false;
	nc->_enableBroadcast = true;
	nc->_name = "ZT_TEST_NETWORK";
	nc->_description = "Built-in dummy test network";

	// Make up a V4 IP from 'self' in the 10.0.0.0/8 range -- no
	// guarantee of uniqueness but collisions are unlikely.
	uint32_t ip = (uint32_t)((self.toInt() & 0x00ffffff) | 0x0a000000); // 10.x.x.x
	if ((ip & 0x000000ff) == 0x000000ff) ip ^= 0x00000001; // but not ending in .255
	if ((ip & 0x000000ff) == 0x00000000) ip ^= 0x00000001; // or .0
	nc->_staticIps.push_back(InetAddress(Utils::hton(ip),8));

	return nc;
}

std::vector<unsigned int> NetworkConfig::allowedEtherTypes() const
{
	std::vector<unsigned int> ets;
	if ((_etWhitelist[0] & 1) != 0) {
		ets.push_back(0);
	} else {
		for(unsigned int i=0;i<sizeof(_etWhitelist);++i) {
			if (_etWhitelist[i]) {
				unsigned char b = _etWhitelist[i];
				unsigned int et = i * 8;
				while (b) {
					if ((b & 1))
						ets.push_back(et);
					b >>= 1;
					++et;
				}
			}
		}
	}
	return ets;
}

void NetworkConfig::_fromDictionary(const Dictionary &d)
{
	static const std::string zero("0");
	static const std::string one("1");

	// NOTE: d.get(name) throws if not found, d.get(name,default) returns default

	_nwid = Utils::hexStrToU64(d.get(ZT_NETWORKCONFIG_DICT_KEY_NETWORK_ID).c_str());
	if (!_nwid)
		throw std::invalid_argument("configuration contains zero network ID");

	_timestamp = Utils::hexStrToU64(d.get(ZT_NETWORKCONFIG_DICT_KEY_TIMESTAMP).c_str());
	_revision = Utils::hexStrToU64(d.get(ZT_NETWORKCONFIG_DICT_KEY_REVISION,"1").c_str()); // older controllers don't send this, so default to 1

	memset(_etWhitelist,0,sizeof(_etWhitelist));
	std::vector<std::string> ets(Utils::split(d.get(ZT_NETWORKCONFIG_DICT_KEY_ALLOWED_ETHERNET_TYPES).c_str(),",","",""));
	for(std::vector<std::string>::const_iterator et(ets.begin());et!=ets.end();++et) {
		unsigned int tmp = Utils::hexStrToUInt(et->c_str()) & 0xffff;
		_etWhitelist[tmp >> 3] |= (1 << (tmp & 7));
	}

	_issuedTo = Address(d.get(ZT_NETWORKCONFIG_DICT_KEY_ISSUED_TO));
	_multicastLimit = Utils::hexStrToUInt(d.get(ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_LIMIT,zero).c_str());
	if (_multicastLimit == 0) _multicastLimit = ZT_MULTICAST_DEFAULT_LIMIT;
	_allowPassiveBridging = (Utils::hexStrToUInt(d.get(ZT_NETWORKCONFIG_DICT_KEY_ALLOW_PASSIVE_BRIDGING,zero).c_str()) != 0);
	_private = (Utils::hexStrToUInt(d.get(ZT_NETWORKCONFIG_DICT_KEY_PRIVATE,one).c_str()) != 0);
	_enableBroadcast = (Utils::hexStrToUInt(d.get(ZT_NETWORKCONFIG_DICT_KEY_ENABLE_BROADCAST,one).c_str()) != 0);
	_name = d.get(ZT_NETWORKCONFIG_DICT_KEY_NAME);
	if (_name.length() > ZT1_MAX_NETWORK_SHORT_NAME_LENGTH)
		throw std::invalid_argument("network short name too long (max: 255 characters)");
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
		switch(addr.ss_family) {
			case AF_INET:
				if ((!addr.netmaskBits())||(addr.netmaskBits() > 32))
					continue;
				break;
			case AF_INET6:
				if ((!addr.netmaskBits())||(addr.netmaskBits() > 128))
					continue;
				break;
			default: // ignore unrecognized address types or junk/empty fields
				continue;
		}
		_staticIps.push_back(addr);
	}
	if (_staticIps.size() > ZT1_MAX_ZT_ASSIGNED_ADDRESSES)
		throw std::invalid_argument("too many ZT-assigned IP addresses");
	std::sort(_staticIps.begin(),_staticIps.end());
	std::unique(_staticIps.begin(),_staticIps.end());

	std::vector<std::string> gatewaysSplit(Utils::split(d.get(ZT_NETWORKCONFIG_DICT_KEY_GATEWAYS,"").c_str(),",","",""));
	for(std::vector<std::string>::const_iterator gwstr(gatewaysSplit.begin());gwstr!=gatewaysSplit.end();++gwstr) {
		InetAddress gw(*gwstr);
		if ((std::find(_gateways.begin(),_gateways.end(),gw) == _gateways.end())&&((gw.ss_family == AF_INET)||(gw.ss_family == AF_INET6)))
			_gateways.push_back(gw);
	}

	std::vector<std::string> activeBridgesSplit(Utils::split(d.get(ZT_NETWORKCONFIG_DICT_KEY_ACTIVE_BRIDGES,"").c_str(),",","",""));
	for(std::vector<std::string>::const_iterator a(activeBridgesSplit.begin());a!=activeBridgesSplit.end();++a) {
		if (a->length() == ZT_ADDRESS_LENGTH_HEX) { // ignore empty or garbage fields
			Address tmp(*a);
			if (!tmp.isReserved())
				_activeBridges.push_back(tmp);
		}
	}
	std::sort(_activeBridges.begin(),_activeBridges.end());
	std::unique(_activeBridges.begin(),_activeBridges.end());

	std::vector<std::string> relaysSplit(Utils::split(d.get(ZT_NETWORKCONFIG_DICT_KEY_RELAYS,"").c_str(),",","",""));
	for(std::vector<std::string>::const_iterator r(relaysSplit.begin());r!=relaysSplit.end();++r) {
		std::size_t semi(r->find(';')); // address;ip/port,...
		if (semi == ZT_ADDRESS_LENGTH_HEX) {
			std::pair<Address,InetAddress> relay(
				Address(r->substr(0,semi)),
				((r->length() > (semi + 1)) ? InetAddress(r->substr(semi + 1)) : InetAddress()) );
			if ((relay.first)&&(!relay.first.isReserved()))
				_relays.push_back(relay);
		}
	}
	std::sort(_relays.begin(),_relays.end());
	std::unique(_relays.begin(),_relays.end());

	_com.fromString(d.get(ZT_NETWORKCONFIG_DICT_KEY_CERTIFICATE_OF_MEMBERSHIP,std::string()));
}

bool NetworkConfig::operator==(const NetworkConfig &nc) const
{
	if (_nwid != nc._nwid) return false;
	if (_timestamp != nc._timestamp) return false;
	if (memcmp(_etWhitelist,nc._etWhitelist,sizeof(_etWhitelist))) return false;
	if (_issuedTo != nc._issuedTo) return false;
	if (_multicastLimit != nc._multicastLimit) return false;
	if (_allowPassiveBridging != nc._allowPassiveBridging) return false;
	if (_private != nc._private) return false;
	if (_enableBroadcast != nc._enableBroadcast) return false;
	if (_name != nc._name) return false;
	if (_description != nc._description) return false;
	if (_staticIps != nc._staticIps) return false;
	if (_gateways != nc._gateways) return false;
	if (_activeBridges != nc._activeBridges) return false;
	if (_relays != nc._relays) return false;
	if (_com != nc._com) return false;
	return true;
}

} // namespace ZeroTier

