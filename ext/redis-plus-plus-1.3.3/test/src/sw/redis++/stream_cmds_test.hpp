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

#ifndef SEWENEW_REDISPLUSPLUS_TEST_STREAM_CMDS_TEST_HPP
#define SEWENEW_REDISPLUSPLUS_TEST_STREAM_CMDS_TEST_HPP

#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <unordered_map>
#include "utils.h"

namespace sw {

namespace redis {

namespace test {

template <typename RedisInstance>
void StreamCmdsTest<RedisInstance>::run() {
    _test_stream_cmds();

    _test_group_cmds();
}

template <typename RedisInstance>
void StreamCmdsTest<RedisInstance>::_test_stream_cmds() {
    auto key = test_key("stream");

    KeyDeleter<RedisInstance> deleter(_redis, key);

    std::vector<std::pair<std::string, std::string>> attrs = {
        {"f1", "v1"},
        {"f2", "v2"}
    };
    const std::vector<std::string> ids = {"1565427842-0", "1565427842-1"};
    REDIS_ASSERT(_redis.xadd(key, ids.at(0), attrs.begin(), attrs.end()) == ids.at(0),
            "failed to test xadd");

    std::vector<std::pair<std::string, std::string>> keys = {std::make_pair(key, "0-0")};
    Result result;
    _redis.xread(keys.begin(), keys.end(), 1, std::inserter(result, result.end()));

    REDIS_ASSERT(result.size() == 1
            && result.find(key) != result.end()
            && result[key].size() == 1
            && result[key].at(0).first == ids.at(0)
            && result[key].at(0).second.size() == 2,
            "failed to test xread");

    result.clear();
    _redis.xread(key, std::string("0-0"), 1, std::inserter(result, result.end()));

    REDIS_ASSERT(result.size() == 1
            && result.find(key) != result.end()
            && result[key].size() == 1
            && result[key].at(0).first == ids.at(0)
            && result[key].at(0).second.size() == 2,
            "failed to test xread");

    result.clear();
    keys = {std::make_pair(key, ids.at(0))};
    _redis.xread(keys.begin(),
                    keys.end(),
                    std::chrono::seconds(1),
                    2,
                    std::inserter(result, result.end()));
    REDIS_ASSERT(result.size() == 0, "failed to test xread");

    _redis.xread(key,
                    ids.at(0),
                    std::chrono::seconds(1),
                    2,
                    std::inserter(result, result.end()));
    REDIS_ASSERT(result.size() == 0, "failed to test xread");

    REDIS_ASSERT(_redis.xadd(key, ids.at(1),
                             attrs.begin(),
                             attrs.end()) == ids.at(1),
                 "failed to test xadd");

    REDIS_ASSERT(_redis.xlen(key) == 2, "failed to test xlen");

    std::vector<Item> items;
    _redis.xrange(key, "-", "+", 2, std::back_inserter(items));
    REDIS_ASSERT(items.size() == 2 &&
                 items.at(0).first == ids.at(0) &&
                 items.at(1).first == ids.at(1),
                 "failed to test xrange with count");

    items.clear();
    _redis.xrange(key, "-", "+", 1, std::back_inserter(items));
    REDIS_ASSERT(items.size() == 1 &&
                 items.at(0).first == ids.at(0),
                 "failed to test xrange with count");

    items.clear();
    _redis.xrevrange(key, "+", "-", 2, std::back_inserter(items));
    REDIS_ASSERT(items.size() == 2 &&
                 items.at(0).first == ids.at(1) &&
                 items.at(1).first == ids.at(0),
                 "failed to test xrevrange with count");

    items.clear();
    _redis.xrevrange(key, "+", "-", 1, std::back_inserter(items));
    REDIS_ASSERT(items.size() == 1 &&
                 items.at(0).first == ids.at(1),
                 "failed to test xrevrange with count");

    REDIS_ASSERT(_redis.xtrim(key, 1, false) == 1, "failed to test xtrim");

    items.clear();
    _redis.xrange(key, "-", "+", std::back_inserter(items));
    REDIS_ASSERT(items.size() == 1 && items[0].first == ids.at(1),
                 "failed to test xrange");

    items.clear();
    _redis.xrevrange(key, "+", "-", std::back_inserter(items));
    REDIS_ASSERT(items.size() == 1 && items[0].first == ids.at(1),
                 "failed to test xrevrange");

    REDIS_ASSERT(_redis.xdel(key, {ids.at(1), std::string("111-111")}) == 1,
                 "failed to test xdel");
}

template <typename RedisInstance>
void StreamCmdsTest<RedisInstance>::_test_group_cmds() {
    auto key = test_key("stream");

    KeyDeleter<RedisInstance> deleter(_redis, key);

    auto group = "group";
    auto consumer1 = "consumer1";

    _redis.xgroup_create(key, group, "$", true);

    std::vector<std::pair<std::string, std::string>> attrs = {
        {"f1", "v1"},
        {"f2", "v2"}
    };
    auto id = _redis.xadd(key, "*", attrs.begin(), attrs.end());
    auto keys = {std::make_pair(key, ">")};

    Result result;
    _redis.xreadgroup(group,
            consumer1,
            keys.begin(),
            keys.end(),
            1,
            std::inserter(result, result.end()));
    REDIS_ASSERT(result.size() == 1
            && result.find(key) != result.end()
            && result[key].size() == 1
            && result[key][0].first == id,
            "failed to test xreadgroup");

    result.clear();
    _redis.xreadgroup(group,
            consumer1,
            key,
            std::string(">"),
            1,
            std::inserter(result, result.end()));
    REDIS_ASSERT(result.size() == 0, "failed to test xreadgroup");

    result.clear();
    _redis.xreadgroup(group,
            "not-exist-consumer",
            keys.begin(),
            keys.end(),
            1,
            std::inserter(result, result.end()));
    REDIS_ASSERT(result.size() == 0, "failed to test xreadgroup");

    result.clear();
    _redis.xreadgroup(group,
            consumer1,
            keys.begin(),
            keys.end(),
            std::chrono::seconds(1),
            1,
            std::inserter(result, result.end()));
    REDIS_ASSERT(result.size() == 0, "failed to test xreadgroup");

    result.clear();
    _redis.xreadgroup(group,
            consumer1,
            key,
            ">",
            std::chrono::seconds(1),
            1,
            std::inserter(result, result.end()));
    REDIS_ASSERT(result.size() == 0, "failed to test xreadgroup");

    using PendingResult = std::vector<std::tuple<std::string, std::string, long long, long long>>;
    PendingResult pending_result;
    _redis.xpending(key, group, "-", "+", 1, consumer1, std::back_inserter(pending_result));

    REDIS_ASSERT(pending_result.size() == 1
            && std::get<0>(pending_result[0]) == id
            && std::get<1>(pending_result[0]) == consumer1,
            "failed to test xpending");

    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto consumer2 = "consumer2";
    std::vector<Item> items;
    auto ids = {id};
    _redis.xclaim(key,
            group,
            consumer2,
            std::chrono::milliseconds(10),
            ids,
            std::back_inserter(items));
    REDIS_ASSERT(items.size() == 1 && items[0].first == id, "failed to test xclaim");

    std::this_thread::sleep_for(std::chrono::seconds(1));

    items.clear();
    _redis.xclaim(key, group, consumer1, std::chrono::milliseconds(10), id, std::back_inserter(items));
    REDIS_ASSERT(items.size() == 1 && items[0].first == id, "failed to test xclaim: " + std::to_string(items.size()));

    _redis.xack(key, group, id);

    REDIS_ASSERT(_redis.xgroup_delconsumer(key, group, consumer1) == 0,
            "failed to test xgroup_delconsumer");

    REDIS_ASSERT(_redis.xgroup_delconsumer(key, group, consumer2) == 0,
            "failed to test xgroup_delconsumer");

    REDIS_ASSERT(_redis.xgroup_destroy(key, group) == 1,
            "failed to test xgroup_destroy");
}

}

}

}

#endif // end SEWENEW_REDISPLUSPLUS_TEST_STREAM_CMDS_TEST_HPP
