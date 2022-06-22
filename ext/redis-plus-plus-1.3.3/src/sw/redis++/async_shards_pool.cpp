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

#include "async_shards_pool.h"
#include <cassert>
#include "errors.h"

namespace sw {

namespace redis {

const std::size_t AsyncShardsPool::SHARDS;

AsyncShardsPool::AsyncShardsPool(const EventLoopSPtr &loop,
        const ConnectionPoolOptions &pool_opts,
        const ConnectionOptions &connection_opts,
        Role role) :
            _pool_opts(pool_opts),
            _connection_opts(connection_opts),
            _role(role),
            _loop(loop) {
    assert(_loop);

    if (_connection_opts.type != ConnectionType::TCP) {
        throw Error("Only support TCP connection for Redis Cluster");
    }

    auto node = Node{_connection_opts.host, _connection_opts.port};
    _shards.emplace(SlotRange{0U, SHARDS}, node);
    _pools.emplace(node,
            std::make_shared<AsyncConnectionPool>(_loop, _pool_opts, _connection_opts));

    _worker = std::thread([this]() { this->_run(); });
}

AsyncShardsPool::AsyncShardsPool(AsyncShardsPool &&that) {
    std::lock_guard<std::mutex> lock(that._mutex);

    _move(std::move(that));
}

AsyncShardsPool& AsyncShardsPool::operator=(AsyncShardsPool &&that) {
    if (this != &that) {
        std::lock(_mutex, that._mutex);
        std::lock_guard<std::mutex> lock_this(_mutex, std::adopt_lock);
        std::lock_guard<std::mutex> lock_that(that._mutex, std::adopt_lock);

        _move(std::move(that));
    }

    return *this;
}

AsyncShardsPool::~AsyncShardsPool() {
    update({}, nullptr);

    if (_worker.joinable()) {
        _worker.join();
    }
}

AsyncConnectionPoolSPtr AsyncShardsPool::fetch(const StringView &key) {
    auto slot = _slot(key);

    return _fetch(slot);
}

AsyncConnectionPoolSPtr AsyncShardsPool::fetch() {
    auto slot = _slot();

    return _fetch(slot);
}

AsyncConnectionPoolSPtr AsyncShardsPool::fetch(const Node &node) {
    std::lock_guard<std::mutex> lock(_mutex);

    auto iter = _pools.find(node);
    if (iter == _pools.end()) {
        // Node doesn't exist, and it should be a newly created node.
        // So add a new connection pool.
        iter = _add_node(node);
    }

    assert(iter != _pools.end());

    return iter->second;
}

void AsyncShardsPool::update(const std::string &key, AsyncEventUPtr event) {
    {
        std::lock_guard<std::mutex> lock(_mutex);

        _events.push(RedeliverEvent{key, std::move(event)});
    }

    _cv.notify_one();
}

ConnectionOptions AsyncShardsPool::connection_options(const StringView &key) {
    auto slot = _slot(key);

    return _connection_options(slot);
}

ConnectionOptions AsyncShardsPool::connection_options() {
    auto slot = _slot();

    return _connection_options(slot);
}

void AsyncShardsPool::_move(AsyncShardsPool &&that) {
    _pool_opts = that._pool_opts;
    _connection_opts = that._connection_opts;
    _role = that._role;
    _shards = std::move(that._shards);
    _pools = std::move(that._pools);
    _loop = std::move(that._loop);
    _worker = std::move(that._worker);
    _events = std::move(that._events);
}

ConnectionOptions AsyncShardsPool::_connection_options(Slot slot) {
    std::lock_guard<std::mutex> lock(_mutex);

    auto &pool = _get_pool(slot);

    assert(pool);

    return pool->connection_options();
}

Slot AsyncShardsPool::_slot(const StringView &key) const {
    // The following code is copied from: https://redis.io/topics/cluster-spec
    // And I did some minor changes.

    const auto *k = key.data();
    auto keylen = key.size();

    // start-end indexes of { and }.
    std::size_t s = 0;
    std::size_t e = 0;

    // Search the first occurrence of '{'.
    for (s = 0; s < keylen; s++)
        if (k[s] == '{') break;

    // No '{' ? Hash the whole key. This is the base case.
    if (s == keylen) return crc16(k, keylen) & SHARDS;

    // '{' found? Check if we have the corresponding '}'.
    for (e = s + 1; e < keylen; e++)
        if (k[e] == '}') break;

    // No '}' or nothing between {} ? Hash the whole key.
    if (e == keylen || e == s + 1) return crc16(k, keylen) & SHARDS;

    // If we are here there is both a { and a } on its right. Hash
    // what is in the middle between { and }.
    return crc16(k + s + 1, e - s - 1) & SHARDS;
}

Slot AsyncShardsPool::_slot() const {
    return _random(0, SHARDS);
}

AsyncConnectionPoolSPtr AsyncShardsPool::_fetch(Slot slot) {
    std::lock_guard<std::mutex> lock(_mutex);

    return _get_pool(slot);
}

AsyncConnectionPoolSPtr& AsyncShardsPool::_get_pool(Slot slot) {
    const auto &node = _get_node(slot);

    auto node_iter = _pools.find(node);
    if (node_iter == _pools.end()) {
        throw Error("Slot is NOT covered: " + std::to_string(slot));
    }

    return node_iter->second;
}

void AsyncShardsPool::_run() {
    while (true) {
        auto events = _fetch_events();

        assert(!events.empty());

        try {
            // TODO: when we try to stop the worker thread,
            // we don't need to call `_update_shards`
            _update_shards();

            // if _redeliver_events or _fail_events returns true if there's a null event,
            // and we exit the thread loop.
            if (_redeliver_events(events)) {
                break;
            }
        } catch (...) {
            if (_fail_events(events, std::current_exception())) {
                break;
            }
        }
    }
}

auto AsyncShardsPool::_fetch_events() -> std::queue<RedeliverEvent> {
    std::queue<RedeliverEvent> events;

    std::unique_lock<std::mutex> lock(_mutex);
    if (_events.empty()) {
        _cv.wait(lock, [this]() { return !(this->_events).empty(); } );
    }

    events.swap(_events);

    return events;
}

std::size_t AsyncShardsPool::_random(std::size_t min, std::size_t max) const {
    static thread_local std::default_random_engine engine;

    std::uniform_int_distribution<std::size_t> uniform_dist(min, max);

    return uniform_dist(engine);
}

const Node& AsyncShardsPool::_get_node(Slot slot) const {
    auto shards_iter = _shards.lower_bound(SlotRange{slot, slot});
    if (shards_iter == _shards.end() || slot < shards_iter->first.min) {
        throw Error("Slot is out of range: " + std::to_string(slot));
    }

    return shards_iter->second;
}

Shards AsyncShardsPool::_get_shards(const std::string &host, int port) {
    auto opts = _connection_opts;
    opts.host = host;
    opts.port = port;
    ShardsPool pool(_pool_opts, opts, _role);

    return pool.shards();
}

void AsyncShardsPool::_update_shards() {
    for (int idx = 0; idx < 4; ++idx) {
        try {
            Shards shards;
            if (idx < 3) {
                Node node;
                {
                    std::lock_guard<std::mutex> lock(_mutex);

                    // Randomly pick a node.
                    node = _get_node(_slot());
                }

                shards = _get_shards(node.host, node.port);
            } else {
                shards = _get_shards(_connection_opts.host, _connection_opts.port);
            }

            std::unordered_set<Node, NodeHash> nodes;
            for (const auto &shard : shards) {
                nodes.insert(shard.second);
            }

            std::lock_guard<std::mutex> lock(_mutex);

            _shards = std::move(shards);

            // Remove non-existent nodes.
            for (auto iter = _pools.begin(); iter != _pools.end(); ) {
                if (nodes.find(iter->first) == nodes.end()) {
                    // Node has been removed.
                    _pools.erase(iter++);
                } else {
                    ++iter;
                }
            }

            // Add connection pool for new nodes.
            // In fact, connections will be created lazily.
            for (const auto &node : nodes) {
                if (_pools.find(node) == _pools.end()) {
                    _add_node(node);
                }
            }

            // Update successfully.
            return;
        } catch (const Error &e) {
            // continue;
        }
    }

    throw Error("Failed to update shards info");
}

auto AsyncShardsPool::_add_node(const Node &node) -> NodeMap::iterator {
    auto opts = _connection_opts;
    opts.host = node.host;
    opts.port = node.port;

    // TODO: Better set readonly an attribute of `Node`.
    if (_role == Role::SLAVE) {
        opts.readonly = true;
    }

    return _pools.emplace(node,
            std::make_shared<AsyncConnectionPool>(_loop, _pool_opts, opts)).first;
}

bool AsyncShardsPool::_redeliver_events(std::queue<RedeliverEvent> &events) {
    bool should_stop_worker = false;
    while (!events.empty()) {
        auto &event = events.front();
        try {
            auto &async_event = event.event;
            if (!async_event) {
                should_stop_worker = true;
                events.pop();
                continue;
            }

            auto pool = fetch(event.key);
            assert(pool);

            GuardedAsyncConnection connection(pool);

            connection.connection().send(std::move(async_event));
        } catch (...) {
            event.event->set_exception(std::current_exception());
        }
        events.pop();
    }

    return should_stop_worker;
}

bool AsyncShardsPool::_fail_events(std::queue<RedeliverEvent> &events,
        std::exception_ptr err) {
    bool should_stop_worker = false;
    while (!events.empty()) {
        auto &event = events.front(); 
        auto &async_event = event.event;
        if (!async_event) {
            should_stop_worker = true;
        } else {
            async_event->set_exception(err);
        }
        events.pop();
    }

    return should_stop_worker;
}

}

}
