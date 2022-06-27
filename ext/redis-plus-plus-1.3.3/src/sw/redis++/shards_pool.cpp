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

#include "shards_pool.h"
#include <unordered_set>
#include "errors.h"

namespace sw {

namespace redis {

const std::size_t ShardsPool::SHARDS;

ShardsPool::ShardsPool(const ConnectionPoolOptions &pool_opts,
                        const ConnectionOptions &connection_opts,
                        Role role) :
                            _pool_opts(pool_opts),
                            _connection_opts(connection_opts),
                            _role(role) {
    if (_connection_opts.type != ConnectionType::TCP) {
        throw Error("Only support TCP connection for Redis Cluster");
    }

    Connection connection(_connection_opts);

    _shards = _cluster_slots(connection);

    _init_pool(_shards);
}

ShardsPool::ShardsPool(ShardsPool &&that) {
    std::lock_guard<std::mutex> lock(that._mutex);

    _move(std::move(that));
}

ShardsPool& ShardsPool::operator=(ShardsPool &&that) {
    if (this != &that) {
        std::lock(_mutex, that._mutex);
        std::lock_guard<std::mutex> lock_this(_mutex, std::adopt_lock);
        std::lock_guard<std::mutex> lock_that(that._mutex, std::adopt_lock);

        _move(std::move(that));
    }

    return *this;
}

ConnectionPoolSPtr ShardsPool::fetch(const StringView &key) {
    auto slot = _slot(key);

    return _fetch(slot);
}

ConnectionPoolSPtr ShardsPool::fetch() {
    auto slot = _slot();

    return _fetch(slot);
}

ConnectionPoolSPtr ShardsPool::fetch(const Node &node) {
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

void ShardsPool::update() {
    // My might send command to a removed node.
    // Try at most 3 times from the current shard masters and finally with the user given connection options.
    for (auto idx = 0; idx < 4; ++idx) {
        try {
            Shards shards;
            if (idx < 3) {
                // Randomly pick a connection.
                auto pool = fetch();
                assert(pool);
                SafeConnection safe_connection(*pool);
                shards = _cluster_slots(safe_connection.connection());
            }
            else {
                Connection connection(_connection_opts);
                shards = _cluster_slots(connection);
            }


            std::unordered_set<Node, NodeHash> nodes;
            for (const auto &shard : shards) {
                nodes.insert(shard.second);
            }

            std::lock_guard<std::mutex> lock(_mutex);

            // TODO: If shards is unchanged, no need to update, and return immediately.

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
        } catch (const Error &) {
            // continue;
        }
    }

    throw Error("Failed to update shards info");
}

ConnectionOptions ShardsPool::connection_options(const StringView &key) {
    auto slot = _slot(key);

    return _connection_options(slot);
}

ConnectionOptions ShardsPool::connection_options() {
    auto slot = _slot();

    return _connection_options(slot);
}

Shards ShardsPool::shards() {
    std::lock_guard<std::mutex> lock(_mutex);

    return _shards;
}

void ShardsPool::_move(ShardsPool &&that) {
    _pool_opts = that._pool_opts;
    _connection_opts = that._connection_opts;
    _shards = std::move(that._shards);
    _pools = std::move(that._pools);
    _role = that._role;
}

void ShardsPool::_init_pool(const Shards &shards) {
    for (const auto &shard : shards) {
        _add_node(shard.second);
    }
}

Shards ShardsPool::_cluster_slots(Connection &connection) const {
    auto reply = _cluster_slots_command(connection);

    assert(reply);

    return _parse_reply(*reply);
}

ReplyUPtr ShardsPool::_cluster_slots_command(Connection &connection) const {
    connection.send("CLUSTER SLOTS");

    return connection.recv();
}

Shards ShardsPool::_parse_reply(redisReply &reply) const {
    if (!reply::is_array(reply)) {
        throw ProtoError("Expect ARRAY reply");
    }

    if (reply.element == nullptr || reply.elements == 0) {
        throw Error("Empty slots");
    }

    Shards shards;
    for (std::size_t idx = 0; idx != reply.elements; ++idx) {
        auto *sub_reply = reply.element[idx];
        if (sub_reply == nullptr) {
            throw ProtoError("Null slot info");
        }

        shards.emplace(_parse_slot_info(*sub_reply));
    }

    return shards;
}

Slot ShardsPool::_parse_slot(redisReply *reply) const {
    if (reply == nullptr) {
        throw ProtoError("null slot id");
    }

    auto slot = reply::parse<long long>(*reply);
    if (slot < 0) {
        throw ProtoError("negative slot id");
    }

    return static_cast<Slot>(slot);
}

Node ShardsPool::_parse_node(redisReply *reply) const {
    if (reply == nullptr
            || !reply::is_array(*reply)
            || reply->element == nullptr
            || reply->elements < 2) {
        throw ProtoError("invalid node info");
    }

    auto host = reply::parse<std::string>(*(reply->element[0]));
    int port = reply::parse<long long>(*(reply->element[1]));

    return {host, port};
}

std::pair<SlotRange, Node> ShardsPool::_parse_slot_info(redisReply &reply) const {
    // Slot info is an array reply: min slot, max slot, master node, [slave nodes]
    if (reply.elements < 3 || reply.element == nullptr) {
        throw ProtoError("Invalid slot info");
    }

    auto min_slot = _parse_slot(reply.element[0]);

    auto max_slot = _parse_slot(reply.element[1]);

    if (min_slot > max_slot) {
        throw ProtoError("Invalid slot range");
    }

    auto slot_range = SlotRange{min_slot, max_slot};

    switch (_role) {
    case Role::MASTER:
        // Return master node, i.e. `reply.element[2]`.
        return std::make_pair(slot_range, _parse_node(reply.element[2]));

    case Role::SLAVE: {
        auto size = reply.elements;
        if (size <= 3) {
            throw Error("no slave node available");
        }

        // Randomly pick a slave node.
        auto *slave_node_reply = reply.element[_random(3, size - 1)];

        return std::make_pair(slot_range, _parse_node(slave_node_reply));
    }

    default:
        throw Error("unknown role");
    }
}

Slot ShardsPool::_slot(const StringView &key) const {
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

Slot ShardsPool::_slot() const {
    return _random(0, SHARDS);
}

std::size_t ShardsPool::_random(std::size_t min, std::size_t max) const {
    static thread_local std::default_random_engine engine;

    std::uniform_int_distribution<std::size_t> uniform_dist(min, max);

    return uniform_dist(engine);
}

ConnectionPoolSPtr& ShardsPool::_get_pool(Slot slot) {
    auto shards_iter = _shards.lower_bound(SlotRange{slot, slot});
    if (shards_iter == _shards.end() || slot < shards_iter->first.min) {
        throw Error("Slot is out of range: " + std::to_string(slot));
    }

    const auto &node = shards_iter->second;

    auto node_iter = _pools.find(node);
    if (node_iter == _pools.end()) {
        throw Error("Slot is NOT covered: " + std::to_string(slot));
    }

    return node_iter->second;
}

ConnectionPoolSPtr ShardsPool::_fetch(Slot slot) {
    std::lock_guard<std::mutex> lock(_mutex);

    return _get_pool(slot);
}

ConnectionOptions ShardsPool::_connection_options(Slot slot) {
    std::lock_guard<std::mutex> lock(_mutex);

    auto &pool = _get_pool(slot);

    assert(pool);

    return pool->connection_options();
}

auto ShardsPool::_add_node(const Node &node) -> NodeMap::iterator {
    auto opts = _connection_opts;
    opts.host = node.host;
    opts.port = node.port;

    // TODO: Better set readonly an attribute of `Node`.
    if (_role == Role::SLAVE) {
        opts.readonly = true;
    }

    return _pools.emplace(node, std::make_shared<ConnectionPool>(_pool_opts, opts)).first;
}

}

}
