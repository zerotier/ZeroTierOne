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

#ifndef SEWENEW_REDISPLUSPLUS_SHARDS_H
#define SEWENEW_REDISPLUSPLUS_SHARDS_H

#include <string>
#include <map>
#include "errors.h"

namespace sw {

namespace redis {

using Slot = std::size_t;

struct SlotRange {
    Slot min;
    Slot max;
};

inline bool operator<(const SlotRange &lhs, const SlotRange &rhs) {
    return lhs.max < rhs.max;
}

struct Node {
    std::string host;
    int port;
};

inline bool operator==(const Node &lhs, const Node &rhs) {
    return lhs.host == rhs.host && lhs.port == rhs.port;
}

struct NodeHash {
    std::size_t operator()(const Node &node) const noexcept {
        auto host_hash = std::hash<std::string>{}(node.host);
        auto port_hash = std::hash<int>{}(node.port);
        return host_hash ^ (port_hash << 1);
    }
};

using Shards = std::map<SlotRange, Node>;

class RedirectionError : public ReplyError {
public:
    RedirectionError(const std::string &msg);

    RedirectionError(const RedirectionError &) = default;
    RedirectionError& operator=(const RedirectionError &) = default;

    RedirectionError(RedirectionError &&) = default;
    RedirectionError& operator=(RedirectionError &&) = default;

    virtual ~RedirectionError() = default;

    Slot slot() const {
        return _slot;
    }

    const Node& node() const {
        return _node;
    }

private:
    std::pair<Slot, Node> _parse_error(const std::string &msg) const;

    Slot _slot = 0;
    Node _node;
};

class MovedError : public RedirectionError {
public:
    explicit MovedError(const std::string &msg) : RedirectionError(msg) {}

    MovedError(const MovedError &) = default;
    MovedError& operator=(const MovedError &) = default;

    MovedError(MovedError &&) = default;
    MovedError& operator=(MovedError &&) = default;

    virtual ~MovedError() = default;
};

class AskError : public RedirectionError {
public:
    explicit AskError(const std::string &msg) : RedirectionError(msg) {}

    AskError(const AskError &) = default;
    AskError& operator=(const AskError &) = default;

    AskError(AskError &&) = default;
    AskError& operator=(AskError &&) = default;

    virtual ~AskError() = default;
};

}

}

#endif // end SEWENEW_REDISPLUSPLUS_SHARDS_H
