#pragma once

#include <cstdarg>
#include <cstring>
#include <string>
#include <cerrno>

namespace RethinkDB {

// All errors thrown by the server have this type
struct Error {
    template <class ...T>
    explicit Error(const char* format_, T... A) {
        format(format_, A...);
    }

    Error() = default;
    Error(Error&&) = default;
    Error(const Error&) = default;

    Error& operator= (Error&& other) {
        message = std::move(other.message);
        return *this;
    }

    static Error from_errno(const char* str){
        return Error("%s: %s", str, strerror(errno));
    }

    // The error message
    std::string message;

private:
    const size_t max_message_size = 2048;

    void format(const char* format_, ...) {
        va_list args;
        va_start(args, format_);
        char message_[max_message_size];
        vsnprintf(message_, max_message_size, format_, args);
        va_end(args);
        message = message_;
    }
};

}
