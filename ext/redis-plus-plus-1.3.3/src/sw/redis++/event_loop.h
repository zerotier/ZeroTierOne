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

#ifndef SEWENEW_REDISPLUSPLUS_EVENT_LOOP_H
#define SEWENEW_REDISPLUSPLUS_EVENT_LOOP_H

#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <exception>
#include <mutex>
#include <thread>
#include <uv.h>
#include "connection.h"

namespace sw {

namespace redis {

class AsyncConnection;
class AsyncEvent;

class EventLoop {
public:
    EventLoop();

    EventLoop(const EventLoop &) = delete;
    EventLoop& operator=(const EventLoop &) = delete;

    EventLoop(EventLoop &&that);

    EventLoop& operator=(EventLoop &&that);

    ~EventLoop();

    void unwatch(std::shared_ptr<AsyncConnection> connection, std::exception_ptr err = nullptr);

    void add(std::shared_ptr<AsyncConnection> event);

    // Not thread safe. Only call it in callback functions.
    void watch(redisAsyncContext &ctx);

private:
    static void _connect_callback(const redisAsyncContext *ctx, int status);

    static void _disconnect_callback(const redisAsyncContext *ctx, int status);

    static void _event_callback(uv_async_t *handle);

    static void _stop_callback(uv_async_t *handle);

    struct LoopDeleter {
        void operator()(uv_loop_t *loop) const;
    };

    using LoopUPtr = std::unique_ptr<uv_loop_t, LoopDeleter>;

    std::string _err_msg(int err) const {
        return uv_strerror(err);
    }

    LoopUPtr _create_event_loop() const;

    using UvAsyncUPtr = std::unique_ptr<uv_async_t>;

    using AsyncCallback = void (*)(uv_async_t*);

    UvAsyncUPtr _create_uv_async(AsyncCallback callback);

    void _stop();

    void _notify();

    void _clean_up(std::unordered_set<std::shared_ptr<AsyncConnection>> &command_events,
            std::unordered_map<std::shared_ptr<AsyncConnection>, std::exception_ptr> &disconnect_events);

    auto _get_events()
        -> std::pair<std::unordered_set<std::shared_ptr<AsyncConnection>>,
            std::unordered_map<std::shared_ptr<AsyncConnection>, std::exception_ptr>>;

    // We must define _event_async and _stop_async before _loop,
    // because these memory can only be release after _loop's deleter
    // has been called, i.e. the deleter will close these handles.
    UvAsyncUPtr _event_async;

    UvAsyncUPtr _stop_async;

    std::thread _loop_thread;

    std::mutex _mtx;

    std::unordered_map<std::shared_ptr<AsyncConnection>, std::exception_ptr> _disconnect_events;

    std::unordered_set<std::shared_ptr<AsyncConnection>> _command_events;

    // _loop must be defined at last, since its destructor needs other data members.
    LoopUPtr _loop;
};

using EventLoopSPtr = std::shared_ptr<EventLoop>;

}

}

#endif // end SEWENEW_REDISPLUSPLUS_EVENT_LOOP_H
