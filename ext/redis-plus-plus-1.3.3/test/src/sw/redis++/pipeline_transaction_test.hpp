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

#ifndef SEWENEW_REDISPLUSPLUS_TEST_PIPELINE_TRANSACTION_TEST_HPP
#define SEWENEW_REDISPLUSPLUS_TEST_PIPELINE_TRANSACTION_TEST_HPP

#include <string>
#include "utils.h"

namespace sw {

namespace redis {

namespace test {

template <typename RedisInstance>
void PipelineTransactionTest<RedisInstance>::run() {
    {
        auto key = test_key("pipeline");
        KeyDeleter<RedisInstance> deleter(_redis, key);
        auto pipe = _pipeline(key, true);
        _test_pipeline(key, pipe);
    }

    {
        auto key = test_key("pipeline");
        KeyDeleter<RedisInstance> deleter(_redis, key);
        auto pipe = _pipeline(key, false);
        _test_pipeline(key, pipe);
    }

    {
        auto key = test_key("pipeline");
        KeyDeleter<RedisInstance> deleter(_redis, key);
        auto pipe = _pipeline(key, false);
        _test_pipeline_streams(key, pipe);
    }

    {
        auto key = test_key("transaction");
        KeyDeleter<RedisInstance> deleter(_redis, key);
        auto tx = _transaction(key, true, true);
        _test_transaction(key, tx);
    }

    {
        auto key = test_key("transaction");
        KeyDeleter<RedisInstance> deleter(_redis, key);
        auto tx = _transaction(key, false, true);
        _test_transaction(key, tx);
    }

    {
        auto key = test_key("transaction");
        KeyDeleter<RedisInstance> deleter(_redis, key);
        auto tx = _transaction(key, true, false);
        _test_transaction(key, tx);
    }

    {
        auto key = test_key("transaction");
        KeyDeleter<RedisInstance> deleter(_redis, key);
        auto tx = _transaction(key, false, false);
        _test_transaction(key, tx);
    }

    _test_watch();

    _test_error_handle(true);
    _test_error_handle(false);
}

template <typename RedisInstance>
Pipeline PipelineTransactionTest<RedisInstance>::_pipeline(const StringView &,
        bool new_connection) {
    return _redis.pipeline(new_connection);
}

template <>
inline Pipeline PipelineTransactionTest<RedisCluster>::_pipeline(const StringView &key,
        bool new_connection) {
    return _redis.pipeline(key, new_connection);
}

template <typename RedisInstance>
Transaction PipelineTransactionTest<RedisInstance>::_transaction(const StringView &,
        bool piped, bool new_connection) {
    return _redis.transaction(piped, new_connection);
}

template <>
inline Transaction PipelineTransactionTest<RedisCluster>::_transaction(const StringView &key,
        bool piped, bool new_connection) {
    return _redis.transaction(key, piped, new_connection);
}

template <typename RedisInstance>
void PipelineTransactionTest<RedisInstance>::_test_pipeline(const StringView &key,
        Pipeline &pipe) {
    std::string val("value");
    auto replies = pipe.set(key, val)
                        .get(key)
                        .strlen(key)
                        .exec();

    REDIS_ASSERT(replies.get<bool>(0), "failed to test pipeline with set operation");

    auto new_val = replies.get<OptionalString>(1);
    std::size_t len = replies.get<long long>(2);
    REDIS_ASSERT(bool(new_val) && *new_val == val && len == val.size(),
            "failed to test pipeline with string operations");

    REDIS_ASSERT(reply::parse<bool>(replies.get(0)), "failed to test pipeline with set operation");

    new_val = reply::parse<OptionalString>(replies.get(1));
    len = reply::parse<long long>(replies.get(2));
    REDIS_ASSERT(bool(new_val) && *new_val == val && len == val.size(),
            "failed to test pipeline with string operations");
}

template <typename RedisInstance>
void PipelineTransactionTest<RedisInstance>::_test_pipeline_streams(const StringView &key,
        Pipeline &pipe) {

    const std::vector<std::string> ids = {"1565427842-0", "1565427842-1"};
    std::vector<std::pair<std::string, std::string>> attrs = {
        {"f1", "v1"},
        {"f2", "v2"}
    };
    for (auto id : ids) {
        pipe.xadd(key, id, attrs.begin(), attrs.end());
    }
    for (auto id : ids) {
        pipe.xrange(key, "-", "+", 1);
        pipe.xrevrange(key, "+", "-", 1);
    }
    auto replies = pipe.exec();
    int replyCnt = 0;
    using Item = std::pair<std::string, std::unordered_map<std::string, std::string>>;
    std::vector<Item> items;
    for (auto id : ids) {
        auto reply = replies.get(replyCnt++);
        std::string retId = reply::parse<std::string>(reply);
        REDIS_ASSERT(retId == id, "failed to test pipeline_streams with xadd");
    }
    for (auto id : ids) {
        auto replyXrange = replies.get(replyCnt++);
        std::vector<Item> items = reply::parse<std::vector<Item>>(replyXrange);
        auto replyXrevrange = replies.get(replyCnt++);
        std::vector<Item> itemsRev = reply::parse<std::vector<Item>>(replyXrevrange);
        REDIS_ASSERT(
            items.size() == 1 &&
            items.at(0).first == ids.at(0) &&
            itemsRev.at(0).first == ids.at(ids.size()-1),
            "failed to test pipeline_streams with count");
    }
}

template <typename RedisInstance>
void PipelineTransactionTest<RedisInstance>::_test_transaction(const StringView &key,
        Transaction &tx) {
    std::unordered_map<std::string, std::string> m = {
        std::make_pair("f1", "v1"),
        std::make_pair("f2", "v2"),
        std::make_pair("f3", "v3")
    };
    auto replies = tx.hmset(key, m.begin(), m.end())
                        .hgetall(key)
                        .hdel(key, "f1")
                        .exec();

    replies.get<void>(0);

    decltype(m) mm;
    replies.get(1, std::inserter(mm, mm.end()));
    REDIS_ASSERT(mm == m, "failed to test transaction");

    REDIS_ASSERT(replies.get<long long>(2) == 1, "failed to test transaction");

    tx.set(key, "value")
        .get(key)
        .incr(key);

    tx.discard();

    replies = tx.del(key)
                .set(key, "value")
                .exec();

    REDIS_ASSERT(replies.get<long long>(0) == 1, "failed to test transaction");

    REDIS_ASSERT(replies.get<bool>(1), "failed to test transaction");
}

template <typename RedisInstance>
void PipelineTransactionTest<RedisInstance>::_test_watch() {
    auto key = test_key("watch");

    KeyDeleter<RedisInstance> deleter(_redis, key);

    {
        auto tx = _transaction(key, false, true);

        auto redis = tx.redis();

        redis.watch(key);

        auto replies = tx.set(key, "1").get(key).exec();

        REDIS_ASSERT(replies.size() == 2
                && replies.template get<bool>(0) == true, "failed to test watch");

        auto val = replies.template get<sw::redis::OptionalString>(1);

        REDIS_ASSERT(val && *val == "1", "failed to test watch");
    }

    try {
        auto tx = _transaction(key, false, true);

        auto redis = tx.redis();

        redis.watch(key);

        // Key has been modified by other client.
        _redis.set(key, "val");

        // Transaction should fail, and throw WatchError
        tx.set(key, "1").exec();

        REDIS_ASSERT(false, "failed to test watch");
    } catch (const sw::redis::WatchError &err) {
        // Catch the error.
    }
}

template <typename RedisInstance>
void PipelineTransactionTest<RedisInstance>::_test_error_handle(bool new_connection) {
    auto key = test_key("error_handle");

    KeyDeleter<RedisInstance> deleter(_redis, key);

    try {
        auto pipe = _pipeline(key, new_connection);

        // This will fail
        pipe.set(key, "val").hget(key, "field").exec();
    } catch (const sw::redis::Error &err) {
        // Ignore the error.
    }

    auto pipe = _pipeline(key, new_connection);
    std::string val("val");
    auto replies = pipe.set(key, val).get(key).exec();
    REDIS_ASSERT(replies.size() == 2, "failed to test pipeline's error handling");
    auto value = replies.template get<sw::redis::OptionalString>(1);
    REDIS_ASSERT(value && *value == val, "failed to test pipeline's error handling");

    {
        auto pipe = _pipeline(key, new_connection);
        pipe.set(key, "val");

        // Forget to call exec() or discard()
    }

    pipe = _pipeline(key, new_connection);
    val = "new value";
    replies = pipe.set(key, val).get(key).exec();
    REDIS_ASSERT(replies.size() == 2, "failed to test pipeline's error handling");
    value = replies.template get<sw::redis::OptionalString>(1);
    REDIS_ASSERT(value && *value == val, "failed to test pipeline's error handling");
}

}

}

}

#endif // end SEWENEW_REDISPLUSPLUS_TEST_PIPELINE_TRANSACTION_TEST_HPP
