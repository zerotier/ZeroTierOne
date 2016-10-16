#ifndef ZT_LINUXDROPPRIVILEGES_HPP
#define ZT_LINUXDROPPRIVILEGES_HPP
#include <string>

namespace ZeroTier {
    void dropPrivileges(std::string homeDir);
}

#endif
