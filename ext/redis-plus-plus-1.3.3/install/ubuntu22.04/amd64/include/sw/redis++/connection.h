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

#ifndef SEWENEW_REDISPLUSPLUS_CONNECTION_H
#define SEWENEW_REDISPLUSPLUS_CONNECTION_H

#include <cerrno>
#include <cstring>
#include <memory>
#include <vector>
#include <string>
#include <sstream>
#include <chrono>
#include <hiredis/hiredis.h>
#include "errors.h"
#include "reply.h"
#include "utils.h"
#include "tls.h"

namespace sw {

namespace redis {

enum class ConnectionType {
    TCP = 0,
    UNIX
};

struct ConnectionOptions {
public:
    ConnectionOptions() = default;

    explicit ConnectionOptions(const std::string &uri);

    ConnectionOptions(const ConnectionOptions &) = default;
    ConnectionOptions& operator=(const ConnectionOptions &) = default;

    ConnectionOptions(ConnectionOptions &&) = default;
    ConnectionOptions& operator=(ConnectionOptions &&) = default;

    ~ConnectionOptions() = default;

    ConnectionType type = ConnectionType::TCP;

    std::string host;

    int port = 6379;

    std::string path;

    std::string user = "default";

    std::string password;

    int db = 0;

    bool keep_alive = false;

    std::chrono::milliseconds connect_timeout{0};

    std::chrono::milliseconds socket_timeout{0};

    tls::TlsOptions tls;

    // `readonly` is only used for reading from a slave node in Redis Cluster mode.
    // Client code should never manually set/get it. This member might be removed in the future.
    bool readonly = false;

private:
    ConnectionOptions _parse_uri(const std::string &uri) const;

    auto _split_uri(const std::string &uri) const
        -> std::tuple<std::string, std::string, std::string>;

    auto _split_path(const std::string &path) const
        -> std::tuple<std::string, int, std::string>;

    void _parse_parameters(const std::string &parameter_string,
                            ConnectionOptions &opts) const;

    void _set_option(const std::string &key, const std::string &val, ConnectionOptions &opts) const;

    bool _parse_bool_option(const std::string &str) const;

    std::chrono::milliseconds _parse_timeout_option(const std::string &str) const;

    std::vector<std::string> _split(const std::string &str, const std::string &delimiter) const;

    void _set_auth_opts(const std::string &auth, ConnectionOptions &opts) const;

    void _set_tcp_opts(const std::string &path, ConnectionOptions &opts) const;

    void _set_unix_opts(const std::string &path, ConnectionOptions &opts) const;
};

class CmdArgs;

class Connection {
public:
    explicit Connection(const ConnectionOptions &opts);

    Connection(const Connection &) = delete;
    Connection& operator=(const Connection &) = delete;

    Connection(Connection &&) = default;
    Connection& operator=(Connection &&) = default;

    ~Connection() = default;

    // Check if the connection is broken. Client needs to do this check
    // before sending some command to the connection. If it's broken,
    // client needs to reconnect it.
    bool broken() const noexcept {
        return _ctx->err != REDIS_OK;
    }

    void reset() noexcept {
        _ctx->err = 0;
    }

    void invalidate() noexcept {
        _ctx->err = REDIS_ERR;
    }

    void reconnect();

    auto create_time() const
        -> std::chrono::time_point<std::chrono::steady_clock> {
        return _create_time;
    }

    auto last_active() const
        -> std::chrono::time_point<std::chrono::steady_clock> {
        return _last_active;
    }

    template <typename ...Args>
    void send(const char *format, Args &&...args);

    void send(int argc, const char **argv, const std::size_t *argv_len);

    void send(CmdArgs &args);

    ReplyUPtr recv(bool handle_error_reply = true);

    const ConnectionOptions& options() const {
        return _opts;
    }

    friend void swap(Connection &lhs, Connection &rhs) noexcept;

private:
    class Connector;

    struct ContextDeleter {
        void operator()(redisContext *context) const {
            if (context != nullptr) {
                redisFree(context);
            }
        };
    };

    using ContextUPtr = std::unique_ptr<redisContext, ContextDeleter>;

    void _set_options();

    void _auth();

    void _select_db();

    void _enable_readonly();

    redisContext* _context();

    ContextUPtr _ctx;

    // The time that the connection is created.
    std::chrono::time_point<std::chrono::steady_clock> _create_time{};

    // The time that the connection is created or the time that
    // the connection is recently used, i.e. `_context()` is called.
    std::chrono::time_point<std::chrono::steady_clock> _last_active{};

    ConnectionOptions _opts;

    // TODO: define _tls_ctx before _ctx
    tls::TlsContextUPtr _tls_ctx;
};

using ConnectionSPtr = std::shared_ptr<Connection>;

enum class Role {
    MASTER,
    SLAVE
};

// Inline implementaions.

template <typename ...Args>
inline void Connection::send(const char *format, Args &&...args) {
    auto ctx = _context();

    assert(ctx != nullptr);

    if (redisAppendCommand(ctx,
                format,
                std::forward<Args>(args)...) != REDIS_OK) {
        throw_error(*ctx, "Failed to send command");
    }

    assert(!broken());
}

inline redisContext* Connection::_context() {
    _last_active = std::chrono::steady_clock::now();

    return _ctx.get();
}

}

}

#endif // end SEWENEW_REDISPLUSPLUS_CONNECTION_H
