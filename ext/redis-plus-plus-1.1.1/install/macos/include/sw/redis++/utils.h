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

#ifndef SEWENEW_REDISPLUSPLUS_UTILS_H
#define SEWENEW_REDISPLUSPLUS_UTILS_H

#include <cstring>
#include <string>
#include <type_traits>

namespace sw {

namespace redis {

// By now, not all compilers support std::string_view,
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
    Optional() = default;

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

using OptionalString = Optional<std::string>;

using OptionalLongLong = Optional<long long>;

using OptionalDouble = Optional<double>;

using OptionalStringPair = Optional<std::pair<std::string, std::string>>;

template <typename ...>
struct IsKvPair : std::false_type {};

template <typename T, typename U>
struct IsKvPair<std::pair<T, U>> : std::true_type {};

template <typename ...>
using Void = void;

template <typename T, typename U = Void<>>
struct IsInserter : std::false_type {};

template <typename T>
//struct IsInserter<T, Void<typename T::container_type>> : std::true_type {};
struct IsInserter<T,
    typename std::enable_if<!std::is_void<typename T::container_type>::value>::type>
        : std::true_type {};

template <typename Iter, typename T = Void<>>
struct IterType {
    using type = typename std::iterator_traits<Iter>::value_type;
};

template <typename Iter>
//struct IterType<Iter, Void<typename Iter::container_type>> {
struct IterType<Iter,
    //typename std::enable_if<std::is_void<typename Iter::value_type>::value>::type> {
    typename std::enable_if<IsInserter<Iter>::value>::type> {
    using type = typename std::decay<typename Iter::container_type::value_type>::type;
};

template <typename Iter, typename T = Void<>>
struct IsIter : std::false_type {};

template <typename Iter>
struct IsIter<Iter, typename std::enable_if<IsInserter<Iter>::value>::type> : std::true_type {};

template <typename Iter>
//struct IsIter<Iter, Void<typename std::iterator_traits<Iter>::iterator_category>>
struct IsIter<Iter,
    typename std::enable_if<!std::is_void<
        typename std::iterator_traits<Iter>::value_type>::value>::type>
            : std::integral_constant<bool, !std::is_convertible<Iter, StringView>::value> {};

template <typename T>
struct IsKvPairIter : IsKvPair<typename IterType<T>::type> {};

template <typename T, typename Tuple>
struct TupleWithType : std::false_type {};

template <typename T>
struct TupleWithType<T, std::tuple<>> : std::false_type {};

template <typename T, typename U, typename ...Args>
struct TupleWithType<T, std::tuple<U, Args...>> : TupleWithType<T, std::tuple<Args...>> {};

template <typename T, typename ...Args>
struct TupleWithType<T, std::tuple<T, Args...>> : std::true_type {};

template <std::size_t ...Is>
struct IndexSequence {};

template <std::size_t I, std::size_t ...Is>
struct MakeIndexSequence : MakeIndexSequence<I - 1, I - 1, Is...> {};

template <std::size_t ...Is>
struct MakeIndexSequence<0, Is...> : IndexSequence<Is...> {};

// NthType and NthValue are taken from
// https://stackoverflow.com/questions/14261183
template <std::size_t I, typename ...Args>
struct NthType {};

template <typename Arg, typename ...Args>
struct NthType<0, Arg, Args...> {
    using type = Arg;
};

template <std::size_t I, typename Arg, typename ...Args>
struct NthType<I, Arg, Args...> {
    using type = typename NthType<I - 1, Args...>::type;
};

template <typename ...Args>
struct LastType {
    using type = typename NthType<sizeof...(Args) - 1, Args...>::type;
};

struct InvalidLastType {};

template <>
struct LastType<> {
    using type = InvalidLastType;
};

template <std::size_t I, typename Arg, typename ...Args>
auto NthValue(Arg &&arg, Args &&...)
    -> typename std::enable_if<(I == 0), decltype(std::forward<Arg>(arg))>::type {
    return std::forward<Arg>(arg);
}

template <std::size_t I, typename Arg, typename ...Args>
auto NthValue(Arg &&, Args &&...args)
    -> typename std::enable_if<(I > 0),
            decltype(std::forward<typename NthType<I, Arg, Args...>::type>(
                    std::declval<typename NthType<I, Arg, Args...>::type>()))>::type {
    return std::forward<typename NthType<I, Arg, Args...>::type>(
            NthValue<I - 1>(std::forward<Args>(args)...));
}

template <typename ...Args>
auto LastValue(Args &&...args)
    -> decltype(std::forward<typename LastType<Args...>::type>(
            std::declval<typename LastType<Args...>::type>())) {
    return std::forward<typename LastType<Args...>::type>(
            NthValue<sizeof...(Args) - 1>(std::forward<Args>(args)...));
}

template <typename T, typename = Void<>>
struct HasPushBack : std::false_type {};

template <typename T>
struct HasPushBack<T,
    typename std::enable_if<
        std::is_void<decltype(
            std::declval<T>().push_back(std::declval<typename T::value_type>())
                )>::value>::type> : std::true_type {};

template <typename T, typename = Void<>>
struct HasInsert : std::false_type {};

template <typename T>
struct HasInsert<T,
    typename std::enable_if<
        std::is_same<
            decltype(std::declval<T>().insert(std::declval<typename T::const_iterator>(),
                                                std::declval<typename T::value_type>())),
            typename T::iterator>::value>::type> : std::true_type {};

template <typename T>
struct IsSequenceContainer
    : std::integral_constant<bool,
        HasPushBack<T>::value
            && !std::is_same<typename std::decay<T>::type, std::string>::value> {};

template <typename T>
struct IsAssociativeContainer
    : std::integral_constant<bool,
        HasInsert<T>::value && !HasPushBack<T>::value> {};

uint16_t crc16(const char *buf, int len);

}

}

#endif // end SEWENEW_REDISPLUSPLUS_UTILS_H
