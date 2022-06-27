/**************************************************************************
   Copyright (c) 2021 sewenew

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

#ifndef SEWENEW_REDISPLUSPLUS_ASYNC_CONNECTION_POOL_H
#define SEWENEW_REDISPLUSPLUS_ASYNC_CONNECTION_POOL_H

#include <cassert>
#include <unordered_set>
#include <chrono>
#include <mutex>
#include <memory>
#include <exception>
#include <condition_variable>
#include <deque>
#include "connection.h"
#include "connection_pool.h"
#include "async_connection.h"
#include "async_sentinel.h"

namespace sw {

namespace redis {

class AsyncConnectionPool;

class SimpleAsyncSentinel {
public:
    SimpleAsyncSentinel(const AsyncSentinelSPtr &sentinel,
                        const std::string &master_name,
                        Role role);

    SimpleAsyncSentinel() = default;

    SimpleAsyncSentinel(const SimpleAsyncSentinel &) = default;
    SimpleAsyncSentinel& operator=(const SimpleAsyncSentinel &) = default;

    SimpleAsyncSentinel(SimpleAsyncSentinel &&) = default;
    SimpleAsyncSentinel& operator=(SimpleAsyncSentinel &&) = default;

    ~SimpleAsyncSentinel() = default;

    explicit operator bool() const {
        return bool(_sentinel);
    }

    AsyncConnectionSPtr create(const ConnectionOptions &opts,
            const std::shared_ptr<AsyncConnectionPool> &pool,
            EventLoop *loop);

private:
    AsyncSentinelSPtr _sentinel;

    std::string _master_name;

    Role _role = Role::MASTER;
};

class AsyncConnectionPool : public std::enable_shared_from_this<AsyncConnectionPool> {
public:
    AsyncConnectionPool(const EventLoopSPtr &loop,
                    const ConnectionPoolOptions &pool_opts,
                    const ConnectionOptions &connection_opts);

    AsyncConnectionPool(SimpleAsyncSentinel sentinel,
                    const EventLoopSPtr &loop,
                    const ConnectionPoolOptions &pool_opts,
                    const ConnectionOptions &connection_opts);

    AsyncConnectionPool(AsyncConnectionPool &&that);
    AsyncConnectionPool& operator=(AsyncConnectionPool &&that);

    AsyncConnectionPool(const AsyncConnectionPool &) = delete;
    AsyncConnectionPool& operator=(const AsyncConnectionPool &) = delete;

    ~AsyncConnectionPool();

    // Fetch a connection from pool.
    AsyncConnectionSPtr fetch();

    ConnectionOptions connection_options();

    void release(AsyncConnectionSPtr connection);

    // Create a new connection.
    AsyncConnectionSPtr create();

    AsyncConnectionPool clone();

    // These update_node_info overloads are called by AsyncSentinel.
    void update_node_info(const std::string &host,
            int port,
            AsyncConnectionSPtr &connection);

    void update_node_info(AsyncConnectionSPtr &connection,
            std::exception_ptr err);

private:
    void _move(AsyncConnectionPool &&that);

    // NOT thread-safe
    AsyncConnectionSPtr _create();

    AsyncConnectionSPtr _fetch();

    void _wait_for_connection(std::unique_lock<std::mutex> &lock);

    bool _need_reconnect(const AsyncConnection &connection,
                            const std::chrono::milliseconds &connection_lifetime,
                            const std::chrono::milliseconds &connection_idle_time) const;

    void _update_connection_opts(const std::string &host, int port) {
        _opts.host = host;
        _opts.port = port;
    }

    bool _role_changed(const ConnectionOptions &opts) const;

    EventLoopSPtr _loop;

    ConnectionOptions _opts;

    ConnectionPoolOptions _pool_opts;

    std::deque<AsyncConnectionSPtr> _pool;

    std::size_t _used_connections = 0;

    std::mutex _mutex;

    std::condition_variable _cv;

    SimpleAsyncSentinel _sentinel;
};

using AsyncConnectionPoolSPtr = std::shared_ptr<AsyncConnectionPool>;

class SafeAsyncConnection {
public:
    explicit SafeAsyncConnection(AsyncConnectionPool &pool) : _pool(pool), _connection(_pool.fetch()) {
        assert(_connection);
    }

    SafeAsyncConnection(const SafeAsyncConnection &) = delete;
    SafeAsyncConnection& operator=(const SafeAsyncConnection &) = delete;

    SafeAsyncConnection(SafeAsyncConnection &&) = delete;
    SafeAsyncConnection& operator=(SafeAsyncConnection &&) = delete;

    ~SafeAsyncConnection() {
        _pool.release(std::move(_connection));
    }

    AsyncConnection& connection() {
        assert(_connection);

        return *_connection;
    }

private:
    AsyncConnectionPool &_pool;
    AsyncConnectionSPtr _connection;
};

}

}

#endif // end SEWENEW_REDISPLUSPLUS_ASYNC_CONNECTION_POOL_H
