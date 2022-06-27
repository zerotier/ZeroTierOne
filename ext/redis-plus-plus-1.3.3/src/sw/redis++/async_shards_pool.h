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

#ifndef SEWENEW_REDISPLUSPLUS_ASYNC_SHARDS_POOL_H
#define SEWENEW_REDISPLUSPLUS_ASYNC_SHARDS_POOL_H

#include <condition_variable>
#include <mutex>
#include <exception>
#include <thread>
#include <queue>
#include <memory>
#include "shards_pool.h"
#include "async_connection_pool.h"

namespace sw {

namespace redis {

class AsyncShardsPool {
public:
    AsyncShardsPool(const AsyncShardsPool &) = delete;
    AsyncShardsPool& operator=(const AsyncShardsPool &) = delete;

    AsyncShardsPool(AsyncShardsPool &&that);
    AsyncShardsPool& operator=(AsyncShardsPool &&that);

    ~AsyncShardsPool();

    AsyncShardsPool(const EventLoopSPtr &loop,
                const ConnectionPoolOptions &pool_opts,
                const ConnectionOptions &connection_opts,
                Role role);

    AsyncConnectionPoolSPtr fetch(const StringView &key);

    AsyncConnectionPoolSPtr fetch();

    AsyncConnectionPoolSPtr fetch(const Node &node);

    void update(const std::string &key, AsyncEventUPtr event);

    ConnectionOptions connection_options(const StringView &key);

    ConnectionOptions connection_options();

private:
    struct RedeliverEvent {
        std::string key;
        AsyncEventUPtr event;
    };

    void _run();

    void _move(AsyncShardsPool &&that);

    Slot _slot(const StringView &key) const;

    AsyncConnectionPoolSPtr _fetch(Slot slot);

    Slot _slot() const;

    std::queue<RedeliverEvent> _fetch_events();

    void _update_shards();

    bool _redeliver_events(std::queue<RedeliverEvent> &events);

    bool _fail_events(std::queue<RedeliverEvent> &events, std::exception_ptr err);

    using NodeMap = std::unordered_map<Node, AsyncConnectionPoolSPtr, NodeHash>;

    NodeMap::iterator _add_node(const Node &node);

    std::size_t _random(std::size_t min, std::size_t max) const;

    const Node& _get_node(Slot slot) const;

    AsyncConnectionPoolSPtr& _get_pool(Slot slot);

    ConnectionOptions _connection_options(Slot slot);

    Shards _get_shards(const std::string &host, int port);

    ConnectionPoolOptions _pool_opts;

    ConnectionOptions _connection_opts;

    Role _role = Role::MASTER;

    Shards _shards;

    NodeMap _pools;

    EventLoopSPtr _loop;

    std::thread _worker;

    std::condition_variable _cv;

    std::mutex _mutex;

    std::queue<RedeliverEvent> _events;

    static const std::size_t SHARDS = 16383;
};

using AsyncShardsPoolSPtr = std::shared_ptr<AsyncShardsPool>;

}

}

#endif // end SEWENEW_REDISPLUSPLUS_ASYNC_SHARDS_POOL_H
