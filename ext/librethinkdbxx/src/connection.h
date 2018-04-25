#pragma once

#include <string>
#include <queue>
#include <mutex>
#include <memory>
#include <condition_variable>

#include "protocol_defs.h"
#include "datum.h"
#include "error.h"

#define FOREVER (-1)
#define SECOND 1
#define MICROSECOND 0.000001

namespace RethinkDB {

class Term;
using OptArgs = std::map<std::string, Term>;

// A connection to a RethinkDB server
// It contains:
//  * A socket
//  * Read and write locks
//  * A cache of responses that have not been read by the corresponding Cursor
class ConnectionPrivate;
class Connection {
public:
    Connection() = delete;
    Connection(const Connection&) noexcept = delete;
    Connection(Connection&&) noexcept = delete;
    Connection& operator=(Connection&&) noexcept = delete;
    Connection& operator=(const Connection&) noexcept = delete;
    ~Connection();

    void close();

private:
    explicit Connection(ConnectionPrivate *dd);
    std::unique_ptr<ConnectionPrivate> d;

    Cursor start_query(Term *term, OptArgs&& args);
    void stop_query(uint64_t);
    void continue_query(uint64_t);

    friend class Cursor;
    friend class CursorPrivate;
    friend class Token;
    friend class Term;
    friend std::unique_ptr<Connection>
        connect(std::string host, int port, std::string auth_key);

};

// $doc(connect)
std::unique_ptr<Connection> connect(std::string host = "localhost", int port = 28015, std::string auth_key = "");

}
