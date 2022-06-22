/**************************************************************************
   Copyright (c) 2021 sewenew

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

#ifndef SEWENEW_REDISPLUSPLUS_CXX_UTILS_H
#define SEWENEW_REDISPLUSPLUS_CXX_UTILS_H

#include <string>
#include <utility>

namespace sw {

namespace redis {

// By now, not all compilers support std::string_view and std::optional,
// so we make our own implementation.

class StringView {
public:
    constexpr StringView() noexcept = default;

    constexpr StringView(const char *data, std::size_t size) : _data(data), _size(size) {}

    StringView(const char *data) : _data(data), _size(std::strlen(data)) {}

    StringView(const std::string &str) : _data(str.data()), _size(str.size()) {}

    constexpr StringView(const StringView &) noexcept = default;

    StringView& operator=(const StringView &) noexcept = default;

    constexpr const char* data() const noexcept {
        return _data;
    }

    constexpr std::size_t size() const noexcept {
        return _size;
    }

private:
    const char *_data = nullptr;
    std::size_t _size = 0;
};

template <typename T>
class Optional {
public:
#if defined(_MSC_VER) && (_MSC_VER < 1910)
    Optional() : _value() {}    // MSVC 2015 bug
#else
    Optional() = default;
#endif

    Optional(const Optional &) = default;
    Optional& operator=(const Optional &) = default;

    Optional(Optional &&) = default;
    Optional& operator=(Optional &&) = default;

    ~Optional() = default;

    template <typename ...Args>
    explicit Optional(Args &&...args) : _value(true, T(std::forward<Args>(args)...)) {}

    explicit operator bool() const {
        return _value.first;
    }

    T& value() {
        return _value.second;
    }

    const T& value() const {
        return _value.second;
    }

    T* operator->() {
        return &(_value.second);
    }

    const T* operator->() const {
        return &(_value.second);
    }

    T& operator*() {
        return _value.second;
    }

    const T& operator*() const {
        return _value.second;
    }

private:
    std::pair<bool, T> _value;
};

}

}

#endif // end SEWENEW_REDISPLUSPLUS_CXX_UTILS_H
