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
#include <tuple>
#include <algorithm>
#include "reply.h"
#include "command.h"
#include "command_args.h"

#ifdef _MSC_VER

#include <winsock2.h>   // for `timeval` with MSVC compiler

#endif

namespace sw {

namespace redis {

ConnectionOptions::ConnectionOptions(const std::string &uri) :
                                        ConnectionOptions(_parse_uri(uri)) {}

ConnectionOptions ConnectionOptions::_parse_uri(const std::string &uri) const {
    std::string type;
    std::string auth;
    std::string path;
    std::tie(type, auth, path) = _split_uri(uri);

    ConnectionOptions opts;

    _set_auth_opts(auth, opts);

    auto db = 0;
    std::string parameter_string;
    std::tie(path, db, parameter_string) = _split_path(path);

    _parse_parameters(parameter_string, opts);

    opts.db = db;

    if (type == "tcp") {
        _set_tcp_opts(path, opts);
    } else if (type == "unix") {
        _set_unix_opts(path, opts);
    } else {
        throw Error("invalid URI: invalid type");
    }

    return opts;
}

void ConnectionOptions::_parse_parameters(const std::string &parameter_string,
                                            ConnectionOptions &opts) const {
    auto parameters = _split(parameter_string, "&");
    if (parameters.empty()) {
        // No parameters
        return;
    }

    for (const auto &parameter : parameters) {
        auto kv_pair = _split(parameter, "=");
        if (kv_pair.size() != 2) {
            throw Error("invalid option: not a key-value pair: " + parameter);
        }

        const auto &key = kv_pair[0];
        const auto &val = kv_pair[1];
        _set_option(key, val, opts);
    }
}

void ConnectionOptions::_set_option(const std::string &key,
                                    const std::string &val,
                                    ConnectionOptions &opts) const {
    if (key == "keep_alive") {
        opts.keep_alive = _parse_bool_option(val);
    } else if (key == "connect_timeout") {
        opts.connect_timeout = _parse_timeout_option(val);
    } else if (key == "socket_timeout") {
        opts.socket_timeout = _parse_timeout_option(val);
    } else {
        throw Error("unknown uri parameter");
    }
}

bool ConnectionOptions::_parse_bool_option(const std::string &str) const {
    if (str == "true") {
        return true;
    } else if (str == "false") {
        return false;
    } else {
        throw Error("invalid uri parameter of bool type: " + str);
    }
}

std::chrono::milliseconds ConnectionOptions::_parse_timeout_option(const std::string &str) const {
    std::size_t timeout = 0;
    std::string unit;
    try {
        std::size_t pos = 0;
        timeout = std::stoul(str, &pos);
        unit = str.substr(pos);
    } catch (const std::exception &e) {
        throw Error("invalid uri parameter of timeout type: " + str);
    }

    if (unit == "ms") {
        return std::chrono::milliseconds(timeout);
    } else if (unit == "s") {
        return std::chrono::seconds(timeout);
    } else if (unit == "m") {
        return std::chrono::minutes(timeout);
    } else {
        throw Error("unknown timeout unit: " + unit);
    }
}

std::vector<std::string> ConnectionOptions::_split(const std::string &str,
                                                    const std::string &delimiter) const {
    if (str.empty()) {
        return {};
    }

    std::vector<std::string> fields;

    if (delimiter.empty()) {
        std::transform(str.begin(), str.end(), std::back_inserter(fields),
                [](char c) { return std::string(1, c); });
        return fields;
    }

    std::string::size_type pos = 0;
    std::string::size_type idx = 0;
    while (true) {
        pos = str.find(delimiter, idx);
        if (pos == std::string::npos) {
            fields.push_back(str.substr(idx));
            break;
        }

        fields.push_back(str.substr(idx, pos - idx));
        idx = pos + delimiter.size();
    }

    return fields;
}

auto ConnectionOptions::_split_uri(const std::string &uri) const
    -> std::tuple<std::string, std::string, std::string> {
    auto pos = uri.find("://");
    if (pos == std::string::npos) {
        throw Error("invalid URI: no scheme");
    }

    auto type = uri.substr(0, pos);

    auto start = pos + 3;
    pos = uri.find("@", start);
    if (pos == std::string::npos) {
        // No auth info.
        return std::make_tuple(type, std::string{}, uri.substr(start));
    }

    auto auth = uri.substr(start, pos - start);

    return std::make_tuple(type, auth, uri.substr(pos + 1));
}

auto ConnectionOptions::_split_path(const std::string &path) const
    -> std::tuple<std::string, int, std::string> {
    auto parameter_pos = path.rfind("?");
    std::string parameter_string;
    if (parameter_pos != std::string::npos) {
        parameter_string = path.substr(parameter_pos + 1);
    }

    auto pos = path.rfind("/");
    if (pos != std::string::npos) {
        // Might specified a db number.
        try {
            auto db = std::stoi(path.substr(pos + 1));

            return std::make_tuple(path.substr(0, pos), db, parameter_string);
        } catch (const std::exception &) {
            // Not a db number, and it might be a path to unix domain socket.
        }
    }

    // No db number specified, and use default one, i.e. 0.
    return std::make_tuple(path.substr(0, parameter_pos), 0, parameter_string);
}

void ConnectionOptions::_set_auth_opts(const std::string &auth, ConnectionOptions &opts) const {
    if (auth.empty()) {
        // No auth info.
        return;
    }

    auto pos = auth.find(":");
    if (pos == std::string::npos) {
        // No user name.
        opts.password = auth;
    } else {
        opts.user = auth.substr(0, pos);
        opts.password = auth.substr(pos + 1);
    }
}

void ConnectionOptions::_set_tcp_opts(const std::string &path, ConnectionOptions &opts) const {
    opts.type = ConnectionType::TCP;

    auto pos = path.find(":");
    if (pos != std::string::npos) {
        // Port number specified.
        try {
            opts.port = std::stoi(path.substr(pos + 1));
        } catch (const std::exception &) {
            throw Error("invalid URI: invalid port");
        }
    } // else use default port, i.e. 6379.

    opts.host = path.substr(0, pos);
}

void ConnectionOptions::_set_unix_opts(const std::string &path, ConnectionOptions &opts) const {
    opts.type = ConnectionType::UNIX;
    opts.path = path;
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
        throw Error("Unknown connection type");
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

    timeval t;
    t.tv_sec = sec.count();
    t.tv_usec = msec.count();
    return t;
}

void swap(Connection &lhs, Connection &rhs) noexcept {
    std::swap(lhs._ctx, rhs._ctx);
    std::swap(lhs._create_time, rhs._create_time);
    std::swap(lhs._opts, rhs._opts);
}

Connection::Connection(const ConnectionOptions &opts) :
            _ctx(Connector(opts).connect()),
            _create_time(std::chrono::steady_clock::now()),
            _last_active(std::chrono::steady_clock::now()),
            _opts(opts) {
    assert(_ctx && !broken());

    const auto &tls_opts = opts.tls;
    // If not compiled with TLS, TLS is always disabled.
    if (tls::enabled(tls_opts)) {
        _tls_ctx = tls::secure_connection(*_ctx, tls_opts);
    }

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

ReplyUPtr Connection::recv(bool handle_error_reply) {
    auto *ctx = _context();

    assert(ctx != nullptr);

    void *r = nullptr;
    if (redisGetReply(ctx, &r) != REDIS_OK) {
        throw_error(*ctx, "Failed to get reply");
    }

    assert(!broken() && r != nullptr);

    auto reply = ReplyUPtr(static_cast<redisReply*>(r));

    if (handle_error_reply && reply::is_error(*reply)) {
        throw_error(*reply);
    }

    return reply;
}

void Connection::_set_options() {
    _auth();

    _select_db();

    if (_opts.readonly) {
        _enable_readonly();
    }
}

void Connection::_enable_readonly() {
    send("READONLY");

    auto reply = recv();

    assert(reply);

    reply::parse<void>(*reply);
}

void Connection::_auth() {
    const std::string DEFAULT_USER = "default";

    if (_opts.user == DEFAULT_USER && _opts.password.empty()) {
        return;
    }

    if (_opts.user == DEFAULT_USER) {
        cmd::auth(*this, _opts.password);
    } else {
        // Redis 6.0 or latter
        cmd::auth(*this, _opts.user, _opts.password);
    }

    auto reply = recv();

    assert(reply);

    reply::parse<void>(*reply);
}

void Connection::_select_db() {
    if (_opts.db == 0) {
        return;
    }

    cmd::select(*this, _opts.db);

    auto reply = recv();

    assert(reply);

    reply::parse<void>(*reply);
}

}

}
