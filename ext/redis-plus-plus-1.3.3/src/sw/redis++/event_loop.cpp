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

#include "event_loop.h"
#include <cassert>
#include <hiredis/adapters/libuv.h>
#include "async_connection.h"

namespace sw {

namespace redis {

EventLoop::EventLoop() {
    _loop = _create_event_loop();

    _event_async = _create_uv_async(_event_callback);
    _stop_async = _create_uv_async(_stop_callback);

    _loop_thread = std::thread([this]() { uv_run(this->_loop.get(), UV_RUN_DEFAULT); });
}

EventLoop::~EventLoop() {
    _stop();

    if (_loop_thread.joinable()) {
        _loop_thread.join();
    }
}

void EventLoop::unwatch(AsyncConnectionSPtr connection, std::exception_ptr err) {
    assert(connection);

    {
        std::lock_guard<std::mutex> lock(_mtx);

        _disconnect_events.emplace(std::move(connection), err);
    }

    _notify();
}

void EventLoop::add(AsyncConnectionSPtr event) {
    assert(event);

    {
        std::lock_guard<std::mutex> lock(_mtx);

        _command_events.insert(std::move(event));
    }

    _notify();
}

void EventLoop::watch(redisAsyncContext &ctx) {
    if (redisLibuvAttach(&ctx, _loop.get()) != REDIS_OK) {
        throw Error("failed to attach to event loop");
    }

    redisAsyncSetConnectCallback(&ctx, EventLoop::_connect_callback);
    redisAsyncSetDisconnectCallback(&ctx, EventLoop::_disconnect_callback);
}

void EventLoop::_connect_callback(const redisAsyncContext *ctx, int status) {
    assert(ctx != nullptr);

    auto *context = static_cast<AsyncContext *>(ctx->data);
    assert(context != nullptr);

    auto &connection = context->connection;
    std::exception_ptr err;
    if (status != REDIS_OK) {
        try {
            throw_error(ctx->c, "failed to connect to server");
        } catch (const Error &e) {
            err = std::current_exception();
        }
    }

    connection->connect_callback(err);
}

void EventLoop::_disconnect_callback(const redisAsyncContext *ctx, int status) {
    assert(ctx != nullptr);

    auto *context = static_cast<AsyncContext *>(ctx->data);
    assert(context != nullptr);

    if (!context->run_disconnect_callback) {
        return;
    }

    auto &connection = context->connection;
    std::exception_ptr err;
    if (status != REDIS_OK) {
        try {
            throw_error(ctx->c, "failed to disconnect from server");
        } catch (const Error &e) {
            err = std::current_exception();
        }
    }

    // TODO: if status == REDIS_OK, should we call the callback?
    connection->disconnect_callback(err);
}

void EventLoop::_event_callback(uv_async_t *handle) {
    assert(handle != nullptr);

    auto *event_loop = static_cast<EventLoop*>(handle->data);
    assert(event_loop != nullptr);

    std::unordered_set<AsyncConnectionSPtr> command_events;
    std::unordered_map<AsyncConnectionSPtr, std::exception_ptr> disconnect_events;
    std::tie(command_events, disconnect_events) = event_loop->_get_events();

    for (auto &connection : command_events) {
        assert(connection);

        connection->event_callback();
    }

    for (auto &ele : disconnect_events) {
        auto &connection = ele.first;
        auto &err = ele.second;

        assert(connection);

        if (!err) {
            // Ensure all pending events have been sent before disconnecting.
            connection->event_callback();
        }

        // If `event_callback` fails, connection will be release by event loop,
        // and this `disconnect` call will do nothing.
        connection->disconnect(err);
    }
}

void EventLoop::_stop_callback(uv_async_t *handle) {
    assert(handle != nullptr);

    auto *event_loop = static_cast<EventLoop*>(handle->data);
    assert(event_loop != nullptr);

    std::unordered_set<AsyncConnectionSPtr> command_events;
    std::unordered_map<AsyncConnectionSPtr, std::exception_ptr> disconnect_events;
    std::tie(command_events, disconnect_events) = event_loop->_get_events();

    event_loop->_clean_up(command_events, disconnect_events);

    uv_stop(event_loop->_loop.get());
}

void EventLoop::_clean_up(std::unordered_set<AsyncConnectionSPtr> &command_events,
        std::unordered_map<AsyncConnectionSPtr, std::exception_ptr> &disconnect_events) {
    auto err = std::make_exception_ptr(Error("event loop is closing"));
    for (auto &connection : command_events) {
        assert(connection);

        connection->disconnect(err);
    }

    for (auto &ele : disconnect_events) {
        auto &connection = ele.first;
        auto e = ele.second;
        if (!e) {
            e = err;
        }

        assert(connection);

        connection->disconnect(e);
    }
}

void EventLoop::LoopDeleter::operator()(uv_loop_t *loop) const {
    if (loop == nullptr) {
        return;
    }

    // How to correctly close an event loop:
    // https://stackoverflow.com/questions/25615340/closing-libuv-handles-correctly
    // TODO: do we need to call this? Since we always has 2 async_t handles.
    if (uv_loop_close(loop) == 0) {
        delete loop;

        return;
    }

    uv_walk(loop,
            [](uv_handle_t *handle, void *) {
                if (handle != nullptr) {
                    // We don't need to release handle's memory in close callback,
                    // since we'll release the memory in EventLoop's destructor.
                    uv_close(handle, nullptr);
                }
            },
            nullptr);

    // Ensure uv_walk's callback to be called.
    uv_run(loop, UV_RUN_DEFAULT);

    uv_loop_close(loop);

    delete loop;
}

void EventLoop::_notify() {
    assert(_event_async);

    uv_async_send(_event_async.get());
}

void EventLoop::_stop() {
    assert(_stop_async);

    uv_async_send(_stop_async.get());
}

auto EventLoop::_get_events()
    -> std::pair<std::unordered_set<AsyncConnectionSPtr>,
                    std::unordered_map<AsyncConnectionSPtr, std::exception_ptr>> {
    std::unordered_set<AsyncConnectionSPtr> command_events;
    std::unordered_map<AsyncConnectionSPtr, std::exception_ptr> disconnect_events;
    {
        std::lock_guard<std::mutex> lock(_mtx);

        command_events.swap(_command_events);
        disconnect_events.swap(_disconnect_events);
    }

    return std::make_pair(std::move(command_events), std::move(disconnect_events));
}

EventLoop::UvAsyncUPtr EventLoop::_create_uv_async(AsyncCallback callback) {
    auto uv_async = std::unique_ptr<uv_async_t>(new uv_async_t);
    auto err = uv_async_init(_loop.get(), uv_async.get(), callback);
    if (err != 0) {
        throw Error("failed to initialize async: " + _err_msg(err));
    }

    uv_async->data = this;

    return uv_async;
}

EventLoop::LoopUPtr EventLoop::_create_event_loop() const {
    auto *loop = new uv_loop_t;
    auto err = uv_loop_init(loop);
    if (err != 0) {
        delete loop;
        throw Error("failed to initialize event loop: " + _err_msg(err));
    }

    return LoopUPtr(loop);
}

}

}
