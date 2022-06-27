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

#include "async_connection_pool.h"
#include <cassert>
#include <utility>
#include "errors.h"

namespace sw {

namespace redis {

SimpleAsyncSentinel::SimpleAsyncSentinel(const AsyncSentinelSPtr &sentinel,
                                            const std::string &master_name,
                                            Role role) :
                                                _sentinel(sentinel),
                                                _master_name(master_name),
                                                _role(role) {
    if (!_sentinel) {
        throw Error("Sentinel cannot be null");
    }

    if (_role != Role::MASTER && _role != Role::SLAVE) {
        throw Error("Role must be Role::MASTER or Role::SLAVE");
    }
}

AsyncConnectionSPtr SimpleAsyncSentinel::create(const ConnectionOptions &opts,
        const std::shared_ptr<AsyncConnectionPool> &pool,
        EventLoop *loop) {
    auto connection = std::make_shared<AsyncConnection>(opts, loop, AsyncConnectionMode::SENTINEL);

    AsyncSentinel::AsyncSentinelTask task;
    task.pool = pool;
    task.connection = connection;
    task.master_name = _master_name;
    task.role = _role;

    _sentinel->add(std::move(task));

    return connection;
}

AsyncConnectionPool::AsyncConnectionPool(const EventLoopSPtr &loop,
        const ConnectionPoolOptions &pool_opts,
        const ConnectionOptions &connection_opts) :
            _loop(loop),
            _opts(connection_opts),
            _pool_opts(pool_opts) {
    if (_pool_opts.size == 0) {
        throw Error("CANNOT create an empty pool");
    }

    // Lazily create connections.
}

AsyncConnectionPool::AsyncConnectionPool(SimpleAsyncSentinel sentinel,
                                const EventLoopSPtr &loop,
                                const ConnectionPoolOptions &pool_opts,
                                const ConnectionOptions &connection_opts) :
                                    _loop(loop),
                                    _opts(connection_opts),
                                    _pool_opts(pool_opts),
                                    _sentinel(std::move(sentinel)) {
    // In this case, the connection must be of TCP type.
    if (_opts.type != ConnectionType::TCP) {
        throw Error("Sentinel only supports TCP connection");
    }

    if (_opts.connect_timeout == std::chrono::milliseconds(0)
            || _opts.socket_timeout == std::chrono::milliseconds(0)) {
        throw Error("With sentinel, connection timeout and socket timeout cannot be 0");
    }

    // Cleanup connection options.
    _update_connection_opts("", -1);

    assert(_sentinel);
}

AsyncConnectionPool::AsyncConnectionPool(AsyncConnectionPool &&that) {
    std::lock_guard<std::mutex> lock(that._mutex);

    _move(std::move(that));
}

AsyncConnectionPool& AsyncConnectionPool::operator=(AsyncConnectionPool &&that) {
    if (this != &that) {
        std::lock(_mutex, that._mutex);
        std::lock_guard<std::mutex> lock_this(_mutex, std::adopt_lock);
        std::lock_guard<std::mutex> lock_that(that._mutex, std::adopt_lock);

        _move(std::move(that));
    }

    return *this;
}

AsyncConnectionPool::~AsyncConnectionPool() {
    assert(_loop);

    // TODO: what if the connection has been borrowed but not returned?
    // Or we dont' need to worry about that, since it's destructing and
    // all borrowed connections should have been returned.
    for (auto &connection : _pool) {
        // TODO: what if some connection has never been watched? Is it possible?
        _loop->unwatch(std::move(connection));
    }
}

AsyncConnectionSPtr AsyncConnectionPool::fetch() {
    std::unique_lock<std::mutex> lock(_mutex);

    if (_pool.empty()) {
        if (_used_connections == _pool_opts.size) {
            _wait_for_connection(lock);
        } else {
            // Lazily create a new connection.
            auto connection = _create();

            ++_used_connections;

            return connection;
        }
    }

    // _pool is NOT empty.
    auto connection = _fetch();

    auto connection_lifetime = _pool_opts.connection_lifetime;
    auto connection_idle_time = _pool_opts.connection_idle_time;

    if (_sentinel) {
        auto opts = _opts;
        auto role_changed = _role_changed(connection->options());
        auto sentinel = _sentinel;

        lock.unlock();

        if (role_changed || _need_reconnect(*connection, connection_lifetime, connection_idle_time)) {
            try {
                auto tmp_connection = sentinel.create(opts, shared_from_this(), _loop.get());

                std::swap(tmp_connection, connection);

                // Release expired connection.
                // TODO: If `unwatch` throw, we will leak the connection.
                _loop->unwatch(std::move(tmp_connection));
            } catch (const Error &e) {
                // Failed to reconnect, return it to the pool, and retry latter.
                release(std::move(connection));
                throw;
            }
        }

        return connection;
    }

    lock.unlock();

    assert(connection);

    if (_need_reconnect(*connection, connection_lifetime, connection_idle_time)) {
        try {
            auto tmp_connection = _create();

            std::swap(tmp_connection, connection);

            // Release expired connection.
            // TODO: If `unwatch` throw, we will leak the connection.
            _loop->unwatch(std::move(tmp_connection));
        } catch (const Error &e) {
            // Failed, return it to the pool, and retry latter.
            release(std::move(connection));
            throw;
        }
    }

    return connection;
}

ConnectionOptions AsyncConnectionPool::connection_options() {
    std::lock_guard<std::mutex> lock(_mutex);

    return _opts;
}

void AsyncConnectionPool::release(AsyncConnectionSPtr connection) {
    {
        std::lock_guard<std::mutex> lock(_mutex);

        _pool.push_back(std::move(connection));
    }

    _cv.notify_one();
}

AsyncConnectionSPtr AsyncConnectionPool::create() {
    std::unique_lock<std::mutex> lock(_mutex);

    auto opts = _opts;

    if (_sentinel) {
        // TODO: it seems that we don't need to copy sentinel,
        // since it's thread-safe.
        auto sentinel = _sentinel;

        lock.unlock();

        return sentinel.create(opts, shared_from_this(), _loop.get());
    } else {
        lock.unlock();

        return std::make_shared<AsyncConnection>(opts, _loop.get());
    }
}

AsyncConnectionPool AsyncConnectionPool::clone() {
    std::unique_lock<std::mutex> lock(_mutex);

    auto opts = _opts;
    auto pool_opts = _pool_opts;

    if (_sentinel) {
        auto sentinel = _sentinel;

        lock.unlock();

        return AsyncConnectionPool(sentinel, _loop, pool_opts, opts);
    } else {
        lock.unlock();

        return AsyncConnectionPool(_loop, pool_opts, opts);
    }
}

void AsyncConnectionPool::update_node_info(const std::string &host,
        int port,
        AsyncConnectionSPtr &connection) {
    {
        std::lock_guard<std::mutex> lock(_mutex);

        _update_connection_opts(host, port);
    }

    connection->update_node_info(host, port);

    _loop->add(connection);
}

void AsyncConnectionPool::update_node_info(AsyncConnectionSPtr &connection,
        std::exception_ptr err) {
    _loop->unwatch(connection, err);
}

void AsyncConnectionPool::_move(AsyncConnectionPool &&that) {
    _loop = std::move(that._loop);
    _opts = std::move(that._opts);
    _pool_opts = std::move(that._pool_opts);
    _pool = std::move(that._pool);
    _used_connections = that._used_connections;
    _sentinel = std::move(that._sentinel);
}

AsyncConnectionSPtr AsyncConnectionPool::_create() {
    if (_sentinel) {
        // Get Redis host and port info from sentinel.
        // In this case, the mutex has been locked.
        return _sentinel.create(_opts, shared_from_this(), _loop.get());
    }

    return std::make_shared<AsyncConnection>(_opts, _loop.get());
}

AsyncConnectionSPtr AsyncConnectionPool::_fetch() {
    assert(!_pool.empty());

    auto connection = std::move(_pool.front());
    _pool.pop_front();

    return connection;
}

void AsyncConnectionPool::_wait_for_connection(std::unique_lock<std::mutex> &lock) {
    auto timeout = _pool_opts.wait_timeout;
    if (timeout > std::chrono::milliseconds(0)) {
        // Wait until _pool is no longer empty or timeout.
        if (!_cv.wait_for(lock,
                    timeout,
                    [this] { return !(this->_pool).empty(); })) {
            throw Error("Failed to fetch a connection in "
                    + std::to_string(timeout.count()) + " milliseconds");
        }
    } else {
        // Wait forever.
        _cv.wait(lock, [this] { return !(this->_pool).empty(); });
    }
}

bool AsyncConnectionPool::_need_reconnect(const AsyncConnection &connection,
                                    const std::chrono::milliseconds &connection_lifetime,
                                    const std::chrono::milliseconds &connection_idle_time) const {
    if (connection.broken()) {
        return true;
    }

    auto now = std::chrono::steady_clock::now();
    if (connection_lifetime > std::chrono::milliseconds(0)) {
        if (now - connection.create_time() > connection_lifetime) {
            return true;
        }
    }

    if (connection_idle_time > std::chrono::milliseconds(0)) {
        if (now.time_since_epoch() - connection.last_active() > connection_idle_time) {
            return true;
        }
    }

    return false;
}

bool AsyncConnectionPool::_role_changed(const ConnectionOptions &opts) const {
    if (opts.host.empty()) {
        // Still waiting for sentinel.
        return false;
    }

    return opts.port != _opts.port || opts.host != _opts.host;
}

}

}
