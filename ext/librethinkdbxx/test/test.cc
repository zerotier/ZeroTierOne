#include <signal.h>

#include <ctime>

#include "testlib.h"

extern void run_upstream_tests();

void test_json(const char* string, const char* ret = "") {
    TEST_EQ(R::Datum::from_json(string).as_json().c_str(), ret[0] ? ret : string);
}

void test_json_parse_print() {
    enter_section("json");
    test_json("-0.0", "-0.0");
    test_json("null");
    test_json("1.2");
    test_json("1.2e20", "1.2e+20");
    test_json("true");
    test_json("false");
    test_json("\"\"");
    test_json("\"\\u1234\"", "\"\u1234\"");
    test_json("\"\\\"\"");
    test_json("\"foobar\"");
    test_json("[]");
    test_json("[1]");
    test_json("[1,2,3,4]");
    test_json("{}");
    test_json("{\"a\":1}");
    test_json("{\"a\":1,\"b\":2,\"c\":3}");
    exit_section();
}

void test_reql() {
    enter_section("reql");
    TEST_EQ((R::expr(1) + 2).run(*conn), R::Datum(3));
    TEST_EQ(R::range(4).count().run(*conn), R::Datum(4));
    TEST_EQ(R::js("Math.abs")(-1).run(*conn), 1);
    exit_section();
}

void test_cursor() {
    enter_section("cursor");
    R::Cursor cursor = R::range(10000).run(*conn);
    TEST_EQ(cursor.next(), 0);
    R::Array array = cursor.to_array();
    TEST_EQ(array.size(), 9999);
    TEST_EQ(*array.begin(), 1);
    TEST_EQ(*array.rbegin(), 9999);
    int i = 0;
    R::range(3).run(*conn).each([&i](R::Datum&& datum){
            TEST_EQ(datum, i++); });
    exit_section();
}

void test_encode(const char* str, const char* b) {
    TEST_EQ(R::base64_encode(str), b);
}

void test_decode(const char* b, const char* str) {
    std::string out;
    TEST_EQ(R::base64_decode(b, out), true);
    TEST_EQ(out, str);
}

#define TEST_B64(a, b) test_encode(a, b); test_decode(b, a)

void test_binary() {
    enter_section("base64");
    TEST_B64("", "");
    TEST_B64("foo", "Zm9v");
    exit_section();
}

void test_issue28() {
    enter_section("issue #28");
    std::vector<std::string> expected{ "rethinkdb", "test" };
    std::vector<std::string> dbs;
    R::Cursor databases = R::db_list().run(*conn);
    for (R::Datum const& db : databases) {
        dbs.push_back(*db.get_string());
    }

    TEST_EQ(dbs, expected);
    exit_section();
}

int main() {
    signal(SIGPIPE, SIG_IGN);
    srand(time(NULL));
    try {
        conn = R::connect();
    } catch(const R::Error& error) {
        printf("FAILURE: could not connect to localhost:28015: %s\n", error.message.c_str());
        return 1;
    }
    try {
        //test_binary();
        //test_json_parse_print();
        //test_reql();
        //test_cursor();
        test_issue28();
        run_upstream_tests();
    } catch (const R::Error& error) {
        printf("FAILURE: uncaught expception: %s\n", error.message.c_str());
        return 1;
    }
    if (!failed) {
        printf("SUCCESS: %d tests passed\n", count);
    } else {
        printf("DONE: %d of %d tests failed\n", failed, count);
        return 1;
    }
}
