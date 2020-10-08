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

#include "connection.h"
#include <cassert>
#include "reply.h"
#include "command.h"
#include "command_args.h"

namespace sw {

namespace redis {

ConnectionOptions::ConnectionOptions(const std::string &uri) :
                                        ConnectionOptions(_parse_options(uri)) {}

ConnectionOptions ConnectionOptions::_parse_options(const std::string &uri) const {
    std::string type;
    std::string path;
    std::tie(type, path) = _split_string(uri, "://");

    if (path.empty()) {
        throw Error("Invalid URI: no path");
    }

    if (type == "tcp") {
        return _parse_tcp_options(path);
    } else if (type == "unix") {
        return _parse_unix_options(path);
    } else {
        throw Error("Invalid URI: invalid type");
    }
}

ConnectionOptions ConnectionOptions::_parse_tcp_options(const std::string &path) const {
    ConnectionOptions options;

    options.type = ConnectionType::TCP;

    std::string host;
    std::string port;
    std::tie(host, port) = _split_string(path, ":");

    options.host = host;
    try {
        if (!port.empty()) {
            options.port = std::stoi(port);
        } // else use default port, i.e. 6379.
    } catch (const std::exception &) {
        throw Error("Invalid URL: invalid port");
    }

    return options;
}

ConnectionOptions ConnectionOptions::_parse_unix_options(const std::string &path) const {
    ConnectionOptions options;

    options.type = ConnectionType::UNIX;
    options.path = path;

    return options;
}

auto ConnectionOptions::_split_string(const std::string &str, const std::string &delimiter) const ->
        std::pair<std::string, std::string> {
    auto pos = str.rfind(delimiter);
    if (pos == std::string::npos) {
        return {str, ""};
    }

    return {str.substr(0, pos), str.substr(pos + delimiter.size())};
}

class Connection::Connector {
public:
    explicit Connector(const ConnectionOptions &opts);

    ContextUPtr connect() const;

private:
    ContextUPtr _connect() const;

    redisContext* _connect_tcp() const;

    redisContext* _connect_unix() const;

    void _set_socket_timeout(redisContext &ctx) const;

    void _enable_keep_alive(redisContext &ctx) const;

    timeval _to_timeval(const std::chrono::milliseconds &dur) const;

    const ConnectionOptions &_opts;
};

Connection::Connector::Connector(const ConnectionOptions &opts) : _opts(opts) {}

Connection::ContextUPtr Connection::Connector::connect() const {
    auto ctx = _connect();

    assert(ctx);

    if (ctx->err != REDIS_OK) {
        throw_error(*ctx, "Failed to connect to Redis");
    }

    _set_socket_timeout(*ctx);

    _enable_keep_alive(*ctx);

    return ctx;
}

Connection::ContextUPtr Connection::Connector::_connect() const {
    redisContext *context = nullptr;
    switch (_opts.type) {
    case ConnectionType::TCP:
        context = _connect_tcp();
        break;

    case ConnectionType::UNIX:
        context = _connect_unix();
        break;

    default:
        // Never goes here.
        throw Error("Unkonw connection type");
    }

    if (context == nullptr) {
        throw Error("Failed to allocate memory for connection.");
    }

    return ContextUPtr(context);
}

redisContext* Connection::Connector::_connect_tcp() const {
    if (_opts.connect_timeout > std::chrono::milliseconds(0)) {
        return redisConnectWithTimeout(_opts.host.c_str(),
                    _opts.port,
                    _to_timeval(_opts.connect_timeout));
    } else {
        return redisConnect(_opts.host.c_str(), _opts.port);
    }
}

redisContext* Connection::Connector::_connect_unix() const {
    if (_opts.connect_timeout > std::chrono::milliseconds(0)) {
        return redisConnectUnixWithTimeout(
                    _opts.path.c_str(),
                    _to_timeval(_opts.connect_timeout));
    } else {
        return redisConnectUnix(_opts.path.c_str());
    }
}

void Connection::Connector::_set_socket_timeout(redisContext &ctx) const {
    if (_opts.socket_timeout <= std::chrono::milliseconds(0)) {
        return;
    }

    if (redisSetTimeout(&ctx, _to_timeval(_opts.socket_timeout)) != REDIS_OK) {
        throw_error(ctx, "Failed to set socket timeout");
    }
}

void Connection::Connector::_enable_keep_alive(redisContext &ctx) const {
    if (!_opts.keep_alive) {
        return;
    }

    if (redisEnableKeepAlive(&ctx) != REDIS_OK) {
        throw_error(ctx, "Failed to enable keep alive option");
    }
}

timeval Connection::Connector::_to_timeval(const std::chrono::milliseconds &dur) const {
    auto sec = std::chrono::duration_cast<std::chrono::seconds>(dur);
    auto msec = std::chrono::duration_cast<std::chrono::microseconds>(dur - sec);

    return {
            static_cast<std::time_t>(sec.count()),
            static_cast<suseconds_t>(msec.count())
    };
}

void swap(Connection &lhs, Connection &rhs) noexcept {
    std::swap(lhs._ctx, rhs._ctx);
    std::swap(lhs._last_active, rhs._last_active);
    std::swap(lhs._opts, rhs._opts);
}

Connection::Connection(const ConnectionOptions &opts) :
            _ctx(Connector(opts).connect()),
            _last_active(std::chrono::steady_clock::now()),
            _opts(opts) {
    assert(_ctx && !broken());

    _set_options();
}

void Connection::reconnect() {
    Connection connection(_opts);

    swap(*this, connection);
}

void Connection::send(int argc, const char **argv, const std::size_t *argv_len) {
    auto ctx = _context();

    assert(ctx != nullptr);

    if (redisAppendCommandArgv(ctx,
                                argc,
                                argv,
                                argv_len) != REDIS_OK) {
        throw_error(*ctx, "Failed to send command");
    }

    assert(!broken());
}

void Connection::send(CmdArgs &args) {
    auto ctx = _context();

    assert(ctx != nullptr);

    if (redisAppendCommandArgv(ctx,
                                args.size(),
                                args.argv(),
                                args.argv_len()) != REDIS_OK) {
        throw_error(*ctx, "Failed to send command");
    }

    assert(!broken());
}

ReplyUPtr Connection::recv() {
    auto *ctx = _context();

    assert(ctx != nullptr);

    void *r = nullptr;
    if (redisGetReply(ctx, &r) != REDIS_OK) {
        throw_error(*ctx, "Failed to get reply");
    }

    assert(!broken() && r != nullptr);

    auto reply = ReplyUPtr(static_cast<redisReply*>(r));

    if (reply::is_error(*reply)) {
        throw_error(*reply);
    }

    return reply;
}

void Connection::_set_options() {
    _auth();

    _select_db();
}

void Connection::_auth() {
    if (_opts.password.empty()) {
        return;
    }

    cmd::auth(*this, _opts.password);

    auto reply = recv();

    reply::parse<void>(*reply);
}

void Connection::_select_db() {
    if (_opts.db == 0) {
        return;
    }

    cmd::select(*this, _opts.db);

    auto reply = recv();

    reply::parse<void>(*reply);
}

}

}
