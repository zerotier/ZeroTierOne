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

#ifndef SEWENEW_REDISPLUSPLUS_ASYNC_CONNECTION_H
#define SEWENEW_REDISPLUSPLUS_ASYNC_CONNECTION_H

#include <cassert>
#include <memory>
#include <mutex>
#include <atomic>
#include <exception>
#include <vector>
#include <hiredis/async.h>
#include "connection.h"
#include "command_args.h"
#include "event_loop.h"
#include "async_utils.h"
#include "tls.h"
#include "shards.h"
#include "cmd_formatter.h"

namespace sw {

namespace redis {

template <typename Result>
struct DefaultResultParser {
    Result operator()(redisReply &reply) const {
        return reply::parse<Result>(reply);
    }
};

class AsyncConnection;
class AsyncConnectionPool;
class AsyncShardsPool;

class AsyncEvent {
public:
    virtual ~AsyncEvent() = default;

    virtual void handle(redisAsyncContext &ctx) = 0;

    virtual void set_exception(std::exception_ptr err) = 0;
};

using AsyncEventUPtr = std::unique_ptr<AsyncEvent>;

enum class AsyncConnectionMode {
    SINGLE = 0,
    SENTINEL,
    CLUSTER
};

class AsyncConnection : public std::enable_shared_from_this<AsyncConnection> {
public:
    AsyncConnection(const ConnectionOptions &opts,
            EventLoop *loop,
            AsyncConnectionMode = AsyncConnectionMode::SINGLE);

    AsyncConnection(const AsyncConnection &) = delete;
    AsyncConnection& operator=(const AsyncConnection &) = delete;

    AsyncConnection(AsyncConnection &&) = delete;
    AsyncConnection& operator=(AsyncConnection &&) = delete;

    ~AsyncConnection();

    bool broken() const noexcept {
        return _state == State::BROKEN;
    }

    auto create_time() const
        -> std::chrono::time_point<std::chrono::steady_clock> {
        return _create_time;
    }

    auto last_active() const
        -> std::chrono::steady_clock::duration {
        return _last_active;
    }

    void disconnect(std::exception_ptr err);

    template <typename Result, typename ResultParser>
    Future<Result> send(FormattedCommand cmd);

    template <typename Result, typename ResultParser>
    Future<Result> send(const std::shared_ptr<AsyncShardsPool> &pool,
            const StringView &key,
            FormattedCommand cmd);

    void send(AsyncEventUPtr event);

    void event_callback();

    void connect_callback(std::exception_ptr err = nullptr);

    void disconnect_callback(std::exception_ptr err);

    ConnectionOptions options();

    void update_node_info(const std::string &host, int port);

private:
    enum class State {
        BROKEN = 0,
        NOT_CONNECTED,
        CONNECTING,
        AUTHING,
        SELECTING_DB,
        READY,
        WAIT_SENTINEL,
        ENABLE_READONLY
    };

    redisAsyncContext& _context() {
        assert(_ctx != nullptr);

        _last_active = std::chrono::steady_clock::now().time_since_epoch();

        return *_ctx;
    }

    void _connecting_callback();

    void _authing_callback();

    void _select_db_callback();

    bool _need_auth() const;

    void _auth();

    bool _need_select_db() const;

    void _select_db();

    bool _need_enable_readonly() const;

    void _enable_readonly();

    void _set_ready();

    void _connect_with_sentinel();

    void _connect();

    void _disable_disconnect_callback();

    void _send();

    std::vector<std::unique_ptr<AsyncEvent>> _get_events();

    void _clean_up();

    void _fail_events(std::exception_ptr err);

    static void _clean_async_context(void *data);

    struct AsyncContextDeleter {
        void operator()(redisAsyncContext *ctx) const {
            if (ctx != nullptr) {
                redisAsyncFree(ctx);
            }
        }
    };
    using AsyncContextUPtr = std::unique_ptr<redisAsyncContext, AsyncContextDeleter>;

    AsyncContextUPtr _connect(const ConnectionOptions &opts);

    ConnectionOptions _opts;

    EventLoop *_loop = nullptr;

    tls::TlsContextUPtr _tls_ctx;

    // _ctx will be release by EventLoop after attached.
    redisAsyncContext *_ctx = nullptr;

    // The time that the connection is created.
    std::chrono::time_point<std::chrono::steady_clock> _create_time{};

    // The time that the connection is created or the time that
    // the connection is recently used, i.e. `_context()` is called.
    // NOTE: `_last_active` is `std::atomic`, and we cannot make it of type time_point,
    // since time_point's constructor is non-trival.
    std::atomic<std::chrono::steady_clock::duration> _last_active{};

    std::vector<std::unique_ptr<AsyncEvent>> _events;

    std::atomic<State> _state{State::NOT_CONNECTED};

    std::exception_ptr _err;

    std::mutex _mtx;
};

using AsyncConnectionSPtr = std::shared_ptr<AsyncConnection>;

struct AsyncContext {
    AsyncContext(AsyncConnectionSPtr conn) : connection(std::move(conn)) {}

    AsyncConnectionSPtr connection;

    bool run_disconnect_callback = true;
};

template <typename Result, typename ResultParser>
class CommandEvent : public AsyncEvent {
public:
    explicit CommandEvent(FormattedCommand cmd) : _cmd(std::move(cmd)) {}

    Future<Result> get_future() {
        return _pro.get_future();
    }

    virtual void handle(redisAsyncContext &ctx) override {
        _handle(ctx, _reply_callback);
    }

    virtual void set_exception(std::exception_ptr err) override {
        _pro.set_exception(err);
    }

    template <typename T>
    struct ResultType {};

    void set_value(redisReply &reply) {
        _set_value(reply, ResultType<Result>{});
    }

protected:
    using Callback = void (*)(redisAsyncContext *, void *, void *);

    void _handle(redisAsyncContext &ctx, Callback callback) {
        if (redisAsyncFormattedCommand(&ctx,
                    callback, this, _cmd.data(), _cmd.size()) != REDIS_OK) {
            throw_error(ctx.c, "failed to send command");
        }
    }

private:
    static void _reply_callback(redisAsyncContext * /*ctx*/, void *r, void *privdata) {
        auto event = static_cast<CommandEvent<Result, ResultParser> *>(privdata);

        assert(event != nullptr);

        try {
            redisReply *reply = static_cast<redisReply *>(r);
            if (reply == nullptr) {
                event->set_exception(std::make_exception_ptr(Error("connection has been closed")));
            } else if (reply::is_error(*reply)) {
                try {
                    throw_error(*reply);
                } catch (const Error &e) {
                    event->set_exception(std::current_exception());
                }
            } else {
                event->set_value(*reply);
            }
        } catch (...) {
            event->set_exception(std::current_exception());
        }

        delete event;
    }

    template <typename T>
    void _set_value(redisReply &reply, ResultType<T>) {
        ResultParser parser;
        _pro.set_value(parser(reply));
    }

    void _set_value(redisReply &reply, ResultType<void>) {
        ResultParser parser;
        parser(reply);

        _pro.set_value();
    }

    FormattedCommand _cmd;

    Promise<Result> _pro;
};

template <typename Result, typename ResultParser>
using CommandEventUPtr = std::unique_ptr<CommandEvent<Result, ResultParser>>;

class AskingEvent : public AsyncEvent {
public:
    explicit AskingEvent(AsyncEvent *event) : _event(event) {}

    ~AskingEvent() {
        if (_event != nullptr) {
            delete _event;
        }
    }

    virtual void handle(redisAsyncContext &ctx) override {
        if (redisAsyncCommand(&ctx, _asking_callback, this, "ASKING") != REDIS_OK) {
            throw_error(ctx.c, "failed to send ASKING command");
        }

        assert(_event != nullptr);

        _event->handle(ctx);

        _event = nullptr;
    }

    virtual void set_exception(std::exception_ptr err) override {
        if (_event != nullptr) {
            _event->set_exception(err);
        }
    }

private:
    static void _asking_callback(redisAsyncContext * /*ctx*/, void *r, void *privdata) {
        auto event = static_cast<AskingEvent *>(privdata);

        assert(event != nullptr);

        // TODO: No need to check the reply. It seems that we can simply ignore the reply,
        // and delete the event.
        try {
            redisReply *reply = static_cast<redisReply *>(r);
            if (reply == nullptr) {
                event->set_exception(std::make_exception_ptr(Error("connection has been closed")));
            } else if (reply::is_error(*reply)) {
                try {
                    throw_error(*reply);
                } catch (const Error &e) {
                    event->set_exception(std::current_exception());
                }
            } else {
                reply::parse<void>(*reply);
            }
        } catch (...) {
            event->set_exception(std::current_exception());
        }

        delete event;
    }

    AsyncEvent *_event = nullptr;
};

// NOTE: This class is similar to `SafeAsyncConnection`.
// The difference is that `SafeAsyncConnection` tries to avoid copying a std::shared_ptr.
class GuardedAsyncConnection {
public:
    explicit GuardedAsyncConnection(const std::shared_ptr<AsyncConnectionPool> &pool);

    GuardedAsyncConnection(const GuardedAsyncConnection &) = delete;
    GuardedAsyncConnection& operator=(const GuardedAsyncConnection &) = delete;

    GuardedAsyncConnection(GuardedAsyncConnection &&) = default;
    GuardedAsyncConnection& operator=(GuardedAsyncConnection &&) = default;

    ~GuardedAsyncConnection();

    AsyncConnection& connection();

private:
    std::shared_ptr<AsyncConnectionPool> _pool;
    std::shared_ptr<AsyncConnection> _connection;
};

template <typename Result, typename ResultParser>
class ClusterEvent : public CommandEvent<Result, ResultParser> {
public:
    explicit ClusterEvent(const std::shared_ptr<AsyncShardsPool> &pool,
            const StringView &key,
            FormattedCommand cmd) :
        CommandEvent<Result, ResultParser>(std::move(cmd)),
        _pool(pool),
        _key(key.data(), key.size()) {}

    virtual void handle(redisAsyncContext &ctx) override {
        CommandEvent<Result, ResultParser>::_handle(ctx, _cluster_reply_callback);
    }

private:
    enum class State {
        NORMAL = 0,
        MOVED,
        ASKING
    };

    static void _cluster_reply_callback(redisAsyncContext * /*ctx*/, void *r, void *privdata) {
        auto event = static_cast<ClusterEvent<Result, ResultParser> *>(privdata);

        assert(event != nullptr);

        try {
            redisReply *reply = static_cast<redisReply *>(r);
            if (reply == nullptr) {
                event->set_exception(std::make_exception_ptr(Error("connection has been closed")));
            } else if (reply::is_error(*reply)) {
                try {
                    throw_error(*reply);
                } catch (const IoError &err) {
                    event->_pool->update(event->_key, AsyncEventUPtr(event));
                    return;
                } catch (const ClosedError &err) {
                    event->_pool->update(event->_key, AsyncEventUPtr(event));
                    return;
                } catch (const MovedError &err) {
                    switch (event->_state) {
                    case State::MOVED:
                        throw Error("too many moved error");
                        break;

                    case State::ASKING:
                        throw Error("Slot migrating...");
                        break;

                    default:
                        break;
                    }

                    event->_state = State::MOVED;
                    event->_pool->update(event->_key, AsyncEventUPtr(event));
                    return;
                } catch (const AskError &err) {
                    event->_state = State::ASKING;
                    auto pool = event->_pool->fetch(err.node());
                    assert(pool);
                    GuardedAsyncConnection connection(pool);
                    connection.connection().send(AsyncEventUPtr(new AskingEvent(event)));
                    return;
                } catch (const Error &e) {
                    event->set_exception(std::current_exception());
                }
            } else {
                event->set_value(*reply);
            }
        } catch (...) {
            event->set_exception(std::current_exception());
        }

        delete event;
    }

    std::shared_ptr<AsyncShardsPool> _pool;

    std::string _key;

    State _state = State::NORMAL;
};

template <typename Result, typename ResultParser>
using ClusterEventUPtr = std::unique_ptr<ClusterEvent<Result, ResultParser>>;

template <typename Result, typename ResultParser>
Future<Result> AsyncConnection::send(FormattedCommand cmd) {
    auto event = CommandEventUPtr<Result, ResultParser>(
            new CommandEvent<Result, ResultParser>(std::move(cmd)));

    auto fut = event->get_future();

    send(std::move(event));

    return fut;
}

template <typename Result, typename ResultParser>
Future<Result> AsyncConnection::send(const std::shared_ptr<AsyncShardsPool> &pool,
        const StringView &key,
        FormattedCommand cmd) {
    auto event = ClusterEventUPtr<Result, ResultParser>(
            new ClusterEvent<Result, ResultParser>(pool, key, std::move(cmd)));

    auto fut = event->get_future();

    send(std::move(event));

    return fut;
}

}

}

#endif // end SEWENEW_REDISPLUSPLUS_ASYNC_CONNECTION_H
