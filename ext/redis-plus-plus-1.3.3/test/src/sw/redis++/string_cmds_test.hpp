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

#ifndef SEWENEW_REDISPLUSPLUS_TEST_STRING_CMDS_TEST_HPP
#define SEWENEW_REDISPLUSPLUS_TEST_STRING_CMDS_TEST_HPP

#include <vector>
#include "utils.h"

namespace sw {

namespace redis {

namespace test {

template <typename RedisInstance>
void StringCmdTest<RedisInstance>::run() {
    _test_str();

    _test_bit();

    _test_numeric();

    _test_getset();

    _test_mgetset();
}

template <typename RedisInstance>
void StringCmdTest<RedisInstance>::_test_str() {
    auto key = test_key("str");

    KeyDeleter<RedisInstance> deleter(_redis, key);

    std::string val("value");

    long long val_size = val.size();

    auto len1 = _redis.append(key, val);
    REDIS_ASSERT(len1 == val_size, "failed to append to non-existent key");

    auto len2 = _redis.append(key, val);
    REDIS_ASSERT(len2 == len1 + val_size, "failed to append to non-empty string");

    auto len3 = _redis.append(key, {});
    REDIS_ASSERT(len3 == len2, "failed to append empty string");

    auto len4 = _redis.strlen(key);
    REDIS_ASSERT(len4 == len3, "failed to test strlen");

    REDIS_ASSERT(_redis.del(key) == 1, "failed to remove key");

    auto len5 = _redis.append(key, {});
    REDIS_ASSERT(len5 == 0, "failed to append empty string to non-existent key");

    _redis.del(key);

    REDIS_ASSERT(_redis.getrange(key, 0, 2) == "", "failed to test getrange on non-existent key");

    _redis.set(key, val);

    REDIS_ASSERT(_redis.getrange(key, 1, 2) == val.substr(1, 2), "failed to test getrange");

    long long new_size = val.size() * 2;
    REDIS_ASSERT(_redis.setrange(key, val.size(), val) == new_size, "failed to test setrange");
    REDIS_ASSERT(_redis.getrange(key, 0, -1) == val + val, "failed to test setrange");
}

template <typename RedisInstance>
void StringCmdTest<RedisInstance>::_test_bit() {
    auto key = test_key("bit");

    KeyDeleter<RedisInstance> deleter(_redis, key);

    REDIS_ASSERT(_redis.bitcount(key) == 0, "failed to test bitcount on non-existent key");

    REDIS_ASSERT(_redis.getbit(key, 5) == 0, "failed to test getbit");

    REDIS_ASSERT(_redis.template command<long long>("SETBIT", key, 1, 1) == 0,
            "failed to test setbit");
    REDIS_ASSERT(_redis.template command<long long>("SETBIT", key, 3, 1) == 0,
            "failed to test setbit");
    REDIS_ASSERT(_redis.template command<long long>("SETBIT", key, 7, 1) == 0,
            "failed to test setbit");
    REDIS_ASSERT(_redis.template command<long long>("SETBIT", key, 10, 1) == 0,
            "failed to test setbit");
    REDIS_ASSERT(_redis.template command<long long>("SETBIT", key, 10, 0) == 1,
            "failed to test setbit");
    REDIS_ASSERT(_redis.template command<long long>("SETBIT", key, 11, 1) == 0,
            "failed to test setbit");
    REDIS_ASSERT(_redis.template command<long long>("SETBIT", key, 21, 1) == 0,
            "failed to test setbit");

    // key -> 01010001, 00010000, 00000100

    REDIS_ASSERT(_redis.getbit(key, 1) == 1, "failed to test getbit");
    REDIS_ASSERT(_redis.getbit(key, 2) == 0, "failed to test getbit");
    REDIS_ASSERT(_redis.getbit(key, 7) == 1, "failed to test getbit");
    REDIS_ASSERT(_redis.getbit(key, 10) == 0, "failed to test getbit");
    REDIS_ASSERT(_redis.getbit(key, 100) == 0, "failed to test getbit");

    REDIS_ASSERT(_redis.bitcount(key) == 5, "failed to test bitcount");
    REDIS_ASSERT(_redis.bitcount(key, 0, 0) == 3, "failed to test bitcount");
    REDIS_ASSERT(_redis.bitcount(key, 0, 1) == 4, "failed to test bitcount");
    REDIS_ASSERT(_redis.bitcount(key, -2, -1) == 2, "failed to test bitcount");

    REDIS_ASSERT(_redis.bitpos(key, 1) == 1, "failed to test bitpos");
    REDIS_ASSERT(_redis.bitpos(key, 0) == 0, "failed to test bitpos");
    REDIS_ASSERT(_redis.bitpos(key, 1, 1, 1) == 11, "failed to test bitpos");
    REDIS_ASSERT(_redis.bitpos(key, 0, 1, 1) == 8, "failed to test bitpos");
    REDIS_ASSERT(_redis.bitpos(key, 1, -1, -1) == 21, "failed to test bitpos");
    REDIS_ASSERT(_redis.bitpos(key, 0, -1, -1) == 16, "failed to test bitpos");

    auto dest_key = test_key("bitop_dest");
    auto src_key1 = test_key("bitop_src1");
    auto src_key2 = test_key("bitop_src2");

    KeyDeleter<RedisInstance> deleters(_redis, {dest_key, src_key1, src_key2});

    // src_key1 -> 00010000
    _redis.template command<long long>("SETBIT", src_key1, 3, 1);

    // src_key2 -> 00000000, 00001000
    _redis.template command<long long>("SETBIT", src_key2, 12, 1);

    REDIS_ASSERT(_redis.bitop(BitOp::AND, dest_key, {src_key1, src_key2}) == 2,
            "failed to test bitop");

    // dest_key -> 00000000, 00000000
    auto v = _redis.get(dest_key);
    REDIS_ASSERT(v && *v == std::string(2, 0), "failed to test bitop");

    REDIS_ASSERT(_redis.bitop(BitOp::NOT, dest_key, src_key1) == 1,
            "failed to test bitop");

    // dest_key -> 11101111
    v = _redis.get(dest_key);
    REDIS_ASSERT(v && *v == std::string(1, '\xEF'), "failed to test bitop");
}

template <typename RedisInstance>
void StringCmdTest<RedisInstance>::_test_numeric() {
    auto key = test_key("numeric");

    KeyDeleter<RedisInstance> deleter(_redis, key);

    REDIS_ASSERT(_redis.incr(key) == 1, "failed to test incr");
    REDIS_ASSERT(_redis.decr(key) == 0, "failed to test decr");
    REDIS_ASSERT(_redis.incrby(key, 3) == 3, "failed to test incrby");
    REDIS_ASSERT(_redis.decrby(key, 3) == 0, "failed to test decrby");
    REDIS_ASSERT(_redis.incrby(key, -3) == -3, "failed to test incrby");
    REDIS_ASSERT(_redis.decrby(key, -3) == 0, "failed to test incrby");
    REDIS_ASSERT(_redis.incrbyfloat(key, 1.5) == 1.5, "failed to test incrbyfloat");
}

template <typename RedisInstance>
void StringCmdTest<RedisInstance>::_test_getset() {
    auto key = test_key("getset");
    auto non_exist_key = test_key("non-existent");

    KeyDeleter<RedisInstance> deleter(_redis, {key, non_exist_key});

    std::string val("value");
    REDIS_ASSERT(_redis.set(key, val), "failed to test set");

    auto v = _redis.get(key);
    REDIS_ASSERT(v && *v == val, "failed to test get");

    v = _redis.getset(key, val + val);
    REDIS_ASSERT(v && *v == val, "failed to test get");

    REDIS_ASSERT(!_redis.set(key, val, std::chrono::milliseconds(0), UpdateType::NOT_EXIST),
            "failed to test set with NOT_EXIST type");
    REDIS_ASSERT(!_redis.set(non_exist_key, val, std::chrono::milliseconds(0), UpdateType::EXIST),
            "failed to test set with EXIST type");

    REDIS_ASSERT(!_redis.setnx(key, val), "failed to test setnx");
    REDIS_ASSERT(_redis.setnx(non_exist_key, val), "failed to test setnx");

    auto ttl = std::chrono::seconds(10);

    _redis.set(key, val, ttl);
    REDIS_ASSERT(_redis.ttl(key) <= ttl.count(), "failed to test set key with ttl");

    _redis.setex(key, ttl, val);
    REDIS_ASSERT(_redis.ttl(key) <= ttl.count(), "failed to test setex");

    auto pttl = std::chrono::milliseconds(10000);

    _redis.psetex(key, ttl, val);
    REDIS_ASSERT(_redis.pttl(key) <= pttl.count(), "failed to test psetex");
}

template <typename RedisInstance>
void StringCmdTest<RedisInstance>::_test_mgetset() {
    auto kvs = {std::make_pair(test_key("k1"), "v1"),
                std::make_pair(test_key("k2"), "v2"),
                std::make_pair(test_key("k3"), "v3")};

    std::vector<std::string> keys;
    std::vector<std::string> vals;
    for (const auto &kv : kvs) {
        keys.push_back(kv.first);
        vals.push_back(kv.second);
    }

    KeyDeleter<RedisInstance> deleter(_redis, keys.begin(), keys.end());

    _redis.mset(kvs);

    std::vector<OptionalString> res;
    _redis.mget(keys.begin(), keys.end(), std::back_inserter(res));

    REDIS_ASSERT(res.size() == kvs.size(), "failed to test mget");

    std::vector<std::string> res_vals;
    for (const auto &ele : res) {
        REDIS_ASSERT(bool(ele), "failed to test mget");

        res_vals.push_back(*ele);
    }

    REDIS_ASSERT(vals == res_vals, "failed to test mget");

    REDIS_ASSERT(!_redis.msetnx(kvs), "failed to test msetnx");
}

}

}

}

#endif // end SEWENEW_REDISPLUSPLUS_TEST_STRING_CMDS_TEST_HPP
