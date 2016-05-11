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

#ifdef ZT_SUPPORT_OLD_STYLE_NETCONF

void NetworkConfig::fromDictionary(const char *ds,unsigned int dslen)
{
	static const std::string zero("0");
	static const std::string one("1");

	Dictionary d(ds,dslen);

	memset(this,0,sizeof(NetworkConfig));

	// NOTE: d.get(name) throws if not found, d.get(name,default) returns default

	networkId = Utils::hexStrToU64(d.get(ZT_NETWORKCONFIG_DICT_KEY_NETWORK_ID,"0").c_str());
	if (!networkId)
		throw std::invalid_argument("configuration contains zero network ID");

	timestamp = Utils::hexStrToU64(d.get(ZT_NETWORKCONFIG_DICT_KEY_TIMESTAMP,"0").c_str());
	revision = Utils::hexStrToU64(d.get(ZT_NETWORKCONFIG_DICT_KEY_REVISION,"1").c_str()); // older controllers don't send this, so default to 1
	issuedTo = Address(d.get(ZT_NETWORKCONFIG_DICT_KEY_ISSUED_TO,"0"));

	multicastLimit = Utils::hexStrToUInt(d.get(ZT_NETWORKCONFIG_DICT_KEY_MULTICAST_LIMIT,zero).c_str());
	if (multicastLimit == 0) multicastLimit = ZT_MULTICAST_DEFAULT_LIMIT;

	flags |= ((Utils::hexStrToUInt(d.get(ZT_NETWORKCONFIG_DICT_KEY_ALLOW_PASSIVE_BRIDGING,zero).c_str()) != 0) ? ZT_NETWORKCONFIG_FLAG_ALLOW_PASSIVE_BRIDGING : 0);
	flags |= ((Utils::hexStrToUInt(d.get(ZT_NETWORKCONFIG_DICT_KEY_ENABLE_BROADCAST,one).c_str()) != 0) ? ZT_NETWORKCONFIG_FLAG_ENABLE_BROADCAST : 0);

	this->type = (Utils::hexStrToUInt(d.get(ZT_NETWORKCONFIG_DICT_KEY_PRIVATE,one).c_str()) != 0) ? ZT_NETWORK_TYPE_PRIVATE : ZT_NETWORK_TYPE_PUBLIC;

	std::string nametmp(d.get(ZT_NETWORKCONFIG_DICT_KEY_NAME,""));
	for(unsigned long i=0;((i<ZT_MAX_NETWORK_SHORT_NAME_LENGTH)&&(i<nametmp.length()));++i)
		name[i] = (char)nametmp[i];
	// we zeroed the entire structure above and _name is ZT_MAX_NETWORK_SHORT_NAME_LENGTH+1, so it will always null-terminate

	std::vector<std::string> activeBridgesSplit(Utils::split(d.get(ZT_NETWORKCONFIG_DICT_KEY_ACTIVE_BRIDGES,"").c_str(),",","",""));
	for(std::vector<std::string>::const_iterator a(activeBridgesSplit.begin());a!=activeBridgesSplit.end();++a) {
		if (a->length() == ZT_ADDRESS_LENGTH_HEX) { // ignore empty or garbage fields
			Address tmp(*a);
			if (!tmp.isReserved()) {
				uint64_t specialist = tmp.toInt();
				for(unsigned int i=0;i<specialistCount;++i) {
					if ((specialists[i] & 0xffffffffffULL) == specialist) {
						specialists[i] |= ZT_NETWORKCONFIG_SPECIALIST_TYPE_ACTIVE_BRIDGE;
						specialist = 0;
						break;
					}
				}
				if ((specialist)&&(specialistCount < ZT_MAX_NETWORK_SPECIALISTS))
					specialists[specialistCount++] = specialist | ZT_NETWORKCONFIG_SPECIALIST_TYPE_ACTIVE_BRIDGE;
			}
		}
	}

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
		if (!addr.isNetwork()) {
			if ((staticIpCount < ZT_MAX_ZT_ASSIGNED_ADDRESSES)&&(std::find(&(staticIps[0]),&(staticIps[staticIpCount]),addr) == &(staticIps[staticIpCount])))
				staticIps[staticIpCount++] = addr;
		}
	}
	std::sort(&(staticIps[0]),&(staticIps[staticIpCount]));

	/* Old versions don't support gateways anyway, so ignore this in old netconfs
	std::vector<std::string> gatewaysSplit(Utils::split(d.get(ZT_NETWORKCONFIG_DICT_KEY_GATEWAYS,"").c_str(),",","",""));
	for(std::vector<std::string>::const_iterator gwstr(gatewaysSplit.begin());gwstr!=gatewaysSplit.end();++gwstr) {
		InetAddress gw(*gwstr);
		if ((gw)&&(_gatewayCount < ZT_MAX_NETWORK_GATEWAYS)&&(std::find(&(_gateways[0]),&(_gateways[_gatewayCount]),gw) == &(_gateways[_gatewayCount])))
			_gateways[_gatewayCount++] = gw;
	}
	std::sort(&(_gateways[0]),&(_gateways[_gatewayCount]));
	*/

	std::vector<std::string> relaysSplit(Utils::split(d.get(ZT_NETWORKCONFIG_DICT_KEY_RELAYS,"").c_str(),",","",""));
	for(std::vector<std::string>::const_iterator r(relaysSplit.begin());r!=relaysSplit.end();++r) {
		if (r->length() >= ZT_ADDRESS_LENGTH_HEX) {
			Address zt(r->substr(0,ZT_ADDRESS_LENGTH_HEX).c_str());
			InetAddress phy[2];
			unsigned int phyCount = 0;
			const std::size_t semi(r->find(';'));
			if ((semi > ZT_ADDRESS_LENGTH_HEX)&&(semi < (r->length() - 2))) {
				std::vector<std::string> phySplit(Utils::split(r->substr(semi+1).c_str(),",","",""));
				for(std::vector<std::string>::const_iterator p(phySplit.begin());((p!=phySplit.end())&&(phyCount < 2));++p) {
					phy[phyCount] = InetAddress(*p);
					if (phy[phyCount])
						++phyCount;
					else phy[phyCount].zero();
				}
			}

			uint64_t specialist = zt.toInt();
			for(unsigned int i=0;i<specialistCount;++i) {
				if ((specialists[i] & 0xffffffffffULL) == specialist) {
					specialists[i] |= ZT_NETWORKCONFIG_SPECIALIST_TYPE_NETWORK_PREFERRED_RELAY;
					specialist = 0;
					break;
				}
			}

			if ((specialist)&&(specialistCount < ZT_MAX_NETWORK_SPECIALISTS))
				specialists[specialistCount++] = specialist | ZT_NETWORKCONFIG_SPECIALIST_TYPE_NETWORK_PREFERRED_RELAY;

			if ((phy[0])&&(pinnedCount < ZT_MAX_NETWORK_PINNED)) {
				pinned[pinnedCount].zt = zt;
				pinned[pinnedCount].phy = phy[0];
				++pinnedCount;
			}
			if ((phy[1])&&(pinnedCount < ZT_MAX_NETWORK_PINNED)) {
				pinned[pinnedCount].zt = zt;
				pinned[pinnedCount].phy = phy[0];
				++pinnedCount;
			}
		}
	}

	std::vector<std::string> ets(Utils::split(d.get(ZT_NETWORKCONFIG_DICT_KEY_ALLOWED_ETHERNET_TYPES,"").c_str(),",","",""));
	for(std::vector<std::string>::const_iterator et(ets.begin());et!=ets.end();++et) {
		unsigned int et2 = Utils::hexStrToUInt(et->c_str()) & 0xffff;
		if ((ruleCount + 1) < ZT_MAX_NETWORK_RULES) {
			if (et2) {
				rules[ruleCount].t = ZT_NETWORK_RULE_MATCH_ETHERTYPE;
				rules[ruleCount].v.etherType = (uint16_t)et2;
				++ruleCount;
			}
			rules[ruleCount++].t = ZT_NETWORK_RULE_ACTION_ACCEPT;
		}
	}

	this->com.fromString(d.get(ZT_NETWORKCONFIG_DICT_KEY_CERTIFICATE_OF_MEMBERSHIP,std::string()));
}

#endif // ZT_SUPPORT_OLD_STYLE_NETCONF

} // namespace ZeroTier
