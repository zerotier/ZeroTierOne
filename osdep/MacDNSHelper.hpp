#ifndef MAC_DNS_HELPER
#define MAC_DNS_HELPER

#include <vector>
#include "../node/InetAddress.hpp"

namespace ZeroTier {

class MacDNSHelper
{
public:
    static void doTheThing();

    static void setDNS(uint64_t nwid, const char *domain, const std::vector<InetAddress> &servers);
    static void removeDNS(uint64_t nwid);
};

}

#endif
