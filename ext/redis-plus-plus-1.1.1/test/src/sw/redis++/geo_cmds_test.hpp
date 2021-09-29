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

#ifndef SEWENEW_REDISPLUSPLUS_TEST_GEO_CMDS_TEST_HPP
#define SEWENEW_REDISPLUSPLUS_TEST_GEO_CMDS_TEST_HPP

#include <vector>
#include <tuple>
#include "utils.h"

namespace sw {

namespace redis {

namespace test {

template <typename RedisInstance>
void GeoCmdTest<RedisInstance>::run() {
    auto key = test_key("geo");
    auto dest = test_key("dest");

    KeyDeleter<RedisInstance> deleter(_redis, {key, dest});

    auto members = {
        std::make_tuple("m1", 10.0, 11.0),
        std::make_tuple("m2", 10.1, 11.1),
        std::make_tuple("m3", 10.2, 11.2)
    };

    REDIS_ASSERT(_redis.geoadd(key, std::make_tuple("m1", 10.0, 11.0)) == 1,
            "failed to test geoadd");
    REDIS_ASSERT(_redis.geoadd(key, members) == 2, "failed to test geoadd");

    auto dist = _redis.geodist(key, "m1", "m4", GeoUnit::KM);
    REDIS_ASSERT(!dist, "failed to test geodist with nonexistent member");

    std::vector<OptionalString> hashes;
    _redis.geohash(key, {"m1", "m4"}, std::back_inserter(hashes));
    REDIS_ASSERT(hashes.size() == 2, "failed to test geohash");
    REDIS_ASSERT(bool(hashes[0]) && *(hashes[0]) == "s1zned3z8u0" && !(hashes[1]),
            "failed to test geohash");
    hashes.clear();
    _redis.geohash(key, {"m4"}, std::back_inserter(hashes));
    REDIS_ASSERT(hashes.size() == 1 && !(hashes[0]), "failed to test geohash");

    std::vector<Optional<std::pair<double, double>>> pos;
    _redis.geopos(key, {"m4"}, std::back_inserter(pos));
    REDIS_ASSERT(pos.size() == 1 && !(pos[0]), "failed to test geopos");

    auto num = _redis.georadius(key,
                                std::make_pair(10.1, 11.1),
                                100,
                                GeoUnit::KM,
                                dest,
                                false,
                                10);
    REDIS_ASSERT(bool(num) && *num == 3, "failed to test georadius with store option");

    std::vector<std::string> mems;
    _redis.georadius(key,
                    std::make_pair(10.1, 11.1),
                    100,
                    GeoUnit::KM,
                    10,
                    true,
                    std::back_inserter(mems));
    REDIS_ASSERT(mems.size() == 3, "failed to test georadius with no option");

    std::vector<std::tuple<std::string, double>> with_dist;
    _redis.georadius(key,
                    std::make_pair(10.1, 11.1),
                    100,
                    GeoUnit::KM,
                    10,
                    true,
                    std::back_inserter(with_dist));
    REDIS_ASSERT(with_dist.size() == 3, "failed to test georadius with dist");

    std::vector<std::tuple<std::string, double, std::pair<double, double>>> with_dist_coord;
    _redis.georadius(key,
                    std::make_pair(10.1, 11.1),
                    100,
                    GeoUnit::KM,
                    10,
                    true,
                    std::back_inserter(with_dist_coord));
    REDIS_ASSERT(with_dist_coord.size() == 3, "failed to test georadius with dist and coord");

    num = _redis.georadiusbymember(key,
                                    "m1",
                                    100,
                                    GeoUnit::KM,
                                    dest,
                                    false,
                                    10);
    REDIS_ASSERT(bool(num) && *num == 3, "failed to test georadiusbymember with store option");

    mems.clear();
    _redis.georadiusbymember(key,
                            "m1",
                            100,
                            GeoUnit::KM,
                            10,
                            true,
                            std::back_inserter(mems));
    REDIS_ASSERT(mems.size() == 3, "failed to test georadiusbymember with no option");

    with_dist.clear();
    _redis.georadiusbymember(key,
                            "m1",
                            100,
                            GeoUnit::KM,
                            10,
                            true,
                            std::back_inserter(with_dist));
    REDIS_ASSERT(with_dist.size() == 3, "failed to test georadiusbymember with dist");

    with_dist_coord.clear();
    _redis.georadiusbymember(key,
                            "m1",
                            100,
                            GeoUnit::KM,
                            10,
                            true,
                            std::back_inserter(with_dist_coord));
    REDIS_ASSERT(with_dist_coord.size() == 3,
            "failed to test georadiusbymember with dist and coord");
}

}

}

}

#endif // end SEWENEW_REDISPLUSPLUS_TEST_GEO_CMDS_TEST_HPP
