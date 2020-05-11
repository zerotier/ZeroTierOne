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

#ifndef SEWENEW_REDISPLUSPLUS_TEST_LIST_CMDS_TEST_HPP
#define SEWENEW_REDISPLUSPLUS_TEST_LIST_CMDS_TEST_HPP

#include "utils.h"

namespace sw {

namespace redis {

namespace test {

template <typename RedisInstance>
void ListCmdTest<RedisInstance>::run() {
    _test_lpoppush();

    _test_rpoppush();

    _test_list();

    _test_blocking();
}

template <typename RedisInstance>
void ListCmdTest<RedisInstance>::_test_lpoppush() {
    auto key = test_key("lpoppush");

    KeyDeleter<RedisInstance> deleter(_redis, key);

    auto item = _redis.lpop(key);
    REDIS_ASSERT(!item, "failed to test lpop");

    REDIS_ASSERT(_redis.lpushx(key, "1") == 0, "failed to test lpushx");
    REDIS_ASSERT(_redis.lpush(key, "1") == 1, "failed to test lpush");
    REDIS_ASSERT(_redis.lpushx(key, "2") == 2, "failed to test lpushx");
    REDIS_ASSERT(_redis.lpush(key, {"3", "4", "5"}) == 5, "failed to test lpush");

    item = _redis.lpop(key);
    REDIS_ASSERT(item && *item == "5", "failed to test lpop");
}

template <typename RedisInstance>
void ListCmdTest<RedisInstance>::_test_rpoppush() {
    auto key = test_key("rpoppush");

    KeyDeleter<RedisInstance> deleter(_redis, key);

    auto item = _redis.rpop(key);
    REDIS_ASSERT(!item, "failed to test rpop");

    REDIS_ASSERT(_redis.rpushx(key, "1") == 0, "failed to test rpushx");
    REDIS_ASSERT(_redis.rpush(key, "1") == 1, "failed to test rpush");
    REDIS_ASSERT(_redis.rpushx(key, "2") == 2, "failed to test rpushx");
    REDIS_ASSERT(_redis.rpush(key, {"3", "4", "5"}) == 5, "failed to test rpush");

    item = _redis.rpop(key);
    REDIS_ASSERT(item && *item == "5", "failed to test rpop");
}

template <typename RedisInstance>
void ListCmdTest<RedisInstance>::_test_list() {
    auto key = test_key("list");

    KeyDeleter<RedisInstance> deleter(_redis, key);

    auto item = _redis.lindex(key, 0);
    REDIS_ASSERT(!item, "failed to test lindex");

    _redis.lpush(key, {"1", "2", "3", "4", "5"});

    REDIS_ASSERT(_redis.lrem(key, 0, "3") == 1, "failed to test lrem");

    REDIS_ASSERT(_redis.linsert(key, InsertPosition::BEFORE, "2", "3") == 5,
            "failed to test lindex");

    REDIS_ASSERT(_redis.llen(key) == 5, "failed to test llen");

    _redis.lset(key, 0, "6");
    item = _redis.lindex(key, 0);
    REDIS_ASSERT(item && *item == "6", "failed to test lindex");

    _redis.ltrim(key, 0, 2);

    std::vector<std::string> res;
    _redis.lrange(key, 0, -1, std::back_inserter(res));
    REDIS_ASSERT(res == std::vector<std::string>({"6", "4", "3"}), "failed to test ltrim");
}

template <typename RedisInstance>
void ListCmdTest<RedisInstance>::_test_blocking() {
    auto k1 = test_key("k1");
    auto k2 = test_key("k2");
    auto k3 = test_key("k3");

    auto keys = {k1, k2, k3};

    KeyDeleter<RedisInstance> deleter(_redis, keys);

    std::string val("value");
    _redis.lpush(k1, val);

    auto res = _redis.blpop(keys.begin(), keys.end());
    REDIS_ASSERT(res && *res == std::make_pair(k1, val), "failed to test blpop");

    res = _redis.brpop(keys, std::chrono::seconds(1));
    REDIS_ASSERT(!res, "failed to test brpop with timeout");

    _redis.lpush(k1, val);
    res = _redis.blpop(k1);
    REDIS_ASSERT(res && *res == std::make_pair(k1, val), "failed to test blpop");

    res = _redis.blpop(k1, std::chrono::seconds(1));
    REDIS_ASSERT(!res, "failed to test blpop with timeout");

    _redis.lpush(k1, val);
    res = _redis.brpop(k1);
    REDIS_ASSERT(res && *res == std::make_pair(k1, val), "failed to test brpop");

    res = _redis.brpop(k1, std::chrono::seconds(1));
    REDIS_ASSERT(!res, "failed to test brpop with timeout");

    auto str = _redis.brpoplpush(k2, k3, std::chrono::seconds(1));
    REDIS_ASSERT(!str, "failed to test brpoplpush with timeout");

    _redis.lpush(k2, val);
    str = _redis.brpoplpush(k2, k3);
    REDIS_ASSERT(str && *str == val, "failed to test brpoplpush");

    str = _redis.rpoplpush(k3, k2);
    REDIS_ASSERT(str && *str == val, "failed to test rpoplpush");
}

}

}

}

#endif // end SEWENEW_REDISPLUSPLUS_TEST_LIST_CMDS_TEST_HPP
