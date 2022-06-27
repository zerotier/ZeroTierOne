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

#include "connection_pool.h"
#include <cassert>
#include "errors.h"

namespace sw {

namespace redis {

ConnectionPool::ConnectionPool(const ConnectionPoolOptions &pool_opts,
        const ConnectionOptions &connection_opts) :
            _opts(connection_opts),
            _pool_opts(pool_opts) {
    if (_pool_opts.size == 0) {
        throw Error("CANNOT create an empty pool");
    }

    // Lazily create connections.
}

ConnectionPool::ConnectionPool(SimpleSentinel sentinel,
                                const ConnectionPoolOptions &pool_opts,
                                const ConnectionOptions &connection_opts) :
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

ConnectionPool::ConnectionPool(ConnectionPool &&that) {
    std::lock_guard<std::mutex> lock(that._mutex);

    _move(std::move(that));
}

ConnectionPool& ConnectionPool::operator=(ConnectionPool &&that) {
    if (this != &that) {
        std::lock(_mutex, that._mutex);
        std::lock_guard<std::mutex> lock_this(_mutex, std::adopt_lock);
        std::lock_guard<std::mutex> lock_that(that._mutex, std::adopt_lock);

        _move(std::move(that));
    }

    return *this;
}

Connection ConnectionPool::fetch() {
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
        auto role_changed = _role_changed(connection.options());
        auto sentinel = _sentinel;

        lock.unlock();

        if (role_changed || _need_reconnect(connection, connection_lifetime, connection_idle_time)) {
            try {
                connection = _create(sentinel, opts, false);
            } catch (const Error &e) {
                // Failed to reconnect, return it to the pool, and retry latter.
                release(std::move(connection));
                throw;
            }
        }

        return connection;
    }

    lock.unlock();

    if (_need_reconnect(connection, connection_lifetime, connection_idle_time)) {
        try {
            connection.reconnect();
        } catch (const Error &e) {
            // Failed to reconnect, return it to the pool, and retry latter.
            release(std::move(connection));
            throw;
        }
    }

    return connection;
}

ConnectionOptions ConnectionPool::connection_options() {
    std::lock_guard<std::mutex> lock(_mutex);

    return _opts;
}

void ConnectionPool::release(Connection connection) {
    {
        std::lock_guard<std::mutex> lock(_mutex);

        _pool.push_back(std::move(connection));
    }

    _cv.notify_one();
}

Connection ConnectionPool::create() {
    std::unique_lock<std::mutex> lock(_mutex);

    auto opts = _opts;

    if (_sentinel) {
        auto sentinel = _sentinel;

        lock.unlock();

        return _create(sentinel, opts, false);
    } else {
        lock.unlock();

        return Connection(opts);
    }
}

ConnectionPool ConnectionPool::clone() {
    std::unique_lock<std::mutex> lock(_mutex);

    auto opts = _opts;
    auto pool_opts = _pool_opts;

    if (_sentinel) {
        auto sentinel = _sentinel;

        lock.unlock();

        return ConnectionPool(sentinel, pool_opts, opts);
    } else {
        lock.unlock();

        return ConnectionPool(pool_opts, opts);
    }
}

void ConnectionPool::_move(ConnectionPool &&that) {
    _opts = std::move(that._opts);
    _pool_opts = std::move(that._pool_opts);
    _pool = std::move(that._pool);
    _used_connections = that._used_connections;
    _sentinel = std::move(that._sentinel);
}

Connection ConnectionPool::_create() {
    if (_sentinel) {
        // Get Redis host and port info from sentinel.
        return _create(_sentinel, _opts, true);
    }

    return Connection(_opts);
}

Connection ConnectionPool::_create(SimpleSentinel &sentinel,
                                    const ConnectionOptions &opts,
                                    bool locked) {
    try {
        auto connection = sentinel.create(opts);

        std::unique_lock<std::mutex> lock(_mutex, std::defer_lock);
        if (!locked) {
            lock.lock();
        }

        const auto &connection_opts = connection.options();
        if (_role_changed(connection_opts)) {
            // Master/Slave has been changed, reconnect all connections.
            _update_connection_opts(connection_opts.host, connection_opts.port);
        }

        return connection;
    } catch (const StopIterError &e) {
        throw Error("Failed to create connection with sentinel");
    }
}

Connection ConnectionPool::_fetch() {
    assert(!_pool.empty());

    auto connection = std::move(_pool.front());
    _pool.pop_front();

    return connection;
}

void ConnectionPool::_wait_for_connection(std::unique_lock<std::mutex> &lock) {
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

bool ConnectionPool::_need_reconnect(const Connection &connection,
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
        if (now - connection.last_active() > connection_idle_time) {
            return true;
        }
    }

    return false;
}

}

}
