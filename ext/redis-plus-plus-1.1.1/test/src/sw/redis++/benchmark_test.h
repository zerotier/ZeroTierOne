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

#ifndef SEWENEW_REDISPLUSPLUS_TEST_BENCHMARK_TEST_H
#define SEWENEW_REDISPLUSPLUS_TEST_BENCHMARK_TEST_H

#include <sw/redis++/redis++.h>

namespace sw {

namespace redis {

namespace test {

struct BenchmarkOptions {
    std::size_t pool_size = 5;
    std::size_t thread_num = 10;
    std::size_t total_request_num = 100000;
    std::size_t key_len = 10;
    std::size_t val_len = 10;
};

template <typename RedisInstance>
class BenchmarkTest {
public:
    BenchmarkTest(const BenchmarkOptions &opts, RedisInstance &instance);

    ~BenchmarkTest() {
        _cleanup();
    }

    void run();

private:
    template <typename Func>
    void _run(const std::string &title, Func &&func);

    template <typename Func>
    std::size_t _run(Func &&func, std::size_t request_num);

    void _test_get();

    std::vector<std::string> _gen_keys() const;

    std::string _gen_value() const;

    void _cleanup();

    const std::string& _key(std::size_t idx) const {
        return _keys[idx % _keys.size()];
    }

    BenchmarkOptions _opts;

    RedisInstance &_redis;

    std::vector<std::string> _keys;

    std::string _value;
};

}

}

}

#include "benchmark_test.hpp"

#endif // end SEWENEW_REDISPLUSPLUS_TEST_BENCHMARK_TEST_H
