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

#ifndef SEWENEW_REDISPLUSPLUS_TEST_SET_CMDS_TEST_HPP
#define SEWENEW_REDISPLUSPLUS_TEST_SET_CMDS_TEST_HPP

#include <unordered_set>
#include <vector>
#include "utils.h"

namespace sw {

namespace redis {

namespace test {

template <typename RedisInstance>
void SetCmdTest<RedisInstance>::run() {
    _test_set();

    _test_multi_set();

    _test_sscan();
}

template <typename RedisInstance>
void SetCmdTest<RedisInstance>::_test_set() {
    auto key = test_key("set");

    KeyDeleter<RedisInstance> deleter(_redis, key);

    std::string m1("m1");
    std::string m2("m2");
    std::string m3("m3");

    REDIS_ASSERT(_redis.sadd(key, m1) == 1, "failed to test sadd");

    auto members = {m1, m2, m3};
    REDIS_ASSERT(_redis.sadd(key, members) == 2, "failed to test sadd with multiple members");

    REDIS_ASSERT(_redis.scard(key) == 3, "failed to test scard");

    REDIS_ASSERT(_redis.sismember(key, m1), "failed to test sismember");

    std::unordered_set<std::string> res;
    _redis.smembers(key, std::inserter(res, res.end()));
    REDIS_ASSERT(res.find(m1) != res.end()
            && res.find(m2) != res.end()
            && res.find(m3) != res.end(),
                "failed to test smembers");

    auto ele = _redis.srandmember(key);
    REDIS_ASSERT(bool(ele) && res.find(*ele) != res.end(), "failed to test srandmember");

    std::vector<std::string> rand_members;
    _redis.srandmember(key, 2, std::back_inserter(rand_members));
    REDIS_ASSERT(rand_members.size() == 2, "failed to test srandmember");

    ele = _redis.spop(key);
    REDIS_ASSERT(bool(ele) && res.find(*ele) != res.end(), "failed to test spop");

    rand_members.clear();
    _redis.spop(key, 3, std::back_inserter(rand_members));
    REDIS_ASSERT(rand_members.size() == 2, "failed to test srandmember");

    rand_members.clear();
    _redis.srandmember(key, 2, std::back_inserter(rand_members));
    REDIS_ASSERT(rand_members.empty(), "failed to test srandmember with empty set");

    _redis.spop(key, 2, std::back_inserter(rand_members));
    REDIS_ASSERT(rand_members.empty(), "failed to test spop with empty set");

    _redis.sadd(key, members);
    REDIS_ASSERT(_redis.srem(key, m1) == 1, "failed to test srem");
    REDIS_ASSERT(_redis.srem(key, members) == 2, "failed to test srem with mulitple members");
    REDIS_ASSERT(_redis.srem(key, members) == 0, "failed to test srem with mulitple members");
}

template <typename RedisInstance>
void SetCmdTest<RedisInstance>::_test_multi_set() {
    auto k1 = test_key("s1");
    auto k2 = test_key("s2");
    auto k3 = test_key("s3");
    auto k4 = test_key("s4");
    auto k5 = test_key("s5");
    auto k6 = test_key("s6");

    KeyDeleter<RedisInstance> keys(_redis, {k1, k2, k3, k4, k5, k6});

    _redis.sadd(k1, {"a", "c"});
    _redis.sadd(k2, {"a", "b"});
    std::vector<std::string> sdiff;
    _redis.sdiff({k1, k1}, std::back_inserter(sdiff));
    REDIS_ASSERT(sdiff.empty(), "failed to test sdiff");

    _redis.sdiff({k1, k2}, std::back_inserter(sdiff));
    REDIS_ASSERT(sdiff == std::vector<std::string>({"c"}), "failed to test sdiff");

    _redis.sdiffstore(k3, {k1, k2});
    sdiff.clear();
    _redis.smembers(k3, std::back_inserter(sdiff));
    REDIS_ASSERT(sdiff == std::vector<std::string>({"c"}), "failed to test sdiffstore");

    REDIS_ASSERT(_redis.sdiffstore(k3, k1) == 2, "failed to test sdiffstore");

    REDIS_ASSERT(_redis.sinterstore(k3, k1) == 2, "failed to test sinterstore");

    REDIS_ASSERT(_redis.sunionstore(k3, k1) == 2, "failed to test sunionstore");

    std::vector<std::string> sinter;
    _redis.sinter({k1, k2}, std::back_inserter(sinter));
    REDIS_ASSERT(sinter == std::vector<std::string>({"a"}), "failed to test sinter");

    _redis.sinterstore(k4, {k1, k2});
    sinter.clear();
    _redis.smembers(k4, std::back_inserter(sinter));
    REDIS_ASSERT(sinter == std::vector<std::string>({"a"}), "failed to test sinterstore");

    std::unordered_set<std::string> sunion;
    _redis.sunion({k1, k2}, std::inserter(sunion, sunion.end()));
    REDIS_ASSERT(sunion == std::unordered_set<std::string>({"a", "b", "c"}),
            "failed to test sunion");

    _redis.sunionstore(k5, {k1, k2});
    sunion.clear();
    _redis.smembers(k5, std::inserter(sunion, sunion.end()));
    REDIS_ASSERT(sunion == std::unordered_set<std::string>({"a", "b", "c"}),
            "failed to test sunionstore");

    REDIS_ASSERT(_redis.smove(k5, k6, "a"), "failed to test smove");
}

template <typename RedisInstance>
void SetCmdTest<RedisInstance>::_test_sscan() {
    auto key = test_key("sscan");

    KeyDeleter<RedisInstance> deleter(_redis, key);

    std::unordered_set<std::string> members = {"m1", "m2", "m3"};
    _redis.sadd(key, members.begin(), members.end());

    std::unordered_set<std::string> res;
    long long cursor = 0;
    while (true) {
        cursor = _redis.sscan(key, cursor, "m*", 1, std::inserter(res, res.end()));
        if (cursor == 0) {
            break;
        }
    }

    REDIS_ASSERT(res == members, "failed to test sscan");

    res.clear();
    cursor = 0;
    while (true) {
        cursor = _redis.sscan(key, cursor, std::inserter(res, res.end()));
        if (cursor == 0) {
            break;
        }
    }

    REDIS_ASSERT(res == members, "failed to test sscan");
}

}

}

}

#endif // end SEWENEW_REDISPLUSPLUS_TEST_SET_CMDS_TEST_HPP
