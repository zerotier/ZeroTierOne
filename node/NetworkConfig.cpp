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

#include <stdint.h>

#include "NetworkConfig.hpp"
#include "Utils.hpp"

namespace ZeroTier {

namespace {

struct ZT_VirtualNetworkStaticDevice_SortByAddress
{
	inline bool operator()(const ZT_VirtualNetworkStaticDevice &a,const ZT_VirtualNetworkStaticDevice &b)
	{
		return (a.address < b.address);
	}
};

struct ZT_VirtualNetworkRule_SortByRuleNo
{
	inline bool operator()(const ZT_VirtualNetworkRule &a,const ZT_VirtualNetworkRule &b)
	{
		return (a.ruleNo < b.ruleNo);
	}
};

} // anonymous namespace

NetworkConfig NetworkConfig::createTestNetworkConfig(const Address &self)
{
	NetworkConfig nc;

	nc._nwid = ZT_TEST_NETWORK_ID;
	nc._timestamp = 1;
	nc._revision = 1;
	nc._issuedTo = self;
	nc._multicastLimit = ZT_MULTICAST_DEFAULT_LIMIT;
	nc._allowPassiveBridging = false;
	nc._type = ZT_NETWORK_TYPE_PUBLIC;
	nc._enableBroadcast = true;

	nc._rules[nc._ruleCount].ruleNo = 1;
	nc._rules[nc._ruleCount].matches = (uint8_t)ZT_NETWORK_RULE_MATCHES_ALL;
	nc._rules[nc._ruleCount].action = (uint8_t)ZT_NETWORK_RULE_ACTION_ACCEPT;
	nc._ruleCount = 1;

	Utils::snprintf(nc._name,sizeof(nc._name),"ZT_TEST_NETWORK");

	// Make up a V4 IP from 'self' in the 10.0.0.0/8 range -- no
	// guarantee of uniqueness but collisions are unlikely.
	uint32_t ip = (uint32_t)((self.toInt() & 0x00ffffff) | 0x0a000000); // 10.x.x.x
	if ((ip & 0x000000ff) == 0x000000ff) ip ^= 0x00000001; // but not ending in .255
	if ((ip & 0x000000ff) == 0x00000000) ip ^= 0x00000001; // or .0
	nc._staticIps[0] = InetAddress(Utils::hton(ip),8);

	// Assign an RFC4193-compliant IPv6 address -- will never collide
	nc._staticIps[1] = InetAddress::makeIpv6rfc4193(ZT_TEST_NETWORK_ID,self.toInt());

	nc._staticIpCount = 2;

	return nc;
}

#ifdef ZT_SUPPORT_OLD_STYLE_NETCONF

void NetworkConfig::fromDictionary(const char *ds,unsigned int dslen)
{
	Dictionary d(ds,dslen);

	static const std::string zero("0");
	static const std::string one("1");

	memset(this,0,sizeof(NetworkConfig));

	// NOTE: d.get(name) throws if not found, d.get(name,default) returns default

	_nwid = Utils::hexStrToU64(d.get(ZT_NETWORKCONFIG_DICT_KEY_NETWORK_ID,"0").c_str());
	if (!_nwid)
		throw std::invalid_argument("configuration contains zero network ID");

	_timestamp = Utils::hexStrToU64(d.get(ZT_NETWORKCONFIG_DICT_KEY_TIMESTAMP,"0").c_str());
	_revision = Utils::hexStrToU64(d.get(ZT_NETWORKCONFIG_DICT_KEY_REVISION,"1").c_str()); // older controllers don't send this, so default to 1
	_issuedTo = Address(d.get(ZT_NETWORKCONFIG_DICT_KEY_ISSUED_TO,"0"));

	_multicastLimit = Utils::hexStrToUInt(d.get(ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_LIMIT,zero).c_str());
	if (_multicastLimit == 0) _multicastLimit = ZT_MULTICAST_DEFAULT_LIMIT;

	_allowPassiveBridging = (Utils::hexStrToUInt(d.get(ZT_NETWORKCONFIG_DICT_KEY_ALLOW_PASSIVE_BRIDGING,zero).c_str()) != 0);
	_enableBroadcast = (Utils::hexStrToUInt(d.get(ZT_NETWORKCONFIG_DICT_KEY_ENABLE_BROADCAST,one).c_str()) != 0);
	_type = (Utils::hexStrToUInt(d.get(ZT_NETWORKCONFIG_DICT_KEY_PRIVATE,one).c_str()) != 0) ? ZT_NETWORK_TYPE_PRIVATE : ZT_NETWORK_TYPE_PUBLIC;

	std::string nametmp(d.get(ZT_NETWORKCONFIG_DICT_KEY_NAME,""));
	for(unsigned long i=0;((i<ZT_MAX_NETWORK_SHORT_NAME_LENGTH)&&(i<nametmp.length()));++i)
		_name[i] = (char)nametmp[i];
	// we zeroed the entire structure above and _name is ZT_MAX_NETWORK_SHORT_NAME_LENGTH+1, so it will always null-terminate

	std::vector<std::string> activeBridgesSplit(Utils::split(d.get(ZT_NETWORKCONFIG_DICT_KEY_ACTIVE_BRIDGES,"").c_str(),",","",""));
	for(std::vector<std::string>::const_iterator a(activeBridgesSplit.begin());a!=activeBridgesSplit.end();++a) {
		if (a->length() == ZT_ADDRESS_LENGTH_HEX) { // ignore empty or garbage fields
			Address tmp(*a);
			if (!tmp.isReserved()) {
				if ((_activeBridgeCount < ZT_MAX_NETWORK_ACTIVE_BRIDGES)&&(std::find(&(_activeBridges[0]),&(_activeBridges[_activeBridgeCount]),tmp) == &(_activeBridges[_activeBridgeCount])))
					_activeBridges[_activeBridgeCount++] = tmp;
			}
		}
	}
	std::sort(&(_activeBridges[0]),&(_activeBridges[_activeBridgeCount]));

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
		if (addr.isNetwork()) {
			if ((_localRouteCount < ZT_MAX_NETWORK_LOCAL_ROUTES)&&(std::find(&(_localRoutes[0]),&(_localRoutes[_localRouteCount]),addr) == &(_localRoutes[_localRouteCount])))
				_localRoutes[_localRouteCount++] = addr;
		} else {
			if ((_staticIpCount < ZT_MAX_ZT_ASSIGNED_ADDRESSES)&&(std::find(&(_staticIps[0]),&(_staticIps[_staticIpCount]),addr) == &(_staticIps[_staticIpCount])))
				_staticIps[_staticIpCount++] = addr;
		}
	}
	std::sort(&(_localRoutes[0]),&(_localRoutes[_localRouteCount]));
	std::sort(&(_staticIps[0]),&(_staticIps[_staticIpCount]));

	std::vector<std::string> gatewaysSplit(Utils::split(d.get(ZT_NETWORKCONFIG_DICT_KEY_GATEWAYS,"").c_str(),",","",""));
	for(std::vector<std::string>::const_iterator gwstr(gatewaysSplit.begin());gwstr!=gatewaysSplit.end();++gwstr) {
		InetAddress gw(*gwstr);
		if ((gw)&&(_gatewayCount < ZT_MAX_NETWORK_GATEWAYS)&&(std::find(&(_gateways[0]),&(_gateways[_gatewayCount]),gw) == &(_gateways[_gatewayCount])))
			_gateways[_gatewayCount++] = gw;
	}
	std::sort(&(_gateways[0]),&(_gateways[_gatewayCount]));

	std::vector<std::string> relaysSplit(Utils::split(d.get(ZT_NETWORKCONFIG_DICT_KEY_RELAYS,"").c_str(),",","",""));
	for(std::vector<std::string>::const_iterator r(relaysSplit.begin());r!=relaysSplit.end();++r) {
		if (r->length() >= ZT_ADDRESS_LENGTH_HEX) {
			Address addr(r->substr(0,ZT_ADDRESS_LENGTH_HEX).c_str());
			InetAddress phys[2];
			unsigned int physCount = 0;
			const std::size_t semi(r->find(';'));
			if ((semi > ZT_ADDRESS_LENGTH_HEX)&&(semi < (r->length() - 2))) {
				std::vector<std::string> phySplit(Utils::split(r->substr(semi+1).c_str(),",","",""));
				for(std::vector<std::string>::const_iterator p(phySplit.begin());((p!=phySplit.end())&&(physCount < 2));++p) {
					phys[physCount] = InetAddress(*p);
					if (phys[physCount])
						++physCount;
					else phys[physCount].zero();
				}
			}

			unsigned int p = _staticCount;
			for(unsigned int i=0;i<_staticCount;++i) {
				if (_static[p].address == addr.toInt()) {
					p = i;
					break;
				}
			}
			if ((p == _staticCount)&&(_staticCount < ZT_MAX_NETWORK_STATIC_DEVICES))
				++_staticCount;
			if (p < ZT_MAX_NETWORK_STATIC_DEVICES) {
				_static[p].address = Address(r->c_str());
				for(unsigned int i=0;i<physCount;++i)
					_static[p].physical[i] = phys[i];
				_static[p].flags |= ZT_NETWORK_STATIC_DEVICE_IS_RELAY;
			}
		}
	}
	std::sort(&(_static[0]),&(_static[_staticCount]),ZT_VirtualNetworkStaticDevice_SortByAddress());

	std::vector<std::string> ets(Utils::split(d.get(ZT_NETWORKCONFIG_DICT_KEY_ALLOWED_ETHERNET_TYPES,"").c_str(),",","",""));
	int rno = 0;
	for(std::vector<std::string>::const_iterator et(ets.begin());et!=ets.end();++et) {
		unsigned int et2 = Utils::hexStrToUInt(et->c_str()) & 0xffff;
		if (_ruleCount < ZT_MAX_NETWORK_RULES) {
			memset(&(_rules[_ruleCount]),0,sizeof(ZT_VirtualNetworkRule));
			_rules[_ruleCount].ruleNo = rno; rno += 10;
			_rules[_ruleCount].matches = (uint8_t)((et2 == 0) ? ZT_NETWORK_RULE_MATCHES_ALL : ZT_NETWORK_RULE_MATCHES_ETHERTYPE);
			_rules[_ruleCount].action = (uint8_t)ZT_NETWORK_RULE_ACTION_ACCEPT;
			_rules[_ruleCount].datum.etherType = (uint16_t)et2;
			++_ruleCount;
		}
	}

	_com.fromString(d.get(ZT_NETWORKCONFIG_DICT_KEY_CERTIFICATE_OF_MEMBERSHIP,std::string()));
}

#endif // ZT_SUPPORT_OLD_STYLE_NETCONF

} // namespace ZeroTier
