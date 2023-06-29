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

#ifndef SEWENEW_REDISPLUSPLUS_COMMAND_ARGS_H
#define SEWENEW_REDISPLUSPLUS_COMMAND_ARGS_H

#include <vector>
#include <list>
#include <string>
#include <tuple>
#include "utils.h"

namespace sw {

namespace redis {

class CmdArgs {
public:
    template <typename Arg>
    CmdArgs& append(Arg &&arg);

    template <typename Arg, typename ...Args>
    CmdArgs& append(Arg &&arg, Args &&...args);

    // All overloads of operator<< are for internal use only.
    CmdArgs& operator<<(const StringView &arg);

    template <typename T,
                 typename std::enable_if<std::is_arithmetic<typename std::decay<T>::type>::value,
                                        int>::type = 0>
    CmdArgs& operator<<(T &&arg);

    template <typename Iter>
    CmdArgs& operator<<(const std::pair<Iter, Iter> &range);

    template <std::size_t N, typename ...Args>
    auto operator<<(const std::tuple<Args...> &) ->
        typename std::enable_if<N == sizeof...(Args), CmdArgs&>::type {
        return *this;
    }

    template <std::size_t N = 0, typename ...Args>
    auto operator<<(const std::tuple<Args...> &arg) ->
        typename std::enable_if<N < sizeof...(Args), CmdArgs&>::type;

    const char** argv() {
        return _argv.data();
    }

    const std::size_t* argv_len() {
        return _argv_len.data();
    }

    std::size_t size() const {
        return _argv.size();
    }

private:
    // Deep copy.
    CmdArgs& _append(std::string arg);

    // Shallow copy.
    CmdArgs& _append(const StringView &arg);

    // Shallow copy.
    CmdArgs& _append(const char *arg);

    template <typename T,
                 typename std::enable_if<std::is_arithmetic<typename std::decay<T>::type>::value,
                                        int>::type = 0>
    CmdArgs& _append(T &&arg) {
        return operator<<(std::forward<T>(arg));
    }

    template <typename Iter>
    CmdArgs& _append(std::true_type, const std::pair<Iter, Iter> &range);

    template <typename Iter>
    CmdArgs& _append(std::false_type, const std::pair<Iter, Iter> &range);

    std::vector<const char *> _argv;
    std::vector<std::size_t> _argv_len;

    std::list<std::string> _args;
};

template <typename Arg>
inline CmdArgs& CmdArgs::append(Arg &&arg) {
    return _append(std::forward<Arg>(arg));
}

template <typename Arg, typename ...Args>
inline CmdArgs& CmdArgs::append(Arg &&arg, Args &&...args) {
    _append(std::forward<Arg>(arg));

    return append(std::forward<Args>(args)...);
}

inline CmdArgs& CmdArgs::operator<<(const StringView &arg) {
    _argv.push_back(arg.data());
    _argv_len.push_back(arg.size());

    return *this;
}

template <typename Iter>
inline CmdArgs& CmdArgs::operator<<(const std::pair<Iter, Iter> &range) {
    return _append(IsKvPair<typename std::decay<decltype(*std::declval<Iter>())>::type>(), range);
}

template <typename T,
             typename std::enable_if<std::is_arithmetic<typename std::decay<T>::type>::value,
                                    int>::type>
inline CmdArgs& CmdArgs::operator<<(T &&arg) {
    return _append(std::to_string(std::forward<T>(arg)));
}

template <std::size_t N, typename ...Args>
auto CmdArgs::operator<<(const std::tuple<Args...> &arg) ->
    typename std::enable_if<N < sizeof...(Args), CmdArgs&>::type {
    operator<<(std::get<N>(arg));

    return operator<<<N + 1, Args...>(arg);
}

inline CmdArgs& CmdArgs::_append(std::string arg) {
    _args.push_back(std::move(arg));
    return operator<<(_args.back());
}

inline CmdArgs& CmdArgs::_append(const StringView &arg) {
    return operator<<(arg);
}

inline CmdArgs& CmdArgs::_append(const char *arg) {
    return operator<<(arg);
}

template <typename Iter>
CmdArgs& CmdArgs::_append(std::false_type, const std::pair<Iter, Iter> &range) {
    auto first = range.first;
    auto last = range.second;
    while (first != last) {
        *this << *first;
        ++first;
    }

    return *this;
}

template <typename Iter>
CmdArgs& CmdArgs::_append(std::true_type, const std::pair<Iter, Iter> &range) {
    auto first = range.first;
    auto last = range.second;
    while (first != last) {
        *this << first->first << first->second;
        ++first;
    }

    return *this;
}

}

}

#endif // end SEWENEW_REDISPLUSPLUS_COMMAND_ARGS_H
