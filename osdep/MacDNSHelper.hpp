#ifndef MAC_DNS_HELPER
#define MAC_DNS_HELPER

#include <vector>
#include "../node/InetAddress.hpp"
#include "../node/MAC.hpp"

namespace ZeroTier {

class MacDNSHelper
{
public:
    static void setDNS(uint64_t nwid, const char *domain, const std::vector<InetAddress> &servers);
    static void removeDNS(uint64_t nwid);
    static bool addIps4(uint64_t nwid, const MAC mac, const char *dev, const std::vector<InetAddress> &addrs);
    static bool addIps6(uint64_t nwid, const MAC mac, const char *dev, const std::vector<InetAddress> &addrs);
    static bool removeIps4(uint64_t nwid);
    static bool removeIps6(uint64_t nwid);
};

}

#endif
