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

#include "async_sentinel.h"
#include <cassert>
#include "errors.h"
#include "async_connection_pool.h"

namespace sw {

namespace redis {

AsyncSentinel::AsyncSentinel(const SentinelOptions &sentinel_opts) :
    _sentinel(std::make_shared<Sentinel>(sentinel_opts)),
    _worker([this]() { this->_run(); }) {}

AsyncSentinel::~AsyncSentinel() {
    _stop_worker();

    if (_worker.joinable()) {
        _worker.join();
    }
}

void AsyncSentinel::add(AsyncSentinelTask task) {
    {
        std::lock_guard<std::mutex> lock(_mutex);

        _tasks.push(std::move(task));
    }

    _cv.notify_one();
}

void AsyncSentinel::_stop_worker() {
    // Add an empty task to stop the worker thread.
    add(AsyncSentinelTask{});
}

void AsyncSentinel::_run() {
    while (true) {
        auto tasks = _fetch_tasks();

        assert(!tasks.empty());

        while (!tasks.empty()) {
            auto &task = tasks.front();
            if (!task) {
                // Got a stopping task.
                return;
            }

            _run_task(task);

            tasks.pop();
        }
    }
}

void AsyncSentinel::_run_task(AsyncSentinelTask &task) {
    auto pool = task.pool.lock();
    if (!pool) {
        // AsyncConnectionPool has been destroyed, give up the task.
        return;
    }

    auto &connection = task.connection;
    try {
        SimpleSentinel sentinel(_sentinel, task.master_name, task.role);
        auto sync_connection = sentinel.create(connection->options());

        const auto &opts = sync_connection.options();
        pool->update_node_info(opts.host, opts.port, connection);
    } catch (const StopIterError &e) {
        pool->update_node_info(connection,
                std::make_exception_ptr(Error("Failed to create connection with sentinel")));
    } catch (const Error &e) {
        pool->update_node_info(connection, std::current_exception());
    }
}

auto AsyncSentinel::_fetch_tasks() -> std::queue<AsyncSentinelTask> {
    std::queue<AsyncSentinelTask> tasks;

    std::unique_lock<std::mutex> lock(_mutex);
    if (_tasks.empty()) {
        _cv.wait(lock, [this]() { return !(this->_tasks).empty(); } );
    }

    tasks.swap(_tasks);

    return tasks;
}

}

}
