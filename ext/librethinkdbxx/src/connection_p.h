#ifndef CONNECTION_P_H
#define CONNECTION_P_H

#include <inttypes.h>

#include "connection.h"
#include "term.h"
#include "json_p.h"

namespace RethinkDB {

extern const int debug_net;

struct Query {
    Protocol::Query::QueryType type;
    uint64_t token;
    Datum term;
    OptArgs optArgs;

    std::string serialize() {
        Array query_arr{static_cast<double>(type)};
        if (term.is_valid()) query_arr.emplace_back(term);
        if (!optArgs.empty())
            query_arr.emplace_back(Term(std::move(optArgs)).datum);

        std::string query_str = write_datum(query_arr);
        if (debug_net > 0) {
            fprintf(stderr, "[%" PRIu64 "] >> %s\n", token, query_str.c_str());
        }

        char header[12];
        memcpy(header, &token, 8);
        uint32_t size = query_str.size();
        memcpy(header + 8, &size, 4);
        query_str.insert(0, header, 12);
        return query_str;
    }
};

// Used internally to convert a raw response type into an enum
Protocol::Response::ResponseType response_type(double t);
Protocol::Response::ErrorType runtime_error_type(double t);

// Contains a response from the server. Use the Cursor class to interact with these responses
class Response {
public:
    Response() = delete;
    explicit Response(Datum&& datum) :
        type(response_type(std::move(datum).extract_field("t").extract_number())),
        error_type(datum.get_field("e") ?
                   runtime_error_type(std::move(datum).extract_field("e").extract_number()) :
                   Protocol::Response::ErrorType(0)),
        result(std::move(datum).extract_field("r").extract_array()) { }
    Error as_error();
    Protocol::Response::ResponseType type;
    Protocol::Response::ErrorType error_type;
    Array result;
};

class Token;
class ConnectionPrivate {
public:
    ConnectionPrivate(int sockfd)
        : guarded_next_token(1), guarded_sockfd(sockfd), guarded_loop_active(false)
    { }

    void run_query(Query query, bool no_reply = false);

    Response wait_for_response(uint64_t, double);
    uint64_t new_token() {
        return guarded_next_token++;
    }

    std::mutex read_lock;
    std::mutex write_lock;
    std::mutex cache_lock;

    struct TokenCache {
        bool closed = false;
        std::condition_variable cond;
        std::queue<Response> responses;
    };

    std::map<uint64_t, TokenCache> guarded_cache;
    uint64_t guarded_next_token;
    int guarded_sockfd;
    bool guarded_loop_active;
};

class CacheLock {
public:
    CacheLock(ConnectionPrivate* conn) : inner_lock(conn->cache_lock) { }

    void lock() {
        inner_lock.lock();
    }

    void unlock() {
        inner_lock.unlock();
    }

    std::unique_lock<std::mutex> inner_lock;
};

class ReadLock {
public:
    ReadLock(ConnectionPrivate* conn_) : lock(conn_->read_lock), conn(conn_) { }

    size_t recv_some(char*, size_t, double wait);
    void recv(char*, size_t, double wait);
    std::string recv(size_t);
    size_t recv_cstring(char*, size_t);

    Response read_loop(uint64_t, CacheLock&&, double);

    std::lock_guard<std::mutex> lock;
    ConnectionPrivate* conn;
};

class WriteLock {
public:
    WriteLock(ConnectionPrivate* conn_) : lock(conn_->write_lock), conn(conn_) { }

    void send(const char*, size_t);
    void send(std::string);

    std::lock_guard<std::mutex> lock;
    ConnectionPrivate* conn;
};

}   // namespace RethinkDB

#endif  // CONNECTION_P_H
