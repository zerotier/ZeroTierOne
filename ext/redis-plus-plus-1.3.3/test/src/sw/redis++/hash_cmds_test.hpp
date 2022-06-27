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

#ifndef SEWENEW_REDISPLUSPLUS_TEST_HASH_CMDS_TEST_HPP
#define SEWENEW_REDISPLUSPLUS_TEST_HASH_CMDS_TEST_HPP

#include <unordered_map>
#include <map>
#include "utils.h"

namespace sw {

namespace redis {

namespace test {

template <typename RedisInstance>
void HashCmdTest<RedisInstance>::run() {
    _test_hash();

    _test_hash_batch();

    _test_numeric();

    _test_hscan();
}

template <typename RedisInstance>
void HashCmdTest<RedisInstance>::_test_hash() {
    auto key = test_key("hash");

    KeyDeleter<RedisInstance> deleter(_redis, key);

    auto f1 = std::string("f1");
    auto v1 = std::string("v1");
    auto f2 = std::string("f2");
    auto v2 = std::string("v2");
    auto f3 = std::string("f3");
    auto v3 = std::string("v3");

    REDIS_ASSERT(_redis.hset(key, f1, v1), "failed to test hset");
    REDIS_ASSERT(!_redis.hset(key, f1, v2), "failed to test hset with exist field");

    std::unordered_map<std::string, std::string> m = {{"ff1", "vv1"}, {"ff2", "vv2"}};
    REDIS_ASSERT(_redis.hset(key, m.begin(), m.end()) == 2, "failed to test hset");
    REDIS_ASSERT(_redis.hset(key, {std::make_pair("ff1", "vv1"),
                std::make_pair("ff2", "vv2")}) == 0, "failed to test hset");

    auto res = _redis.hget(key, f1);
    REDIS_ASSERT(res && *res == v2, "failed to test hget");

    REDIS_ASSERT(_redis.hsetnx(key, f2, v1), "failed to test hsetnx");
    REDIS_ASSERT(!_redis.hsetnx(key, f2, v2), "failed to test hsetnx with exist field");

    res = _redis.hget(key, f2);
    REDIS_ASSERT(res && *res == v1, "failed to test hget");

    REDIS_ASSERT(!_redis.hexists(key, f3), "failed to test hexists");
    REDIS_ASSERT(_redis.hset(key, std::make_pair(f3, v3)), "failed to test hset");
    REDIS_ASSERT(_redis.hexists(key, f3), "failed to test hexists");

    REDIS_ASSERT(_redis.hlen(key) == 5, "failed to test hlen");
    REDIS_ASSERT(_redis.hstrlen(key, f1) == static_cast<long long>(v1.size()),
            "failed to test hstrlen");

    REDIS_ASSERT(_redis.hdel(key, f1) == 1, "failed to test hdel");
    REDIS_ASSERT(_redis.hdel(key, {f1, f2, f3}) == 2, "failed to test hdel range");
}

template <typename RedisInstance>
void HashCmdTest<RedisInstance>::_test_hash_batch() {
    auto key = test_key("hash");

    KeyDeleter<RedisInstance> deleter(_redis, key);

    auto f1 = std::string("f1");
    auto v1 = std::string("v1");
    auto f2 = std::string("f2");
    auto v2 = std::string("v2");
    auto f3 = std::string("f3");

    _redis.hmset(key, {std::make_pair(f1, v1),
                        std::make_pair(f2, v2)});

    std::vector<std::string> fields;
    _redis.hkeys(key, std::back_inserter(fields));
    REDIS_ASSERT(fields.size() == 2, "failed to test hkeys");

    std::vector<std::string> vals;
    _redis.hvals(key, std::back_inserter(vals));
    REDIS_ASSERT(vals.size() == 2, "failed to test hvals");

    std::unordered_map<std::string, std::string> items;
    _redis.hgetall(key, std::inserter(items, items.end()));
    REDIS_ASSERT(items.size() == 2 && items[f1] == v1 && items[f2] == v2,
            "failed to test hgetall");

    std::vector<std::pair<std::string, std::string>> item_vec;
    _redis.hgetall(key, std::back_inserter(item_vec));
    REDIS_ASSERT(item_vec.size() == 2, "failed to test hgetall");

    std::vector<OptionalString> res;
    _redis.hmget(key, {f1, f2, f3}, std::back_inserter(res));
    REDIS_ASSERT(res.size() == 3
            && bool(res[0]) && *(res[0]) == v1
            && bool(res[1]) && *(res[1]) == v2
            && !res[2],
                "failed to test hmget");
}

template <typename RedisInstance>
void HashCmdTest<RedisInstance>::_test_numeric() {
    auto key = test_key("numeric");

    KeyDeleter<RedisInstance> deleter(_redis, key);

    auto field = "field";

    REDIS_ASSERT(_redis.hincrby(key, field, 1) == 1, "failed to test hincrby");
    REDIS_ASSERT(_redis.hincrby(key, field, -1) == 0, "failed to test hincrby");
    REDIS_ASSERT(_redis.hincrbyfloat(key, field, 1.5) == 1.5, "failed to test hincrbyfloat");
}

template <typename RedisInstance>
void HashCmdTest<RedisInstance>::_test_hscan() {
    auto key = test_key("hscan");

    KeyDeleter<RedisInstance> deleter(_redis, key);

    auto items = std::unordered_map<std::string, std::string>{
        std::make_pair("f1", "v1"),
        std::make_pair("f2", "v2"),
        std::make_pair("f3", "v3"),
    };

    _redis.hmset(key, items.begin(), items.end());

    std::unordered_map<std::string, std::string> item_map;
    auto cursor = 0;
    while (true) {
        cursor = _redis.hscan(key, cursor, "f*", 2, std::inserter(item_map, item_map.end()));
        if (cursor == 0) {
            break;
        }
    }

    REDIS_ASSERT(item_map == items, "failed to test hscan with pattern and count");

    std::map<std::string, std::string> item_vec;
    cursor = 0;
    while (true) {
        cursor = _redis.hscan(key, cursor, std::inserter(item_vec, item_vec.begin()));
        if (cursor == 0) {
            break;
        }
    }

    REDIS_ASSERT(item_vec.size() == items.size(), "failed to test hscan");
    for (const auto &ele : item_vec) {
        REDIS_ASSERT(items.find(ele.first) != items.end(), "failed to test hscan");
    }
}

}

}

}

#endif // end SEWENEW_REDISPLUSPLUS_TEST_HASH_CMDS_TEST_HPP
