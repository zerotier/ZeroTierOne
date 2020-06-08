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

#ifndef SEWENEW_REDISPLUSPLUS_TEST_SANITY_TEST_H
#define SEWENEW_REDISPLUSPLUS_TEST_SANITY_TEST_H

#include <sw/redis++/redis++.h>

namespace sw {

namespace redis {

namespace test {

template <typename RedisInstance>
class SanityTest {
public:
    SanityTest(const ConnectionOptions &opts, RedisInstance &instance)
        : _opts(opts), _redis(instance) {}

    void run();

private:
    void _test_uri_ctor();

    void _ping(Redis &instance);

    void _test_move_ctor();

    void _test_cmdargs();

    void _test_generic_command();

    void _test_hash_tag();

    void _test_hash_tag(std::initializer_list<std::string> keys);

    std::string _test_key(const std::string &key);

    void _test_ping(Redis &instance);

    void _test_pipeline(const StringView &key, Pipeline &pipeline);

    void _test_transaction(const StringView &key, Transaction &transaction);

    Pipeline _pipeline(const StringView &key);

    Transaction _transaction(const StringView &key);

    ConnectionOptions _opts;

    RedisInstance &_redis;
};

}

}

}

#include "sanity_test.hpp"

#endif // end SEWENEW_REDISPLUSPLUS_TEST_SANITY_TEST_H
