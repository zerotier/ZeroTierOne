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

#include "command_options.h"
#include "errors.h"

namespace {

const std::string NEGATIVE_INFINITY_NUMERIC = "-inf";
const std::string POSITIVE_INFINITY_NUMERIC = "+inf";

const std::string NEGATIVE_INFINITY_STRING = "-";
const std::string POSITIVE_INFINITY_STRING = "+";

std::string unbound(const std::string &bnd);

std::string bound(const std::string &bnd);

}

namespace sw {

namespace redis {

const std::string& UnboundedInterval<double>::min() const {
    return NEGATIVE_INFINITY_NUMERIC;
}

const std::string& UnboundedInterval<double>::max() const {
    return POSITIVE_INFINITY_NUMERIC;
}

BoundedInterval<double>::BoundedInterval(double min, double max, BoundType type) :
                                            _min(std::to_string(min)),
                                            _max(std::to_string(max)) {
    switch (type) {
    case BoundType::CLOSED:
        // Do nothing
        break;

    case BoundType::OPEN:
        _min = unbound(_min);
        _max = unbound(_max);
        break;

    case BoundType::LEFT_OPEN:
        _min = unbound(_min);
        break;

    case BoundType::RIGHT_OPEN:
        _max = unbound(_max);
        break;

    default:
        throw Error("Unknow BoundType");
    }
}

LeftBoundedInterval<double>::LeftBoundedInterval(double min, BoundType type) :
                                                    _min(std::to_string(min)) {
    switch (type) {
    case BoundType::OPEN:
        _min = unbound(_min);
        break;

    case BoundType::RIGHT_OPEN:
        // Do nothing.
        break;

    default:
        throw Error("Bound type can only be OPEN or RIGHT_OPEN");
    }
}

const std::string& LeftBoundedInterval<double>::max() const {
    return POSITIVE_INFINITY_NUMERIC;
}

RightBoundedInterval<double>::RightBoundedInterval(double max, BoundType type) :
                                                    _max(std::to_string(max)) {
    switch (type) {
    case BoundType::OPEN:
        _max = unbound(_max);
        break;

    case BoundType::LEFT_OPEN:
        // Do nothing.
        break;

    default:
        throw Error("Bound type can only be OPEN or LEFT_OPEN");
    }
}

const std::string& RightBoundedInterval<double>::min() const {
    return NEGATIVE_INFINITY_NUMERIC;
}

const std::string& UnboundedInterval<std::string>::min() const {
    return NEGATIVE_INFINITY_STRING;
}

const std::string& UnboundedInterval<std::string>::max() const {
    return POSITIVE_INFINITY_STRING;
}

BoundedInterval<std::string>::BoundedInterval(const std::string &min,
                                                const std::string &max,
                                                BoundType type) {
    switch (type) {
    case BoundType::CLOSED:
        _min = bound(min);
        _max = bound(max);
        break;

    case BoundType::OPEN:
        _min = unbound(min);
        _max = unbound(max);
        break;

    case BoundType::LEFT_OPEN:
        _min = unbound(min);
        _max = bound(max);
        break;

    case BoundType::RIGHT_OPEN:
        _min = bound(min);
        _max = unbound(max);
        break;

    default:
        throw Error("Unknow BoundType");
    }
}

LeftBoundedInterval<std::string>::LeftBoundedInterval(const std::string &min, BoundType type) {
    switch (type) {
    case BoundType::OPEN:
        _min = unbound(min);
        break;

    case BoundType::RIGHT_OPEN:
        _min = bound(min);
        break;

    default:
        throw Error("Bound type can only be OPEN or RIGHT_OPEN");
    }
}

const std::string& LeftBoundedInterval<std::string>::max() const {
    return POSITIVE_INFINITY_STRING;
}

RightBoundedInterval<std::string>::RightBoundedInterval(const std::string &max, BoundType type) {
    switch (type) {
    case BoundType::OPEN:
        _max = unbound(max);
        break;

    case BoundType::LEFT_OPEN:
        _max = bound(max);
        break;

    default:
        throw Error("Bound type can only be OPEN or LEFT_OPEN");
    }
}

const std::string& RightBoundedInterval<std::string>::min() const {
    return NEGATIVE_INFINITY_STRING;
}

}

}

namespace {

std::string unbound(const std::string &bnd) {
    return "(" + bnd;
}

std::string bound(const std::string &bnd) {
    return "[" + bnd;
}

}
