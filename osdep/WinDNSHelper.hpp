#ifndef WIN_DNS_HELPER_H_
#define WIN_DNS_HELPER_H_

#include <vector>
#include <cstdint>
#include "../node/InetAddress.hpp"


namespace ZeroTier 
{

class WinDNSHelper
{
public:
	static void setDNS(uint64_t nwid, const char* domain, const std::vector<InetAddress>& servers);
	static void removeDNS(uint64_t nwid);

private:
	static std::pair<bool, std::string> hasDNSConfig(uint64_t nwid);
};

}

#endif