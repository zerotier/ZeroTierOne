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

#ifndef SEWENEW_REDISPLUSPLUS_TEST_BENCHMARK_TEST_HPP
#define SEWENEW_REDISPLUSPLUS_TEST_BENCHMARK_TEST_HPP

#include <chrono>
#include <random>
#include <future>
#include <algorithm>
#include "utils.h"

namespace sw {

namespace redis {

namespace test {

template <typename RedisInstance>
BenchmarkTest<RedisInstance>::BenchmarkTest(const BenchmarkOptions &opts,
        RedisInstance &instance) : _opts(opts), _redis(instance) {
    REDIS_ASSERT(_opts.pool_size > 0
            && _opts.thread_num > 0
            && _opts.total_request_num > 0
            && _opts.key_len > 0
            && _opts.val_len > 0,
                "Invalid benchmark test options.");

    _keys = _gen_keys();
    _value = _gen_value();
}

template <typename RedisInstance>
void BenchmarkTest<RedisInstance>::run() {
    _cleanup();

    _run("SET key value", [this](std::size_t idx) { this->_redis.set(this->_key(idx), _value); });

    _run("GET key", [this](std::size_t idx) {
                        auto res = this->_redis.get(this->_key(idx));
                        (void)res;
                    });

    _cleanup();

    _run("LPUSH key value", [this](std::size_t idx) {
                                this->_redis.lpush(this->_key(idx), _value);
                            });

    _run("LRANGE key 0 10", [this](std::size_t idx) {
                std::vector<std::string> res;
                res.reserve(10);
                this->_redis.lrange(this->_key(idx), 0, 10, std::back_inserter(res));
            });

    _run("LPOP key", [this](std::size_t idx) {
                        auto res = this->_redis.lpop(this->_key(idx));
                        (void)res;
                     });

    _cleanup();

    _run("INCR key", [this](std::size_t idx) {
                        auto num = this->_redis.incr(this->_key(idx));
                        (void)num;
                     });

    _cleanup();

    _run("SADD key member", [this](std::size_t idx) {
                                auto num = this->_redis.sadd(this->_key(idx), _value);
                                (void)num;
                            });

    _run("SPOP key", [this](std::size_t idx) {
                        auto res = this->_redis.spop(this->_key(idx));
                        (void)res;
                     });

    _cleanup();
}

template <typename RedisInstance>
template <typename Func>
void BenchmarkTest<RedisInstance>::_run(const std::string &title, Func &&func) {
    auto thread_num = _opts.thread_num;
    auto requests_per_thread = _opts.total_request_num / thread_num;
    auto total_request_num = requests_per_thread * thread_num;
    std::vector<std::future<std::size_t>> res;
    res.reserve(thread_num);
    res.push_back(std::async(std::launch::async,
                    [this](Func &&func, std::size_t request_num) {
                        return this->_run(std::forward<Func>(func), request_num);
                    },
                    std::forward<Func>(func),
                    requests_per_thread));

    auto total_in_msec = 0;
    for (auto &fut : res) {
        total_in_msec += fut.get();
    }

    auto total_in_sec = total_in_msec * 1.0 / 1000;

    auto avg = total_in_msec * 1.0 / total_request_num;

    auto ops = static_cast<std::size_t>(1000 / avg);

    std::cout << "-----" << title << "-----" << std::endl;
    std::cout << total_request_num << " requests cost " << total_in_sec << " seconds" << std::endl;
    std::cout << ops << " requests per second" << std::endl;
}

template <typename RedisInstance>
template <typename Func>
std::size_t BenchmarkTest<RedisInstance>::_run(Func &&func, std::size_t request_num) {
    auto start = std::chrono::steady_clock::now();

    for (auto idx = 0U; idx != request_num; ++idx) {
        func(idx);
    }

    auto stop = std::chrono::steady_clock::now();

    return std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
}

template <typename RedisInstance>
std::vector<std::string> BenchmarkTest<RedisInstance>::_gen_keys() const {
    const auto KEY_NUM = 100;
    std::vector<std::string> res;
    res.reserve(KEY_NUM);
    std::default_random_engine engine(std::random_device{}());
    std::uniform_int_distribution<int> uniform_dist(0, 255);
    for (auto i = 0; i != KEY_NUM; ++i) {
        std::string str;
        str.reserve(_opts.key_len);
        for (std::size_t j = 0; j != _opts.key_len; ++j) {
            str.push_back(static_cast<char>(uniform_dist(engine)));
        }
        res.push_back(str);
    }

    return res;
}

template <typename RedisInstance>
std::string BenchmarkTest<RedisInstance>::_gen_value() const {
    return std::string(_opts.val_len, 'x');
}

template <typename RedisInstance>
void BenchmarkTest<RedisInstance>::_cleanup() {
    for (const auto &key : _keys) {
        _redis.del(key);
    }
}

}

}

}

#endif // end SEWENEW_REDISPLUSPLUS_TEST_BENCHMARK_TEST_HPP
