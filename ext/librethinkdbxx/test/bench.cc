/*
#include <signal.h>
#include <ctime>
#include <chrono>
#include <rethinkdb.h>

namespace R = RethinkDB;
std::unique_ptr<R::Connection> conn;

int main() {
    signal(SIGPIPE, SIG_IGN);
    try {
        conn = R::connect();
    } catch(const R::Error& error) {
        printf("FAILURE: could not connect to localhost:28015: %s\n", error.message.c_str());
        return 1;
    }

    try {
        printf("running test...\n");
        auto start = std::chrono::steady_clock::now();
        R::Datum d = R::range(1, 1000000)
          .map([]() { return R::object("test", "hello", "data", "world"); })
          .run(*conn);
        auto end = std::chrono::steady_clock::now();
        auto diff = end - start;

        printf("result size: %d\n", (int)d.get_array()->size());
        printf("completed in %f ms\n", std::chrono::duration<double, std::milli>(diff).count());
    } catch (const R::Error& error) {
        printf("FAILURE: uncaught exception: %s\n", error.message.c_str());
        return 1;
    }
}
*/

#include <iostream>
#include <rethinkdb.h>

namespace R = RethinkDB;

int main() {
    auto conn = R::connect();
    if (!conn) {
        std::cerr << "Could not connect to server\n";
        return 1;
    }

    std::cout << "Connected" << std::endl;
    R::Cursor databases = R::db_list().run(*conn);
    for (R::Datum const& db : databases) {
        std::cout << *db.get_string() << '\n';
    }

    return 0;
}


