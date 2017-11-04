#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>

#include <netdb.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <cinttypes>
#include <memory>

#include "connection.h"
#include "connection_p.h"
#include "json_p.h"
#include "exceptions.h"
#include "term.h"
#include "cursor_p.h"

#include "rapidjson-config.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/encodedstream.h"
#include "rapidjson/document.h"

namespace RethinkDB {

using QueryType = Protocol::Query::QueryType;

// constants
const int debug_net = 0;
const uint32_t version_magic =
    static_cast<uint32_t>(Protocol::VersionDummy::Version::V0_4);
const uint32_t json_magic =
    static_cast<uint32_t>(Protocol::VersionDummy::Protocol::JSON);

std::unique_ptr<Connection> connect(std::string host, int port, std::string auth_key) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, 16, "%d", port);
    struct addrinfo *servinfo;
    int ret = getaddrinfo(host.c_str(), port_str, &hints, &servinfo);
    if (ret) throw Error("getaddrinfo: %s\n", gai_strerror(ret));

    struct addrinfo *p;
    Error error;
    int sockfd;
    for (p = servinfo; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
            error = Error::from_errno("socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            ::close(sockfd);
            error = Error::from_errno("connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        throw error;
    }

    freeaddrinfo(servinfo);

    std::unique_ptr<ConnectionPrivate> conn_private(new ConnectionPrivate(sockfd));
    WriteLock writer(conn_private.get());
    {
        size_t size = auth_key.size();
        char buf[12 + size];
        memcpy(buf, &version_magic, 4);
        uint32_t n = size;
        memcpy(buf + 4, &n, 4);
        memcpy(buf + 8, auth_key.data(), size);
        memcpy(buf + 8 + size, &json_magic, 4);
        writer.send(buf, sizeof buf);
    }

    ReadLock reader(conn_private.get());
    {
        const size_t max_response_length = 1024;
        char buf[max_response_length + 1];
        size_t len = reader.recv_cstring(buf, max_response_length);
        if (len == max_response_length || strcmp(buf, "SUCCESS")) {
            buf[len] = 0;
            ::close(sockfd);
            throw Error("Server rejected connection with message: %s", buf);
        }
    }

    return std::unique_ptr<Connection>(new Connection(conn_private.release()));
}

Connection::Connection(ConnectionPrivate *dd) : d(dd) { }
Connection::~Connection() {
    // close();
    if (d->guarded_sockfd >= 0)
        ::close(d->guarded_sockfd);
}

size_t ReadLock::recv_some(char* buf, size_t size, double wait) {
    if (wait != FOREVER) {
        while (true) {
            fd_set readfds;
            struct timeval tv;

            FD_ZERO(&readfds);
            FD_SET(conn->guarded_sockfd, &readfds);

            tv.tv_sec = (int)wait;
            tv.tv_usec = (int)((wait - (int)wait) / MICROSECOND);
            int rv = select(conn->guarded_sockfd + 1, &readfds, NULL, NULL, &tv);
            if (rv == -1) {
                throw Error::from_errno("select");
            } else if (rv == 0) {
                throw TimeoutException();
            }

            if (FD_ISSET(conn->guarded_sockfd, &readfds)) {
                break;
            }
        }
    }

    ssize_t numbytes = ::recv(conn->guarded_sockfd, buf, size, 0);
    if (numbytes <= 0) throw Error::from_errno("recv");
    if (debug_net > 1) {
        fprintf(stderr, "<< %s\n", write_datum(std::string(buf, numbytes)).c_str());
    }

    return numbytes;
}

void ReadLock::recv(char* buf, size_t size, double wait) {
    while (size) {
        size_t numbytes = recv_some(buf, size, wait);

        buf += numbytes;
        size -= numbytes;
    }
}

size_t ReadLock::recv_cstring(char* buf, size_t max_size){
    size_t size = 0;
    for (; size < max_size; size++) {
        recv(buf, 1, FOREVER);
        if (*buf == 0) {
            break;
        }
        buf++;
    }
    return size;
}

void WriteLock::send(const char* buf, size_t size) {
    while (size) {
        ssize_t numbytes = ::write(conn->guarded_sockfd, buf, size);
        if (numbytes == -1) throw Error::from_errno("write");
        if (debug_net > 1) {
            fprintf(stderr, ">> %s\n", write_datum(std::string(buf, numbytes)).c_str());
        }

        buf += numbytes;
        size -= numbytes;
    }
}

void WriteLock::send(const std::string data) {
    send(data.data(), data.size());
}

std::string ReadLock::recv(size_t size) {
    char buf[size];
    recv(buf, size, FOREVER);
    return buf;
}

void Connection::close() {
    CacheLock guard(d.get());
    for (auto& it : d->guarded_cache) {
        stop_query(it.first);
    }

    int ret = ::close(d->guarded_sockfd);
    if (ret == -1) {
        throw Error::from_errno("close");
    }
    d->guarded_sockfd = -1;
}

Response ConnectionPrivate::wait_for_response(uint64_t token_want, double wait) {
    CacheLock guard(this);
    ConnectionPrivate::TokenCache& cache = guarded_cache[token_want];

    while (true) {
        if (!cache.responses.empty()) {
            Response response(std::move(cache.responses.front()));
            cache.responses.pop();
            if (cache.closed && cache.responses.empty()) {
                guarded_cache.erase(token_want);
            }

            return response;
        }

        if (cache.closed) {
            throw Error("Trying to read from a closed token");
        }

        if (guarded_loop_active) {
            cache.cond.wait(guard.inner_lock);
        } else {
            break;
        }
    }

    ReadLock reader(this);
    return reader.read_loop(token_want, std::move(guard), wait);
}

Response ReadLock::read_loop(uint64_t token_want, CacheLock&& guard, double wait) {
    if (!guard.inner_lock) {
        guard.lock();
    }
    if (conn->guarded_loop_active) {
        throw Error("Cannot run more than one read loop on the same connection");
    }
    conn->guarded_loop_active = true;
    guard.unlock();

    try {
        while (true) {
            char buf[12];
            bzero(buf, sizeof(buf));
            recv(buf, 12, wait);
            uint64_t token_got;
            memcpy(&token_got, buf, 8);
            uint32_t length;
            memcpy(&length, buf + 8, 4);

            std::unique_ptr<char[]> bufmem(new char[length + 1]);
            char *buffer = bufmem.get();
            bzero(buffer, length + 1);
            recv(buffer, length, wait);
            buffer[length] = '\0';

            rapidjson::Document json;
            json.ParseInsitu(buffer);
            if (json.HasParseError()) {
                fprintf(stderr, "json parse error, code: %d, position: %d\n",
                    (int)json.GetParseError(), (int)json.GetErrorOffset());
            } else if (json.IsNull()) {
                fprintf(stderr, "null value, read: %s\n", buffer);
            }

            Datum datum = read_datum(json);
            if (debug_net > 0) {
                fprintf(stderr, "[%" PRIu64 "] << %s\n", token_got, write_datum(datum).c_str());
            }

            Response response(std::move(datum));

            if (token_got == token_want) {
                guard.lock();
                if (response.type != Protocol::Response::ResponseType::SUCCESS_PARTIAL) {
                    auto it = conn->guarded_cache.find(token_got);
                    if (it != conn->guarded_cache.end()) {
                        it->second.closed = true;
                        it->second.cond.notify_all();
                    }
                    conn->guarded_cache.erase(it);
                }
                conn->guarded_loop_active = false;
                for (auto& it : conn->guarded_cache) {
                    it.second.cond.notify_all();
                }
                return response;
            } else {
                guard.lock();
                auto it = conn->guarded_cache.find(token_got);
                if (it == conn->guarded_cache.end()) {
                    // drop the response
                } else if (!it->second.closed) {
                    it->second.responses.emplace(std::move(response));
                    if (response.type != Protocol::Response::ResponseType::SUCCESS_PARTIAL) {
                        it->second.closed = true;
                    }
                }
                it->second.cond.notify_all();
                guard.unlock();
            }
        }
    } catch (const TimeoutException &e) {
        if (!guard.inner_lock){
            guard.lock();
        }
        conn->guarded_loop_active = false;
        throw e;
    }
}

void ConnectionPrivate::run_query(Query query, bool no_reply) {
    WriteLock writer(this);
    writer.send(query.serialize());
}

Cursor Connection::start_query(Term *term, OptArgs&& opts) {
    bool no_reply = false;
    auto it = opts.find("noreply");
    if (it != opts.end()) {
        no_reply = *(it->second.datum.get_boolean());
    }

    uint64_t token = d->new_token();
    {
        CacheLock guard(d.get());
        d->guarded_cache[token];
    }

    d->run_query(Query{QueryType::START, token, term->datum, std::move(opts)});
    if (no_reply) {
        return Cursor(new CursorPrivate(token, this, Nil()));
    }

    Cursor cursor(new CursorPrivate(token, this));
    Response response = d->wait_for_response(token, FOREVER);
    cursor.d->add_response(std::move(response));
    return cursor;
}

void Connection::stop_query(uint64_t token) {
    const auto& it = d->guarded_cache.find(token);
    if (it != d->guarded_cache.end() && !it->second.closed) {
        d->run_query(Query{QueryType::STOP, token}, true);
    }
}

void Connection::continue_query(uint64_t token) {
    d->run_query(Query{QueryType::CONTINUE, token}, true);
}

Error Response::as_error() {
    std::string repr;
    if (result.size() == 1) {
        std::string* string = result[0].get_string();
        if (string) {
            repr = *string;
        } else {
            repr = write_datum(result[0]);
        }
    } else {
        repr = write_datum(Datum(result));
    }
    std::string err;
    using RT = Protocol::Response::ResponseType;
    using ET = Protocol::Response::ErrorType;
    switch (type) {
    case RT::SUCCESS_SEQUENCE: err = "unexpected response: SUCCESS_SEQUENCE"; break;
    case RT::SUCCESS_PARTIAL:  err = "unexpected response: SUCCESS_PARTIAL"; break;
    case RT::SUCCESS_ATOM: err = "unexpected response: SUCCESS_ATOM"; break;
    case RT::WAIT_COMPLETE: err = "unexpected response: WAIT_COMPLETE"; break;
    case RT::SERVER_INFO: err = "unexpected response: SERVER_INFO"; break;
    case RT::CLIENT_ERROR: err = "ReqlDriverError"; break;
    case RT::COMPILE_ERROR: err = "ReqlCompileError"; break;
    case RT::RUNTIME_ERROR:
        switch (error_type) {
        case ET::INTERNAL: err = "ReqlInternalError"; break;
        case ET::RESOURCE_LIMIT: err = "ReqlResourceLimitError"; break;
        case ET::QUERY_LOGIC: err = "ReqlQueryLogicError"; break;
        case ET::NON_EXISTENCE: err = "ReqlNonExistenceError"; break;
        case ET::OP_FAILED: err = "ReqlOpFailedError"; break;
        case ET::OP_INDETERMINATE: err = "ReqlOpIndeterminateError"; break;
        case ET::USER: err = "ReqlUserError"; break;
        case ET::PERMISSION_ERROR: err = "ReqlPermissionError"; break;
        default: err = "ReqlRuntimeError"; break;
        }
    }
    throw Error("%s: %s", err.c_str(), repr.c_str());
}

Protocol::Response::ResponseType response_type(double t) {
    int n = static_cast<int>(t);
    using RT = Protocol::Response::ResponseType;
    switch (n) {
    case static_cast<int>(RT::SUCCESS_ATOM):
        return RT::SUCCESS_ATOM;
    case static_cast<int>(RT::SUCCESS_SEQUENCE):
        return RT::SUCCESS_SEQUENCE;
    case static_cast<int>(RT::SUCCESS_PARTIAL):
        return RT::SUCCESS_PARTIAL;
    case static_cast<int>(RT::WAIT_COMPLETE):
        return RT::WAIT_COMPLETE;
    case static_cast<int>(RT::CLIENT_ERROR):
        return RT::CLIENT_ERROR;
    case static_cast<int>(RT::COMPILE_ERROR):
        return RT::COMPILE_ERROR;
    case static_cast<int>(RT::RUNTIME_ERROR):
        return RT::RUNTIME_ERROR;
    default:
        throw Error("Unknown response type");
    }
}

Protocol::Response::ErrorType runtime_error_type(double t) {
    int n = static_cast<int>(t);
    using ET = Protocol::Response::ErrorType;
    switch (n) {
    case static_cast<int>(ET::INTERNAL):
        return ET::INTERNAL;
    case static_cast<int>(ET::RESOURCE_LIMIT):
        return ET::RESOURCE_LIMIT;
    case static_cast<int>(ET::QUERY_LOGIC):
        return ET::QUERY_LOGIC;
    case static_cast<int>(ET::NON_EXISTENCE):
        return ET::NON_EXISTENCE;
    case static_cast<int>(ET::OP_FAILED):
        return ET::OP_FAILED;
    case static_cast<int>(ET::OP_INDETERMINATE):
        return ET::OP_INDETERMINATE;
    case static_cast<int>(ET::USER):
        return ET::USER;
    default:
        throw Error("Unknown error type");
    }
}

}
