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

#ifndef SEWENEW_REDISPLUSPLUS_ASYNC_SENTINEL_H
#define SEWENEW_REDISPLUSPLUS_ASYNC_SENTINEL_H

#include <mutex>
#include <memory>
#include <queue>
#include <condition_variable>
#include "sentinel.h"
#include "async_connection.h"

namespace sw {

namespace redis {

class AsyncConnectionPool;

class AsyncSentinel {
public:
    explicit AsyncSentinel(const SentinelOptions &sentinel_opts);

    AsyncSentinel(const AsyncSentinel &) = delete;
    AsyncSentinel& operator=(const AsyncSentinel &) = delete;

    AsyncSentinel(AsyncSentinel &&) = delete;
    AsyncSentinel& operator=(AsyncSentinel &&) = delete;

    ~AsyncSentinel();

private:
    friend class SimpleAsyncSentinel;

    struct AsyncSentinelTask {
        operator bool() const noexcept {
            return bool(connection);
        }

        std::weak_ptr<AsyncConnectionPool> pool;

        AsyncConnectionSPtr connection;

        std::string master_name;

        Role role = Role::MASTER;
    };

    void add(AsyncSentinelTask task);

    void _stop_worker();

    void _run_task(AsyncSentinelTask &task);

    std::queue<AsyncSentinelTask> _fetch_tasks();

    void _run();

    std::queue<AsyncSentinelTask> _tasks;

    std::shared_ptr<Sentinel> _sentinel;

    std::thread _worker;

    std::mutex _mutex;

    std::condition_variable _cv;
};

using AsyncSentinelSPtr = std::shared_ptr<AsyncSentinel>;

}

}

#endif // end SEWENEW_REDISPLUSPLUS_ASYNC_SENTINEL_H
