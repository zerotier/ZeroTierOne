#ifndef ZT_CONTROLLER_REDIS_HPP
#define ZT_CONTROLLER_REDIS_HPP

#include <string>

namespace ZeroTier {
struct RedisConfig {
    std::string hostname;
    int port;
    std::string password;
    bool clusterMode;
};
}

#endif