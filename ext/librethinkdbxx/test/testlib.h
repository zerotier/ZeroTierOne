#pragma once

#include <sstream>
#include <cstdio>
#include <stack>
#include <cmath>
#include <regex>

#include <rethinkdb.h>

namespace R = RethinkDB;

extern std::vector<std::pair<const char*, bool>> section;
extern int failed;
extern int count;
extern std::unique_ptr<R::Connection> conn;
extern int verbosity;

const char* indent();

void enter_section(const char* name);
void section_cleanup();
void exit_section();

#define TEST_DO(code)                                                   \
    if (verbosity > 1) fprintf(stderr, "%sTEST: %s\n", indent(), #code); \
    code

#define TEST_EQ(code, expected) \
    do {                                                                \
        if (verbosity > 1) fprintf(stderr, "%sTEST: %s\n", indent(), #code); \
        try { test_eq(#code, (code), (expected)); }                     \
        catch (const R::Error& error) { test_eq(#code, error, (expected)); } \
    } while (0)

struct err {
    err(const char* type_, std::string message_, R::Array&& backtrace_ = {}) :
        type(type_), message(message_), backtrace(std::move(backtrace_)) { }

    std::string convert_type() const {
        return type;
    }

    static std::string trim_message(std::string msg) {
        size_t i = msg.find(":\n");
        if (i != std::string::npos) {
            return msg.substr(0, i + 1);
        }
        return msg;
    }

    std::string type;
    std::string message;
    R::Array backtrace;
};

struct err_regex {
    err_regex(const char* type_, const char* message_, R::Array&& backtrace_ = {}) :
        type(type_), message(message_), backtrace(std::move(backtrace_)) { }
    std::string type;
    std::string message;
    R::Array backtrace;
    std::string regex() const {
        return type + ": " + message;
    }
};

R::Object regex(const char* pattern);

bool match(const char* pattern, const char* string);

R::Object partial(R::Object&& object);
R::Object partial(R::Array&& array);
R::Datum uuid();
R::Object arrlen(int n, R::Datum&& datum);
R::Object arrlen(int n);
R::Term new_table();
std::string repeat(std::string&& s, int n);
R::Term fetch(R::Cursor& cursor, int count = -1, double timeout = 1);
R::Object bag(R::Array&& array);
R::Object bag(R::Datum&& d);

struct temp_table {
    temp_table() {
        char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        char name_[15] = "temp_";
        for (unsigned int i = 5; i + 1 < sizeof name_; ++i) {
            name_[i] = chars[random() % (sizeof chars - 1)];
        }
        name_[14] = 0;
        R::table_create(name_).run(*conn);
        name = name_;
    }

    ~temp_table() {
        try {
            R::table_drop(name).run(*conn);
        } catch (const R::Error &e) {
            if(!strstr(e.message.c_str(), "does not exist")){
                printf("error dropping temp_table: %s\n", e.message.c_str());
            }
        }
    }

    R::Term table() { return R::table(name); }
    std::string name;
};

void clean_slate();

// std::string to_string(const R::Cursor&);
std::string to_string(const R::Term&);
std::string to_string(const R::Datum&);
std::string to_string(const R::Error&);
std::string to_string(const err_regex&);
std::string to_string(const err&);

bool equal(const R::Datum&, const R::Datum&);
bool equal(const R::Error&, const err_regex&);
bool equal(const R::Error&, const err&);

template <class T>
bool equal(const T& a, const err& b) {
    return false;
}

template <class T>
bool equal(const T& a, const err_regex& b) {
    return false;
}

template <class T>
bool equal(const R::Error& a, const T& b) {
    return false;
}

std::string truncate(std::string&&);

template <class T, class U>
void test_eq(const char* code, const T val, const U expected) {

    try {
        count ++;
        if (!equal(val, expected)) {
            failed++;
            for (auto& it : section) {
                if (it.second) {
                    printf("%sSection: %s\n", indent(), it.first);
                    it.second = false;
                }
            }
            try {
                printf("%sFAILURE in ‘%s’:\n%s  Expected: ‘%s’\n%s   but got: ‘%s’\n",
                    indent(), code,
                    indent(), truncate(to_string(expected)).c_str(),
                    indent(), truncate(to_string(val)).c_str());
            } catch (const R::Error& e) {
                printf("%sFAILURE: Failed to print failure description: %s\n", indent(), e.message.c_str());
            } catch (...) {
                printf("%sFAILURE: Failed to print failure description\n", indent());
            }
        }
    } catch (const std::regex_error& rx_err) {
        printf("%sSKIP: error with regex (likely a buggy regex implementation): %s\n", indent(), rx_err.what());
    }
}

template <class U>
void test_eq(const char* code, const R::Cursor& val, const U expected) {
    try {
        R::Datum result = val.to_datum();
        test_eq(code, result, expected);
    } catch (R::Error& error) {
        test_eq(code, error, expected);
    }
}

int len(const R::Datum&);

R::Term wait(int n);

#define PacificTimeZone() (-7 * 3600)
#define UTCTimeZone() (0)

extern R::Datum nil;

inline R::Cursor maybe_run(R::Cursor& c, R::Connection&, R::OptArgs&& o = {}) {
    return std::move(c);
}

inline R::Cursor maybe_run(R::Term q, R::Connection& c, R::OptArgs&& o = {}) {
    return q.run(c, std::move(o));
}

inline int operator+(R::Datum a, int b) {
    return a.extract_number() + b;
}

inline R::Array operator*(R::Array arr, int n) {
    R::Array ret;
    for(int i = 0; i < n; i++) {
        for(const auto& it: arr) {
            ret.push_back(it);
        }
    }
    return ret;
}

inline R::Array array_range(int x, int y) {
    R::Array ret;
    for(int i = x; i < y; ++i) {
        ret.push_back(i);
    }
    return ret;
}

template <class F>
inline R::Array array_map(F f, R::Array a){
    R::Array ret;
    for(R::Datum& d: a) {
        ret.push_back(f(d.extract_number()));
    }
    return ret;
}

R::Array append(R::Array lhs, R::Array rhs);

template <class T>
std::string str(T x){
    return to_string(x);
}
