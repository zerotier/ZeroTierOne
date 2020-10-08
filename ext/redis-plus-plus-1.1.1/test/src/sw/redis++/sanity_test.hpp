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

#ifndef SEWENEW_REDISPLUSPLUS_TEST_SANITY_TEST_HPP
#define SEWENEW_REDISPLUSPLUS_TEST_SANITY_TEST_HPP

#include "utils.h"

namespace sw {

namespace redis {

namespace test {

template <typename RedisInstance>
void SanityTest<RedisInstance>::run() {
    _test_uri_ctor();

    _test_move_ctor();

    cluster_specializing_test(*this, &SanityTest<RedisInstance>::_test_ping, _redis);

    auto pipe_key = test_key("pipeline");
    auto tx_key = test_key("transaction");

    KeyDeleter<RedisInstance> deleter(_redis, {pipe_key, tx_key});

    auto pipeline = _pipeline(pipe_key);
    _test_pipeline(pipe_key, pipeline);

    auto transaction = _transaction(tx_key);
    _test_transaction(tx_key, transaction);

    _test_cmdargs();

    _test_generic_command();
}

template <typename RedisInstance>
void SanityTest<RedisInstance>::_test_uri_ctor() {
    std::string uri;
    switch (_opts.type) {
    case sw::redis::ConnectionType::TCP:
        uri = "tcp://" + _opts.host + ":" + std::to_string(_opts.port);
        break;

    case sw::redis::ConnectionType::UNIX:
        REDIS_ASSERT(false, "NO test for UNIX Domain Socket");
        break;

    default:
        REDIS_ASSERT(false, "Unknown connection type");
    }

    auto instance = RedisInstance(uri);

    cluster_specializing_test(*this, &SanityTest<RedisInstance>::_ping, instance);
}

template <typename RedisInstance>
void SanityTest<RedisInstance>::_ping(Redis &instance) {
    try {
        auto pong = instance.ping();
        REDIS_ASSERT(pong == "PONG", "Failed to test constructing Redis with uri");
    } catch (const sw::redis::ReplyError &e) {
        REDIS_ASSERT(e.what() == std::string("NOAUTH Authentication required."),
                "Failed to test constructing Redis with uri");
    }
}

template <typename RedisInstance>
void SanityTest<RedisInstance>::_test_move_ctor() {
    auto test_move_ctor = std::move(_redis);

    _redis = std::move(test_move_ctor);
}

template <typename RedisInstance>
void SanityTest<RedisInstance>::_test_cmdargs() {
    auto lpush_num = [](Connection &connection, const StringView &key, long long num) {
        connection.send("LPUSH %b %lld",
                        key.data(), key.size(),
                        num);
    };

    auto lpush_nums = [](Connection &connection,
                            const StringView &key,
                            const std::vector<long long> &nums) {
        CmdArgs args;
        args.append("LPUSH").append(key);
        for (auto num : nums) {
            args.append(std::to_string(num));
        }

        connection.send(args);
    };

    auto key = test_key("lpush_num");

    KeyDeleter<RedisInstance> deleter(_redis, key);

    auto reply = _redis.command(lpush_num, key, 1);
    REDIS_ASSERT(reply::parse<long long>(*reply) == 1, "failed to test cmdargs");

    std::vector<long long> nums = {2, 3, 4, 5};
    reply = _redis.command(lpush_nums, key, nums);
    REDIS_ASSERT(reply::parse<long long>(*reply) == 5, "failed to test cmdargs");

    std::vector<std::string> res;
    _redis.lrange(key, 0, -1, std::back_inserter(res));
    REDIS_ASSERT((res == std::vector<std::string>{"5", "4", "3", "2", "1"}),
            "failed to test cmdargs");
}

template <typename RedisInstance>
void SanityTest<RedisInstance>::_test_generic_command() {
    auto key = test_key("key");
    auto not_exist_key = test_key("not_exist_key");
    auto k1 = test_key("k1");
    auto k2 = test_key("k2");

    KeyDeleter<RedisInstance> deleter(_redis, {key, not_exist_key, k1, k2});

    std::string cmd("set");
    _redis.command(cmd, key, 123);
    auto reply = _redis.command("get", key);
    auto val = reply::parse<OptionalString>(*reply);
    REDIS_ASSERT(val && *val == "123", "failed to test generic command");

    val = _redis.template command<OptionalString>("get", key);
    REDIS_ASSERT(val && *val == "123", "failed to test generic command");

    std::vector<OptionalString> res;
    _redis.command("mget", key, not_exist_key, std::back_inserter(res));
    REDIS_ASSERT(res.size() == 2 && res[0] && *res[0] == "123" && !res[1],
            "failed to test generic command");

    reply = _redis.command("incr", key);
    REDIS_ASSERT(reply::parse<long long>(*reply) == 124, "failed to test generic command");

    _redis.command("mset", k1.c_str(), "v", k2.c_str(), "v");
    reply = _redis.command("mget", k1, k2);
    res.clear();
    reply::to_array(*reply, std::back_inserter(res));
    REDIS_ASSERT(res.size() == 2 && res[0] && *(res[0]) == "v" && res[1] && *(res[1]) == "v",
            "failed to test generic command");

    res = _redis.template command<std::vector<OptionalString>>("mget", k1, k2);
    REDIS_ASSERT(res.size() == 2 && res[0] && *(res[0]) == "v" && res[1] && *(res[1]) == "v",
            "failed to test generic command");

    res.clear();
    _redis.command("mget", k1, k2, std::back_inserter(res));
    REDIS_ASSERT(res.size() == 2 && res[0] && *(res[0]) == "v" && res[1] && *(res[1]) == "v",
            "failed to test generic command");

    auto set_cmd_str = {"set", key.c_str(), "new_value"};
    _redis.command(set_cmd_str.begin(), set_cmd_str.end());

    auto get_cmd_str = {"get", key.c_str()};
    reply = _redis.command(get_cmd_str.begin(), get_cmd_str.end());
    val = reply::parse<OptionalString>(*reply);
    REDIS_ASSERT(val && *val == "new_value", "failed to test generic command");

    val = _redis.template command<OptionalString>(get_cmd_str.begin(), get_cmd_str.end());
    REDIS_ASSERT(val && *val == "new_value", "failed to test generic command");

    auto mget_cmd_str = {"mget", key.c_str(), not_exist_key.c_str()};
    res.clear();
    _redis.command(mget_cmd_str.begin(), mget_cmd_str.end(), std::back_inserter(res));
    REDIS_ASSERT(res.size() == 2 && res[0] && *res[0] == "new_value" && !res[1],
            "failed to test generic command");
}

template <typename RedisInstance>
void SanityTest<RedisInstance>::_test_hash_tag() {
    _test_hash_tag({_test_key("{tag}postfix1"),
                    _test_key("{tag}postfix2"),
                    _test_key("{tag}postfix3")});

    _test_hash_tag({_test_key("prefix1{tag}postfix1"),
                    _test_key("prefix2{tag}postfix2"),
                    _test_key("prefix3{tag}postfix3")});

    _test_hash_tag({_test_key("prefix1{tag}"),
                    _test_key("prefix2{tag}"),
                    _test_key("prefix3{tag}")});

    _test_hash_tag({_test_key("prefix{}postfix"),
                    _test_key("prefix{}postfix"),
                    _test_key("prefix{}postfix")});

    _test_hash_tag({_test_key("prefix1{tag}post}fix1"),
                    _test_key("prefix2{tag}pos}tfix2"),
                    _test_key("prefix3{tag}postfi}x3")});

    _test_hash_tag({_test_key("prefix1{t{ag}postfix1"),
                    _test_key("prefix2{t{ag}postfix2"),
                    _test_key("prefix3{t{ag}postfix3")});

    _test_hash_tag({_test_key("prefix1{t{ag}postfi}x1"),
                    _test_key("prefix2{t{ag}post}fix2"),
                    _test_key("prefix3{t{ag}po}stfix3")});
}

template <typename RedisInstance>
void SanityTest<RedisInstance>::_test_hash_tag(std::initializer_list<std::string> keys) {
    KeyDeleter<RedisInstance> deleter(_redis, keys.begin(), keys.end());

    std::string value = "value";
    std::vector<std::pair<std::string, std::string>> kvs;
    for (const auto &key : keys) {
        kvs.emplace_back(key, value);
    }

    _redis.mset(kvs.begin(), kvs.end());

    std::vector<OptionalString> res;
    res.reserve(keys.size());
    _redis.mget(keys.begin(), keys.end(), std::back_inserter(res));

    REDIS_ASSERT(res.size() == keys.size(), "failed to test hash tag");

    for (const auto &ele : res) {
        REDIS_ASSERT(ele && *ele == value, "failed to test hash tag");
    }
}

template <typename RedisInstance>
std::string SanityTest<RedisInstance>::_test_key(const std::string &key) {
    REDIS_ASSERT(key.size() > 1, "failed to generate key");

    // Ensure that key prefix has NO hash tag. Also see the implementation of test_key.
    return key.substr(1);
}

template <typename RedisInstance>
void SanityTest<RedisInstance>::_test_ping(Redis &instance) {
    auto reply = instance.command("ping");
    REDIS_ASSERT(reply && reply::parse<std::string>(*reply) == "PONG",
            "failed to test generic command");

    auto pong = instance.command<std::string>("ping");
    REDIS_ASSERT(pong == "PONG", "failed to test generic command");
}

template <typename RedisInstance>
void SanityTest<RedisInstance>::_test_pipeline(const StringView &key, Pipeline &pipeline) {
    auto pipe_replies = pipeline.command("set", key, "value").command("get", key).exec();
    auto val = pipe_replies.get<OptionalString>(1);
    REDIS_ASSERT(val && *val == "value", "failed to test generic command");
}

template <typename RedisInstance>
void SanityTest<RedisInstance>::_test_transaction(const StringView &key, Transaction &transaction) {
    auto tx_replies = transaction.command("set", key, 456).command("incr", key).exec();
    REDIS_ASSERT(tx_replies.get<long long>(1) == 457, "failed to test generic command");
}

template <typename RedisInstance>
Pipeline SanityTest<RedisInstance>::_pipeline(const StringView &) {
    return _redis.pipeline();
}

template <>
inline Pipeline SanityTest<RedisCluster>::_pipeline(const StringView &key) {
    return _redis.pipeline(key);
}

template <typename RedisInstance>
Transaction SanityTest<RedisInstance>::_transaction(const StringView &) {
    return _redis.transaction();
}

template <>
inline Transaction SanityTest<RedisCluster>::_transaction(const StringView &key) {
    return _redis.transaction(key);
}

}

}

}

#endif // end SEWENEW_REDISPLUSPLUS_TEST_SANITY_TEST_HPP
