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

#ifndef SEWENEW_REDISPLUSPLUS_TEST_CONNECTION_CMDS_TEST_HPP
#define SEWENEW_REDISPLUSPLUS_TEST_CONNECTION_CMDS_TEST_HPP

#include "utils.h"

namespace sw {

namespace redis {

namespace test {

template <typename RedisInstance>
void ConnectionCmdTest<RedisInstance>::run() {
    cluster_specializing_test(*this, &ConnectionCmdTest<RedisInstance>::_run, _redis);
}

template <typename RedisInstance>
void ConnectionCmdTest<RedisInstance>::_run(Redis &instance) {
    auto message = std::string("hello");

    REDIS_ASSERT(instance.echo(message) == message, "failed to test echo");

    REDIS_ASSERT(instance.ping() == "PONG", "failed to test ping");

    REDIS_ASSERT(instance.ping(message) == message, "failed to test ping");
}

}

}

}

#endif // end SEWENEW_REDISPLUSPLUS_TEST_CONNECTION_CMDS_TEST_HPP
