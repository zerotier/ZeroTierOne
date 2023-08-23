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

#ifndef SEWENEW_REDISPLUSPLUS_COMMAND_OPTIONS_H
#define SEWENEW_REDISPLUSPLUS_COMMAND_OPTIONS_H

#include <string>
#include "utils.h"

namespace sw {

namespace redis {

enum class UpdateType {
    EXIST,
    NOT_EXIST,
    ALWAYS
};

enum class InsertPosition {
    BEFORE,
    AFTER
};

enum class BoundType {
    CLOSED,
    OPEN,
    LEFT_OPEN,
    RIGHT_OPEN
};

// (-inf, +inf)
template <typename T>
class UnboundedInterval;

// [min, max], (min, max), (min, max], [min, max)
template <typename T>
class BoundedInterval;

// [min, +inf), (min, +inf)
template <typename T>
class LeftBoundedInterval;

// (-inf, max], (-inf, max)
template <typename T>
class RightBoundedInterval;

template <>
class UnboundedInterval<double> {
public:
    const std::string& min() const;

    const std::string& max() const;
};

template <>
class BoundedInterval<double> {
public:
    BoundedInterval(double min, double max, BoundType type);

    const std::string& min() const {
        return _min;
    }

    const std::string& max() const {
        return _max;
    }

private:
    std::string _min;
    std::string _max;
};

template <>
class LeftBoundedInterval<double> {
public:
    LeftBoundedInterval(double min, BoundType type);

    const std::string& min() const {
        return _min;
    }

    const std::string& max() const;

private:
    std::string _min;
};

template <>
class RightBoundedInterval<double> {
public:
    RightBoundedInterval(double max, BoundType type);

    const std::string& min() const;

    const std::string& max() const {
        return _max;
    }

private:
    std::string _max;
};

template <>
class UnboundedInterval<std::string> {
public:
    const std::string& min() const;

    const std::string& max() const;
};

template <>
class BoundedInterval<std::string> {
public:
    BoundedInterval(const std::string &min, const std::string &max, BoundType type);

    const std::string& min() const {
        return _min;
    }

    const std::string& max() const {
        return _max;
    }

private:
    std::string _min;
    std::string _max;
};

template <>
class LeftBoundedInterval<std::string> {
public:
    LeftBoundedInterval(const std::string &min, BoundType type);

    const std::string& min() const {
        return _min;
    }

    const std::string& max() const;

private:
    std::string _min;
};

template <>
class RightBoundedInterval<std::string> {
public:
    RightBoundedInterval(const std::string &max, BoundType type);

    const std::string& min() const;

    const std::string& max() const {
        return _max;
    }

private:
    std::string _max;
};

struct LimitOptions {
    long long offset = 0;
    long long count = -1;
};

enum class Aggregation {
    SUM,
    MIN,
    MAX
};

enum class BitOp {
    AND,
    OR,
    XOR,
    NOT
};

enum class GeoUnit {
    M,
    KM,
    MI,
    FT
};

template <typename T>
struct WithCoord : TupleWithType<std::pair<double, double>, T> {};

template <typename T>
struct WithDist : TupleWithType<double, T> {};

template <typename T>
struct WithHash : TupleWithType<long long, T> {};

}

}

#endif // end SEWENEW_REDISPLUSPLUS_COMMAND_OPTIONS_H
