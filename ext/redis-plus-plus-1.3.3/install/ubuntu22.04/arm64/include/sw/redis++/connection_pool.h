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

#ifndef SEWENEW_REDISPLUSPLUS_CONNECTION_POOL_H
#define SEWENEW_REDISPLUSPLUS_CONNECTION_POOL_H

#include <cassert>
#include <chrono>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <deque>
#include "connection.h"
#include "sentinel.h"

namespace sw {

namespace redis {

struct ConnectionPoolOptions {
    // Max number of connections, including both in-use and idle ones.
    std::size_t size = 1;

    // Max time to wait for a connection. 0ms means client waits forever.
    std::chrono::milliseconds wait_timeout{0};

    // Max lifetime of a connection. 0ms means we never expire the connection.
    std::chrono::milliseconds connection_lifetime{0};

    // Max idle time of a connection. 0ms means we never expire the connection.
    std::chrono::milliseconds connection_idle_time{0};
};

class ConnectionPool {
public:
    ConnectionPool(const ConnectionPoolOptions &pool_opts,
                    const ConnectionOptions &connection_opts);

    ConnectionPool(SimpleSentinel sentinel,
                    const ConnectionPoolOptions &pool_opts,
                    const ConnectionOptions &connection_opts);

    ConnectionPool() = default;

    ConnectionPool(ConnectionPool &&that);
    ConnectionPool& operator=(ConnectionPool &&that);

    ConnectionPool(const ConnectionPool &) = delete;
    ConnectionPool& operator=(const ConnectionPool &) = delete;

    ~ConnectionPool() = default;

    // Fetch a connection from pool.
    Connection fetch();

    ConnectionOptions connection_options();

    void release(Connection connection);

    // Create a new connection.
    Connection create();

    ConnectionPool clone();

private:
    void _move(ConnectionPool &&that);

    // NOT thread-safe
    Connection _create();

    Connection _create(SimpleSentinel &sentinel, const ConnectionOptions &opts, bool locked);

    Connection _fetch();

    void _wait_for_connection(std::unique_lock<std::mutex> &lock);

    bool _need_reconnect(const Connection &connection,
                            const std::chrono::milliseconds &connection_lifetime,
                            const std::chrono::milliseconds &connection_idle_time) const;

    void _update_connection_opts(const std::string &host, int port) {
        _opts.host = host;
        _opts.port = port;
    }

    bool _role_changed(const ConnectionOptions &opts) const {
        return opts.port != _opts.port || opts.host != _opts.host;
    }

    ConnectionOptions _opts;

    ConnectionPoolOptions _pool_opts;

    std::deque<Connection> _pool;

    std::size_t _used_connections = 0;

    std::mutex _mutex;

    std::condition_variable _cv;

    SimpleSentinel _sentinel;
};

using ConnectionPoolSPtr = std::shared_ptr<ConnectionPool>;

class SafeConnection {
public:
    explicit SafeConnection(ConnectionPool &pool) : _pool(pool), _connection(_pool.fetch()) {
        assert(!_connection.broken());
    }

    SafeConnection(const SafeConnection &) = delete;
    SafeConnection& operator=(const SafeConnection &) = delete;

    SafeConnection(SafeConnection &&) = delete;
    SafeConnection& operator=(SafeConnection &&) = delete;

    ~SafeConnection() {
        _pool.release(std::move(_connection));
    }

    Connection& connection() {
        return _connection;
    }

private:
    ConnectionPool &_pool;
    Connection _connection;
};

// NOTE: This class is similar to `SafeConnection`.
// The difference is that `SafeConnection` tries to avoid copying a std::shared_ptr.
class GuardedConnection {
public:
    explicit GuardedConnection(const ConnectionPoolSPtr &pool) : _pool(pool),
                                                        _connection(_pool->fetch()) {
        assert(!_connection.broken());
    }

    GuardedConnection(const GuardedConnection &) = delete;
    GuardedConnection& operator=(const GuardedConnection &) = delete;

    GuardedConnection(GuardedConnection &&) = default;
    GuardedConnection& operator=(GuardedConnection &&) = default;

    ~GuardedConnection() {
        // If `GuardedConnection` has been moved, `_pool` will be nullptr.
        if (_pool) {
            _pool->release(std::move(_connection));
        }
    }

    Connection& connection() {
        return _connection;
    }

private:
    ConnectionPoolSPtr _pool;
    Connection _connection;
};

using GuardedConnectionSPtr = std::shared_ptr<GuardedConnection>;

}

}

#endif // end SEWENEW_REDISPLUSPLUS_CONNECTION_POOL_H
