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

#ifndef SEWENEW_REDISPLUSPLUS_TEST_ZSET_CMDS_TEST_HPP
#define SEWENEW_REDISPLUSPLUS_TEST_ZSET_CMDS_TEST_HPP

#include <map>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include "utils.h"

namespace sw {

namespace redis {

namespace test {

template <typename RedisInstance>
void ZSetCmdTest<RedisInstance>::run() {
    _test_zset();

    _test_zscan();

    _test_range();

    _test_lex();

    _test_multi_zset();

    _test_zpop();

    _test_bzpop();
}

template <typename RedisInstance>
void ZSetCmdTest<RedisInstance>::_test_zset() {
    auto key = test_key("zset");

    KeyDeleter<RedisInstance> deleter(_redis, key);

    std::map<std::string, double> s = {
        std::make_pair("m1", 1.2),
        std::make_pair("m2", 2),
        std::make_pair("m3", 3),
    };

    const auto &ele = *(s.begin());
    REDIS_ASSERT(_redis.zadd(key, ele.first, ele.second, UpdateType::EXIST) == 0,
            "failed to test zadd with noexistent member");

    REDIS_ASSERT(_redis.zadd(key, s.begin(), s.end()) == 3, "failed to test zadd");

    REDIS_ASSERT(_redis.zadd(key, ele.first, ele.second, UpdateType::NOT_EXIST) == 0,
            "failed to test zadd with exist member");

    REDIS_ASSERT(_redis.zadd(key, s.begin(), s.end(), UpdateType::ALWAYS, true) == 0,
            "failed to test zadd");

    REDIS_ASSERT(_redis.zcard(key) == 3, "failed to test zcard");

    auto rank = _redis.zrank(key, "m2");
    REDIS_ASSERT(bool(rank) && *rank == 1, "failed to test zrank");
    rank = _redis.zrevrank(key, "m4");
    REDIS_ASSERT(!rank, "failed to test zrevrank with nonexistent member");

    auto score = _redis.zscore(key, "m4");
    REDIS_ASSERT(!score, "failed to test zscore with nonexistent member");

    REDIS_ASSERT(_redis.zincrby(key, 1, "m3") == 4, "failed to test zincrby");

    score = _redis.zscore(key, "m3");
    REDIS_ASSERT(score && *score == 4, "failed to test zscore");

    REDIS_ASSERT(_redis.zrem(key, "m1") == 1, "failed to test zrem");
    REDIS_ASSERT(_redis.zrem(key, {"m1", "m2", "m3", "m4"}) == 2, "failed to test zrem");
}

template <typename RedisInstance>
void ZSetCmdTest<RedisInstance>::_test_zscan() {
    auto key = test_key("zscan");

    KeyDeleter<RedisInstance> deleter(_redis, key);

    std::map<std::string, double> s = {
        std::make_pair("m1", 1.2),
        std::make_pair("m2", 2),
        std::make_pair("m3", 3),
    };
    _redis.zadd(key, s.begin(), s.end());

    std::map<std::string, double> res;
    auto cursor = 0;
    while (true) {
        cursor = _redis.zscan(key, cursor, "m*", 2, std::inserter(res, res.end()));
        if (cursor == 0) {
            break;
        }
    }
    REDIS_ASSERT(res == s, "failed to test zscan");
}

template <typename RedisInstance>
void ZSetCmdTest<RedisInstance>::_test_range() {
    auto key = test_key("range");

    KeyDeleter<RedisInstance> deleter(_redis, key);

    std::vector<std::pair<std::string, double>> s = {
        std::make_pair("m1", 1),
        std::make_pair("m2", 2),
        std::make_pair("m3", 3),
        std::make_pair("m4", 4),
    };
    std::vector<std::string> sKeys;
    for (const auto &p : s) {
        sKeys.push_back(p.first);
    }
    std::vector<std::string> sReversedKeys = sKeys;
    std::reverse(sReversedKeys.begin(), sReversedKeys.end());

    _redis.zadd(key, s.begin(), s.end());

    REDIS_ASSERT(_redis.zcount(key, UnboundedInterval<double>{}) == static_cast<long long int>(s.size()),
                 "failed to test zcount");

    std::vector<std::string> members;
    _redis.zrange(key, 0, -1, std::back_inserter(members));
    REDIS_ASSERT(members.size() == s.size() && members == sKeys,
                 "failed to test zrange");

    std::vector<std::pair<std::string, double>> res;
    _redis.zrange(key, 0, -1, std::inserter(res, res.end()));
    REDIS_ASSERT(s == res, "failed to test zrange with score");

    members.clear();
    _redis.zrevrange(key, 0, 0, std::back_inserter(members));
    REDIS_ASSERT(members.size() == 1 && members.at(0) == sKeys.at(s.size()-1),
                 "failed to test zrevrange");

    res.clear();
    _redis.zrevrange(key, 0, 0, std::inserter(res, res.end()));
    REDIS_ASSERT(res.size() == 1 && res.at(0) == s.at(s.size()-1),
                 "failed to test zrevrange with score");

    members.clear();
    _redis.zrangebyscore(key, UnboundedInterval<double>{}, std::back_inserter(members));
    REDIS_ASSERT(members.size() == s.size() && members == sKeys,
                 "failed to test zrangebyscore");

    LimitOptions limitOpts;
    limitOpts.offset = 0;
    limitOpts.count = 2;
    members.clear();
    _redis.zrangebyscore(key, UnboundedInterval<double>{}, limitOpts, std::back_inserter(members));
    REDIS_ASSERT(members.size() == 2 &&
                 members.at(0) == sKeys.at(0) &&
                 members.at(1) == sKeys.at(1),
                 "failed to test zrangebyscore with limits 0, 1");

    limitOpts.offset = 1;
    members.clear();
    _redis.zrangebyscore(key, UnboundedInterval<double>{}, limitOpts, std::back_inserter(members));
    REDIS_ASSERT(members.size() == 2 &&
                 members.at(0) == sKeys.at(1) &&
                 members.at(1) == sKeys.at(2),
                 "failed to test zrangebyscore with limits 1, 2");

    limitOpts.offset = s.size() - 1;
    members.clear();
    _redis.zrangebyscore(key, UnboundedInterval<double>{}, limitOpts, std::back_inserter(members));
    REDIS_ASSERT(members.size() == 1 &&
                 members.at(0) == sKeys.at(sKeys.size() - 1),
                 "failed to test zrangebyscore with limits size-1, 2");

    members.clear();
    _redis.zrangebyscore(key,
            BoundedInterval<double>(1, 2, BoundType::RIGHT_OPEN),
            std::back_inserter(members));
    REDIS_ASSERT(members.size() == 1 && members.at(0) == sKeys.at(0),
                 "failed to test zrangebyscore");

    res.clear();
    _redis.zrangebyscore(key,
            LeftBoundedInterval<double>(2, BoundType::OPEN),
            std::inserter(res, res.end()));
    REDIS_ASSERT(res.size() == 2 && res.at(0) == s.at(s.size()-2) && res.at(1) == s.at(s.size()-1),
                 "failed to test zrangebyscore");

    members.clear();
    _redis.zrevrangebyscore(key,
            BoundedInterval<double>(1, 3, BoundType::CLOSED),
            std::back_inserter(members));
    REDIS_ASSERT(members.size() == sReversedKeys.size()-1,
                 "failed to test zrevrangebyscore (size)");
    for (size_t i=0; i<members.size(); i++) {
        REDIS_ASSERT(members.at(i) == sReversedKeys.at(i+1),
                     "failed to test zrevrangebyscore");
    }

    limitOpts.offset = 0;
    members.clear();
    _redis.zrevrangebyscore(key,
            BoundedInterval<double>(1, 3, BoundType::CLOSED),
            limitOpts,
            std::back_inserter(members));
    REDIS_ASSERT(members.size() == 2 &&
                 members.at(0) == sReversedKeys.at(1) &&
                 members.at(1) == sReversedKeys.at(2),
                 "failed to test zrevrangebyscore with limits 0, 2");

    limitOpts.offset = 1;
    members.clear();
    _redis.zrevrangebyscore(key,
                            BoundedInterval<double>(1, 3, BoundType::CLOSED),
                            limitOpts,
                            std::back_inserter(members));
    REDIS_ASSERT(members.size() == 2 &&
                 members.at(0) == sReversedKeys.at(2) &&
                 members.at(1) == sReversedKeys.at(3),
                 "failed to test zrevrangebyscore with limits 1, 2");

    limitOpts.offset = s.size()-2;
    members.clear();
    _redis.zrevrangebyscore(key,
                            BoundedInterval<double>(1, 3, BoundType::CLOSED),
                            limitOpts,
                            std::back_inserter(members));
    REDIS_ASSERT(members.size() == 1 &&
                 members.at(0) == sReversedKeys.at(s.size()-1),
                 "failed to test zrevrangebyscore with limits size-2, 2");

    res.clear();
    _redis.zrevrangebyscore(key,
            RightBoundedInterval<double>(1, BoundType::LEFT_OPEN),
            std::inserter(res, res.end()));
    REDIS_ASSERT(res.size() == 1 && res.at(0) == s.at(0), "failed to test zrevrangebyscore");

    REDIS_ASSERT(_redis.zremrangebyrank(key, 0, 0) == 1, "failed to test zremrangebyrank");

    REDIS_ASSERT(_redis.zremrangebyscore(key,
                BoundedInterval<double>(2, 3, BoundType::LEFT_OPEN)) == 1,
            "failed to test zremrangebyscore");
}

template <typename RedisInstance>
void ZSetCmdTest<RedisInstance>::_test_lex() {
    auto key = test_key("lex");

    KeyDeleter<RedisInstance> deleter(_redis, key);

    auto s = {
        std::make_pair("m1", 0),
        std::make_pair("m2", 0),
        std::make_pair("m3", 0),
    };
    _redis.zadd(key, s.begin(), s.end());

    REDIS_ASSERT(_redis.zlexcount(key, UnboundedInterval<std::string>{}) == 3,
            "failed to test zlexcount");

    std::vector<std::string> members;
    _redis.zrangebylex(key,
            LeftBoundedInterval<std::string>("m2", BoundType::OPEN),
            std::back_inserter(members));
    REDIS_ASSERT(members.size() == 1 && members[0] == "m3",
            "failed to test zrangebylex");

    members.clear();
    _redis.zrevrangebylex(key,
            RightBoundedInterval<std::string>("m1", BoundType::LEFT_OPEN),
            std::back_inserter(members));
    REDIS_ASSERT(members.size() == 1 && members[0] == "m1",
            "failed to test zrevrangebylex");

    REDIS_ASSERT(_redis.zremrangebylex(key,
                BoundedInterval<std::string>("m1", "m3", BoundType::OPEN)) == 1,
            "failed to test zremrangebylex");
}

template <typename RedisInstance>
void ZSetCmdTest<RedisInstance>::_test_multi_zset() {
    auto k1 = test_key("k1");
    auto k2 = test_key("k2");
    auto k3 = test_key("k3");

    KeyDeleter<RedisInstance> deleter(_redis, {k1, k2, k3});

    _redis.zadd(k1, {std::make_pair("a", 1), std::make_pair("b", 2)});
    _redis.zadd(k2, {std::make_pair("a", 2), std::make_pair("c", 3)});

    REDIS_ASSERT(_redis.zinterstore(k3, {k1, k2}) == 1, "failed to test zinterstore");
    auto score = _redis.zscore(k3, "a");
    REDIS_ASSERT(bool(score) && *score == 3, "failed to test zinterstore");

    REDIS_ASSERT(_redis.zinterstore(k3, k1, 2) == 2, "failed to test zinterstore");

    _redis.del(k3);

    REDIS_ASSERT(_redis.zinterstore(k3, {k1, k2}, Aggregation::MAX) == 1,
            "failed to test zinterstore");
    score = _redis.zscore(k3, "a");
    REDIS_ASSERT(bool(score) && *score == 2, "failed to test zinterstore");

    _redis.del(k3);

    REDIS_ASSERT(_redis.zunionstore(k3,
                {std::make_pair(k1, 1), std::make_pair(k2, 2)},
                Aggregation::MIN) == 3,
            "failed to test zunionstore");
    std::vector<std::pair<std::string, double>> res;
    _redis.zrange(k3, 0, -1, std::back_inserter(res));
    for (const auto &ele : res) {
        if (ele.first == "a") {
            REDIS_ASSERT(ele.second == 1, "failed to test zunionstore");
        } else if (ele.first == "b") {
            REDIS_ASSERT(ele.second == 2, "failed to test zunionstore");
        } else if (ele.first == "c") {
            REDIS_ASSERT(ele.second == 6, "failed to test zunionstore");
        } else {
            REDIS_ASSERT(false, "failed to test zuionstore");
        }
    }

    REDIS_ASSERT(_redis.zunionstore(k3, k1, 2) == 2, "failed to test zunionstore");
}

template <typename RedisInstance>
void ZSetCmdTest<RedisInstance>::_test_zpop() {
    auto key = test_key("zpop");
    auto not_exist_key = test_key("zpop_not_exist");

    KeyDeleter<RedisInstance> deleter(_redis, key);

    _redis.zadd(key, {std::make_pair("m1", 1.1),
                        std::make_pair("m2", 2.2),
                        std::make_pair("m3", 3.3),
                        std::make_pair("m4", 4.4),
                        std::make_pair("m5", 5.5),
                        std::make_pair("m6", 6.6)});

    auto item = _redis.zpopmax(key);
    REDIS_ASSERT(item && item->first == "m6", "failed to test zpopmax");

    item = _redis.zpopmax(not_exist_key);
    REDIS_ASSERT(!item, "failed to test zpopmax");

    item = _redis.zpopmin(key);
    REDIS_ASSERT(item && item->first == "m1", "failed to test zpopmin");

    item = _redis.zpopmin(not_exist_key);
    REDIS_ASSERT(!item, "failed to test zpopmin");

    std::vector<std::pair<std::string, double>> vec;
    _redis.zpopmax(key, 2, std::back_inserter(vec));
    REDIS_ASSERT(vec.size() == 2 && vec[0].first == "m5" && vec[1].first == "m4",
            "failed to test zpopmax");

    std::unordered_map<std::string, double> m;
    _redis.zpopmin(key, 2, std::inserter(m, m.end()));
    REDIS_ASSERT(m.size() == 2 && m.find("m3") != m.end() && m.find("m2") != m.end(),
            "failed to test zpopmin");
}

template <typename RedisInstance>
void ZSetCmdTest<RedisInstance>::_test_bzpop() {
    auto key1 = test_key("bzpop1");
    auto key2 = test_key("bzpop2");

    KeyDeleter<RedisInstance> deleter(_redis, {key1, key2});

    _redis.zadd(key1, {std::make_pair("m1", 1.1),
                        std::make_pair("m2", 2.2),
                        std::make_pair("m3", 3.3),
                        std::make_pair("m4", 4.4),
                        std::make_pair("m5", 5.5),
                        std::make_pair("m6", 6.6)});

    _redis.zadd(key2, {std::make_pair("m1", 1.1),
                        std::make_pair("m2", 2.2),
                        std::make_pair("m3", 3.3),
                        std::make_pair("m4", 4.4),
                        std::make_pair("m5", 5.5),
                        std::make_pair("m6", 6.6)});

    auto item = _redis.bzpopmax(key1);
    REDIS_ASSERT(item && std::get<0>(*item) == key1 && std::get<1>(*item) == "m6",
            "failed to test bzpopmax");

    item = _redis.bzpopmin(key1, std::chrono::seconds(1));
    REDIS_ASSERT(item && std::get<0>(*item) == key1 && std::get<1>(*item) == "m1",
            "failed to test zpopmin");

    item = _redis.bzpopmax({key1, key2}, std::chrono::seconds(1));
    REDIS_ASSERT(item && std::get<0>(*item) == key1 && std::get<1>(*item) == "m5",
            "failed to test zpopmax");

    item = _redis.bzpopmin({key2, key1});
    REDIS_ASSERT(item && std::get<0>(*item) == key2 && std::get<1>(*item) == "m1",
            "failed to test zpopmin");
}

}

}

}

#endif // end SEWENEW_REDISPLUSPLUS_TEST_ZSET_CMDS_TEST_HPP
