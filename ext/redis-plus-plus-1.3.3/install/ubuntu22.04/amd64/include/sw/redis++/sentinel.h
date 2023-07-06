/**************************************************************************
   Copyright (c) 2017 sewenew

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 *************************************************************************/

#ifndef SEWENEW_REDISPLUSPLUS_SENTINEL_H
#define SEWENEW_REDISPLUSPLUS_SENTINEL_H

#include <string>
#include <list>
#include <vector>
#include <memory>
#include <mutex>
#include "connection.h"
#include "shards.h"
#include "reply.h"
#include "tls.h"

namespace sw {

namespace redis {

struct SentinelOptions {
    std::vector<std::pair<std::string, int>> nodes;

    std::string password;

    bool keep_alive = true;

    std::chrono::milliseconds connect_timeout{100};

    std::chrono::milliseconds socket_timeout{100};

    std::chrono::milliseconds retry_interval{100};

    std::size_t max_retry = 2;

    tls::TlsOptions tls;
};

class Sentinel {
public:
    explicit Sentinel(const SentinelOptions &sentinel_opts);

    Sentinel(const Sentinel &) = delete;
    Sentinel& operator=(const Sentinel &) = delete;

    Sentinel(Sentinel &&) = delete;
    Sentinel& operator=(Sentinel &&) = delete;

    ~Sentinel() = default;

private:
    Connection master(const std::string &master_name, const ConnectionOptions &opts);

    Connection slave(const std::string &master_name, const ConnectionOptions &opts);

    class Iterator;

    friend class SimpleSentinel;

    std::list<ConnectionOptions> _parse_options(const SentinelOptions &opts) const;

    Optional<Node> _get_master_addr_by_name(Connection &connection, const StringView &name);

    std::vector<Node> _get_slave_addr_by_name(Connection &connection, const StringView &name);

    Connection _connect_redis(const Node &node, ConnectionOptions opts);

    Role _get_role(Connection &connection);

    std::vector<Node> _parse_slave_info(redisReply &reply) const;

    std::list<Connection> _healthy_sentinels;

    std::list<ConnectionOptions> _broken_sentinels;

    SentinelOptions _sentinel_opts;

    std::mutex _mutex;
};

class SimpleSentinel {
public:
    SimpleSentinel(const std::shared_ptr<Sentinel> &sentinel,
                    const std::string &master_name,
                    Role role);

    SimpleSentinel() = default;

    SimpleSentinel(const SimpleSentinel &) = default;
    SimpleSentinel& operator=(const SimpleSentinel &) = default;

    SimpleSentinel(SimpleSentinel &&) = default;
    SimpleSentinel& operator=(SimpleSentinel &&) = default;

    ~SimpleSentinel() = default;

    explicit operator bool() const {
        return bool(_sentinel);
    }

    Connection create(const ConnectionOptions &opts);

private:
    std::shared_ptr<Sentinel> _sentinel;

    std::string _master_name;

    Role _role = Role::MASTER;
};

class StopIterError : public Error {
public:
    StopIterError() : Error("StopIterError") {}

    StopIterError(const StopIterError &) = default;
    StopIterError& operator=(const StopIterError &) = default;

    StopIterError(StopIterError &&) = default;
    StopIterError& operator=(StopIterError &&) = default;

    virtual ~StopIterError() override = default;
};

}

}

#endif // end SEWENEW_REDISPLUSPLUS_SENTINEL_H
