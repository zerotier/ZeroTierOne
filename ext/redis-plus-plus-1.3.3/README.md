# redis-plus-plus

[![Build Status](https://travis-ci.org/sewenew/redis-plus-plus.svg?branch=master)](https://travis-ci.org/sewenew/redis-plus-plus)

- [Overview](#overview)
    - [Features](#features)
    - [Branches](#branches)
- [Installation](#installation)
    - [Install hiredis](#install-hiredis)
    - [Install redis-plus-plus](#install-redis-plus-plus)
    - [Run Tests (Optional)](#run-tests-optional)
    - [Use redis-plus-plus In Your Project](#use-redis-plus-plus-in-your-project)
- [Getting Started](#getting-started)
- [API Reference](#api-reference)
    - [Connection](#connection)
    - [Send Command to Redis Server](#send-command-to-redis-server)
    - [Exception](#exception)
    - [Generic Command Interface](#generic-command-interface)
    - [Publish/Subscribe](#publishsubscribe)
    - [Pipeline](#pipeline)
    - [Transaction](#transaction)
    - [Redis Cluster](#redis-cluster)
    - [Redis Sentinel](#redis-sentinel)
    - [Redis Stream](#redis-stream)
    - [Redis Modules](#redis-modules)
    - [Async Interface](#async-interface)
- [Redis Recipes](#redis-recipes)
    - [Redlock](#redlock)
- [Author](#author)

## Overview

This is a C++ client library for Redis. It's based on [hiredis](https://github.com/redis/hiredis), and is compatible with C++ 17, C++ 14, and C++ 11.

**NOTE**: I'm not a native speaker. So if the documentation is unclear, please feel free to open an issue or pull request. I'll response ASAP.

### Features
- Most commands for Redis.
- Connection pool.
- Redis scripting.
- Thread safe unless otherwise stated.
- Redis publish/subscribe.
- Redis pipeline.
- Redis transaction.
- Redis Cluster.
- Redis Sentinel.
- STL-like interfaces.
- Generic command interface.
- Redis Stream.
- Redlock.
- Redis ACL.
- TLS/SSL support.

### Branches

The master branch is the stable branch, which passes all tests. The dev branch is unstable. If you want to contribute, please create pull request on dev branch.

## Installation

### Install hiredis

Since *redis-plus-plus* is based on *hiredis*, you should install *hiredis* first. The minimum version requirement for *hiredis* is **v0.12.1**. However, [the latest stable release](https://github.com/redis/hiredis/releases) of *hiredis* is always recommended.

**NOTE**: You must ensure that there's only 1 version of hiredis is installed. Otherwise, you might get some wired problems. Check the following issues for example: [issue 135](https://github.com/sewenew/redis-plus-plus/issues/135), [issue 140](https://github.com/sewenew/redis-plus-plus/issues/140) and [issue 158](https://github.com/sewenew/redis-plus-plus/issues/158).

Normally, you can install *hiredis* with a C++ package manager, and that's the easiest way to do it, e.g. `sudo apt-get install libhiredis-dev`. However, if you want to install the latest code of hiredis, or a specified version (e.g. async support needs hiredis v1.0.0 or later), you can install it from source.

Note again: DO NOT INSTALL MULTIPLE VERSIONS OF HIREDIS.

```
git clone https://github.com/redis/hiredis.git

cd hiredis

make

make install
```

By default, *hiredis* is installed at */usr/local*. If you want to install *hiredis* at non-default location, use the following commands to specify the installation path.

```
make PREFIX=/non/default/path

make PREFIX=/non/default/path install
```

### Install redis-plus-plus

*redis-plus-plus* is built with [CMAKE](https://cmake.org).

```
git clone https://github.com/sewenew/redis-plus-plus.git

cd redis-plus-plus

mkdir build

cd build

cmake -DREDIS_PLUS_PLUS_CXX_STANDARD=17 ..

make

make install

cd ..
```

If *hiredis* is installed at non-default location, you should use `CMAKE_PREFIX_PATH` to specify the installation path of *hiredis*. By default, *redis-plus-plus* is installed at */usr/local*. However, you can use `CMAKE_INSTALL_PREFIX` to install *redis-plus-plus* at non-default location.

```
cmake -DCMAKE_PREFIX_PATH=/path/to/hiredis -DCMAKE_INSTALL_PREFIX=/path/to/install/redis-plus-plus ..
```

Since version 1.3.0, by default, *redis-plus-plus* is built with the `-std=c++17` standard. So that we can use the [std::string_view](#stringview) and [std::optional](#optional) features. However, it can also be built with the `-std=c++11` or `-std=c++14` standard, and in that case, we have our own simple implementation of `std::string_view` and `std::optional`. In order to explicitly specify c++ standard, you can use the following cmake flag: `-DREDIS_PLUS_PLUS_CXX_STANDARD=11`.

```
cmake -DCMAKE_PREFIX_PATH=/path/to/hiredis -DCMAKE_INSTALL_PREFIX=/path/to/install/redis-plus-plus -DREDIS_PLUS_PLUS_CXX_STANDARD=11 ..
```

**NOTE**: You should build *redis-plus-plus* and your application with the same standard, e.g. if you build *redis-plus-plus* with C++17 standard, you MUST also build your application code with C++17 standard.

When compiling *redis-plus-plus*, it also compiles a test program, which might take a while. However, you can disable building test with the following cmake option: `-DREDIS_PLUS_PLUS_BUILD_TEST=OFF`.

```
cmake -DCMAKE_PREFIX_PATH=/path/to/hiredis -DCMAKE_INSTALL_PREFIX=/path/to/install/redis-plus-plus -DREDIS_PLUS_PLUS_BUILD_TEST=OFF ..
```

By default, *redis-plus-plus* builds both a static library and a shared library. If you only want to build one of them, you can disable the other with `-DREDIS_PLUS_PLUS_BUILD_STATIC=OFF` or `-DREDIS_PLUS_PLUS_BUILD_SHARED=OFF`.

*redis-plus-plus* builds static library with `-fPIC` option, i.e. Position Independent Code, by default. However, you can disable it with `-DREDIS_PLUS_PLUS_BUILD_STATIC_WITH_PIC=OFF`.

#### Windows Support

Now *hiredis* has Windows support, and since Visual Studio 2017, Visual Studio has built-in support for CMake. So *redis-plus-plus* also supports Windows platform now. It has been fully tested with Visual Studio 2017 and later on Win 10. I'm not familiar with Visual Studio environment, and the following doc might not be accurate. If you're familiar with the Windows platform, feel free to update this doc on how to install *redis-plus-plus* on Windows.

##### CMake Support On Visual Studio

The following are some links on how to build CMake project with Visual Studio 2017 or later. If you're not familiar with it, you'd better read these instructions first:

- [CMake support in Visual Studio](https://devblogs.microsoft.com/cppblog/cmake-support-in-visual-studio/)
- [CMake projects in Visual Studio](https://docs.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio?view=vs-2017)
- [CMakeSettings.json schema reference](https://docs.microsoft.com/en-us/cpp/build/cmakesettings-reference?view=vs-2017)
- [Open a project from a GitHub repo](https://docs.microsoft.com/en-us/visualstudio/get-started/tutorial-open-project-from-repo?view=vs-2019#open-a-project-from-a-github-repo)

**NOTE**: IMHO, Visual Studio 2017's support for CMake project is not very mature, and I recommend you to build *hiredis* and *redis-plus-plus with Visual Studio 2019.

##### Build hiredis

First of all, you need to get the latest code of *hiredis* on master branch. Older version might not support Windows platform. *hiredis*' CMakeLists.txt uses `add_compile_definitions` method, which is only supported by cmake 3.12 or later. However, Visual Studio 2017's cmake version is older than that. So if you're using Visual Studio 2017, you need to comment the following line in the CMakeLists.txt file:

```
#IF(WIN32)
#    ADD_COMPILE_DEFINITIONS(_CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN)
#ENDIF()
```

You can use the **Open Folder** feature to open *hiredis* project, and build it with the instructions (links) mentioned above.

##### Build redis-plus-plus

Since *redis-plus-plus* depends on *hiredis*, we need to specify the installation paths of *hiredis* before building it. You can use the **Open Folder** feature to open *redis-plus-plus* project. You need to edit the *CMakeSetting.json* file (automatically generated by Visual Studio) to set *HIREDIS_HEADER*, *HIREDIS_LIB* and *TEST_HIREDIS_LIB* variables to specify the installation path of hiredis headers, installation path of hiredis dynamic library and installation path of hiredis static library. The following is an example of *CMakeSetting.json* file:

```
{
    "configurations": [
      {
        "name": "x64-Release",
        "generator": "Visual Studio 15 2017 Win64",
        "configurationType": "Release",
        "buildRoot": "${env.LOCALAPPDATA}\\CMakeBuild\\${workspaceHash}\\build\\${name}",
        "cmakeCommandArgs": "",
        "buildCommandArgs": "-m -v:minimal",
        "variables": [
          {
            "name": "HIREDIS_HEADER",
            "value": "installation path of hiredis header files",
            "type": "PATH"
          },
          {
            "name": "HIREDIS_LIB",
            "value": "installation path of dynamic library of hiredis",
            "type": "FILEPATH"
          },
          {
            "name": "TEST_HIREDIS_LIB",
            "value": "installation path of static library of hiredis",
            "type": "FILEPATH"
          }
        ]
      }
    ]
}
```

Then you can build it the instructions (links) mentioned above. If you're building with Visual Studio 2017 in debug mode, you might get [/bigobj error](https://docs.microsoft.com/en-us/cpp/build/reference/bigobj-increase-number-of-sections-in-dot-obj-file?view=vs-2017) when building the test. In this case, you can disable building test by setting `-DREDIS_PLUS_PLUS_BUILD_TEST=OFF` or build it in Release mode.

**NOTE**:

- Since 1.3.0, *redis-puls-plus* is built with C++17 by default, and you should also set your application code to be built with C++17. If you still want to build the *redis-plus-plus* with C++11, you can set the `REDIS_PLUS_PLUS_CXX_STANDARD` cmake option to 11.
- TLS/SSL support has not been tested on Windows yet.

##### The Order of Header Files

On Windows platform, if your application code also needs to include *windows.h*. You must ensure that *sw/redis++/redis++.h* is included before *windows.h*. Check [this issue](https://github.com/sewenew/redis-plus-plus/issues/194) for detail.

#### Building a redis-plus-plus Debian Package (Optional)

Basic support for building a GNU/Debian package is supplied with the use of cmake.
The following example shows how to build the Debian package:

```
mkdir build; cd build
cmake ..
cpack -G DEB
```

The install prefix may be modified as follows:

```
mkdir build; cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
cpack -G DEB
```

### Run Tests (Optional)

*redis-plus-plus* has been fully tested with the following compilers:

```
gcc version 4.8.5 20150623 (Red Hat 4.8.5-39) (GCC)
gcc version 5.5.0 20171010 (Ubuntu 5.5.0-12ubuntu1)
gcc version 6.5.0 20181026 (Ubuntu 6.5.0-2ubuntu1~18.04)
gcc version 7.4.0 (Ubuntu 7.4.0-1ubuntu1~18.04.1)
gcc version 8.3.0 (Ubuntu 8.3.0-6ubuntu1~18.04.1)
gcc version 9.2.1 20191008 (Ubuntu 9.2.1-9ubuntu2)
gcc version 10.2.1 20210110 (Debian 10.2.1-6)
clang version 3.9.1-19ubuntu1 (tags/RELEASE_391/rc2)
clang version 4.0.1-10 (tags/RELEASE_401/final)
clang version 5.0.1-4 (tags/RELEASE_501/final)
clang version 6.0.0-1ubuntu2 (tags/RELEASE_600/final)
clang version 7.0.0-3~ubuntu0.18.04.1 (tags/RELEASE_700/final)
clang version 8.0.1-3build1 (tags/RELEASE_801/final)
Apple clang version 11.0.0 (clang-1100.0.33.12)
Visual Studio 2017 (Win 10)
Visual Studio 2019 (Win 10)
```

If you build *redis-plus-plus* with `-DREDIS_PLUS_PLUS_BUILD_TEST=ON` (the default behavior, and you can disable building test with `-DREDIS_PLUS_PLUS_BUILD_TEST=OFF`), you'll get a test program in *compile/test* directory: *compile/test/test_redis++*.

In order to run the tests, you need to set up a Redis instance, and a Redis Cluster. Since the test program will send most of Redis commands to the server and cluster, you need to set up Redis of the latest version (by now, it's 5.0). Otherwise, the tests might fail. For example, if you set up Redis 4.0 for testing, the test program will fail when it tries to send the `ZPOPMAX` command (a Redis 5.0 command) to the server. If you want to run the tests with other Redis versions, you have to comment out commands that haven't been supported by your Redis, from test source files in *redis-plus-plus/test/src/sw/redis++/* directory. Sorry for the inconvenience, and I'll fix this problem to make the test program work with any version of Redis in the future.

**NOTE**: The latest version of Redis is only a requirement for running the tests. In fact, you can use *redis-plus-plus* with Redis of any version, e.g. Redis 2.0, Redis 3.0, Redis 4.0, Redis 5.0.

**NEVER** run the test program in production envronment, since the keys, which the test program reads or writes, might conflict with your application.

In order to run tests with both Redis and Redis Cluster, you can run the test program with the following command:

```
./compile/test/test_redis++ -h host -p port -a auth -n cluster_node -c cluster_port
```

- *host* and *port* are the host and port number of the Redis instance.
- *cluster_node* and *cluster_port* are the host and port number of Redis Cluster. You only need to set the host and port number of a single node in the cluster, *redis-plus-plus* will find other nodes automatically.
- *auth* is the password of the Redis instance and Redis Cluster. The Redis instance and Redis Cluster must be configured with the same password. If there's no password configured, don't set this option.

If you only want to run tests with Redis, you only need to specify *host*, *port* and *auth* options:

```
./compile/test/test_redis++ -h host -p port -a auth
```

Similarly, if you only want to run tests with Redis Cluster, just specify *cluster_node*, *cluster_port* and *auth* options:

```
./compile/test/test_redis++ -a auth -n cluster_node -c cluster_port
```

By default, the test program will not test running *redis-plus-plus* in multi-threads environment. If you want to do multi-threads test, which might cost a long time, you can specify the *-m* option:

```
./compile/test/test_redis++ -h host -p port -a auth -n cluster_node -c cluster_port -m
```

If all tests have been passed, the test program will print the following message:

```
Pass all tests
```

Otherwise, it prints the error message.

#### Performance

*redis-plus-plus* runs as fast as *hiredis*, since it's a wrapper of *hiredis*. You can run *test_redis++* in benchmark mode to check the performance in your environment.

```
./compile/test/test_redis++ -h host -p port -a auth -n cluster_node -c cluster_port -b -t thread_num -s connection_pool_size -r request_num -k key_len -v val_len
```

- *-b* option turns the test program into benchmark mode.
- *thread_num* specifies the number of worker threads. `10` by default.
- *connection_pool_size* specifies the size of the connection pool. `5` by default.
- *request_num* specifies the total number of requests sent to server for each test. `100000` by default.
- *key_len* specifies the length of the key for each operation. `10` by default.
- *val_len* specifies the length of the value. `10` by default.

The bechmark will generate `100` random binary keys for testing, and the size of these keys is specified by *key_len*. When the benchmark runs, it will read/write with these keys. So **NEVER** run the test program in your production environment, otherwise, it might inaccidently delete your data.

### Use redis-plus-plus In Your Project

After compiling the code, you'll get both shared library and static library. Since *redis-plus-plus* depends on *hiredis*, you need to link both libraries to your Application. Also don't forget to specify the c++ standard, `-std=c++17`, `-std=c++14` or `-std=c++11`, as well as the thread-related option.

#### Use Static Libraries

Take gcc as an example.

```
g++ -std=c++17 -o app app.cpp /path/to/libredis++.a /path/to/libhiredis.a -pthread
```

If *hiredis* and *redis-plus-plus* are installed at non-default location, you should use `-I` option to specify the header path.

```
g++ -std=c++17 -I/non-default/install/include/path -o app app.cpp /path/to/libredis++.a /path/to/libhiredis.a -pthread
```

#### Use Shared Libraries

```
g++ -std=c++17 -o app app.cpp -lredis++ -lhiredis -pthread
```

If *hiredis* and *redis-plus-plus* are installed at non-default location, you should use `-I` and `-L` options to specify the header and library paths.

```
g++ -std=c++17 -I/non-default/install/include/path -L/non-default/install/lib/path -o app app.cpp -lredis++ -lhiredis -pthread
```

When linking with shared libraries, and running your application, you might get the following error message:

```
error while loading shared libraries: xxx: cannot open shared object file: No such file or directory.
```

That's because the linker cannot find the shared libraries. In order to solve the problem, you can add the path where you installed *hiredis* and *redis-plus-plus* libraries, to `LD_LIBRARY_PATH` environment variable. For example:

```
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
```

Check [this StackOverflow question](https://stackoverflow.com/questions/480764) for details on how to solve the problem.

#### Build With Cmake

If you're using cmake to build your application, you need to add *hiredis* and *redis-plus-plus* dependencies in your *CMakeLists.txt*:

```CMake
# <---------- set c++ standard ------------->
# NOTE: you must build redis-plus-plus and your application code with the same standard.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# <------------ add hiredis dependency --------------->
find_path(HIREDIS_HEADER hiredis)
target_include_directories(target PUBLIC ${HIREDIS_HEADER})

find_library(HIREDIS_LIB hiredis)
target_link_libraries(target ${HIREDIS_LIB})

# <------------ add redis-plus-plus dependency -------------->
# NOTE: this should be *sw* NOT *redis++*
find_path(REDIS_PLUS_PLUS_HEADER sw)
target_include_directories(target PUBLIC ${REDIS_PLUS_PLUS_HEADER})

find_library(REDIS_PLUS_PLUS_LIB redis++)
target_link_libraries(target ${REDIS_PLUS_PLUS_LIB})
```

See [this issue](https://github.com/sewenew/redis-plus-plus/issues/5) for a complete example of *CMakeLists.txt*.

Also, if you installed *hiredis* and *redis-plus-plus* at non-default location, you need to run cmake with `CMAKE_PREFIX_PATH` option to specify the installation path of these two libraries.

```
cmake -DCMAKE_PREFIX_PATH=/installation/path/to/the/two/libs ..
```

## Getting Started

```C++
#include <sw/redis++/redis++.h>

using namespace sw::redis;

try {
    // Create an Redis object, which is movable but NOT copyable.
    auto redis = Redis("tcp://127.0.0.1:6379");

    // ***** STRING commands *****

    redis.set("key", "val");
    auto val = redis.get("key");    // val is of type OptionalString. See 'API Reference' section for details.
    if (val) {
        // Dereference val to get the returned value of std::string type.
        std::cout << *val << std::endl;
    }   // else key doesn't exist.

    // ***** LIST commands *****

    // std::vector<std::string> to Redis LIST.
    std::vector<std::string> vec = {"a", "b", "c"};
    redis.rpush("list", vec.begin(), vec.end());

    // std::initializer_list to Redis LIST.
    redis.rpush("list", {"a", "b", "c"});

    // Redis LIST to std::vector<std::string>.
    vec.clear();
    redis.lrange("list", 0, -1, std::back_inserter(vec));

    // ***** HASH commands *****

    redis.hset("hash", "field", "val");

    // Another way to do the same job.
    redis.hset("hash", std::make_pair("field", "val"));

    // std::unordered_map<std::string, std::string> to Redis HASH.
    std::unordered_map<std::string, std::string> m = {
        {"field1", "val1"},
        {"field2", "val2"}
    };
    redis.hmset("hash", m.begin(), m.end());

    // Redis HASH to std::unordered_map<std::string, std::string>.
    m.clear();
    redis.hgetall("hash", std::inserter(m, m.begin()));

    // Get value only.
    // NOTE: since field might NOT exist, so we need to parse it to OptionalString.
    std::vector<OptionalString> vals;
    redis.hmget("hash", {"field1", "field2"}, std::back_inserter(vals));

    // ***** SET commands *****

    redis.sadd("set", "m1");

    // std::unordered_set<std::string> to Redis SET.
    std::unordered_set<std::string> set = {"m2", "m3"};
    redis.sadd("set", set.begin(), set.end());

    // std::initializer_list to Redis SET.
    redis.sadd("set", {"m2", "m3"});

    // Redis SET to std::unordered_set<std::string>.
    set.clear();
    redis.smembers("set", std::inserter(set, set.begin()));

    if (redis.sismember("set", "m1")) {
        std::cout << "m1 exists" << std::endl;
    }   // else NOT exist.

    // ***** SORTED SET commands *****

    redis.zadd("sorted_set", "m1", 1.3);

    // std::unordered_map<std::string, double> to Redis SORTED SET.
    std::unordered_map<std::string, double> scores = {
        {"m2", 2.3},
        {"m3", 4.5}
    };
    redis.zadd("sorted_set", scores.begin(), scores.end());

    // Redis SORTED SET to std::vector<std::pair<std::string, double>>.
    // NOTE: The return results of zrangebyscore are ordered, if you save the results
    // in to `std::unordered_map<std::string, double>`, you'll lose the order.
    std::vector<std::pair<std::string, double>> zset_result;
    redis.zrangebyscore("sorted_set",
            UnboundedInterval<double>{},            // (-inf, +inf)
            std::back_inserter(zset_result));

    // Only get member names:
    // pass an inserter of std::vector<std::string> type as output parameter.
    std::vector<std::string> without_score;
    redis.zrangebyscore("sorted_set",
            BoundedInterval<double>(1.5, 3.4, BoundType::CLOSED),   // [1.5, 3.4]
            std::back_inserter(without_score));

    // Get both member names and scores:
    // pass an back_inserter of std::vector<std::pair<std::string, double>> as output parameter.
    std::vector<std::pair<std::string, double>> with_score;
    redis.zrangebyscore("sorted_set",
            BoundedInterval<double>(1.5, 3.4, BoundType::LEFT_OPEN),    // (1.5, 3.4]
            std::back_inserter(with_score));

    // ***** SCRIPTING commands *****

    // Script returns a single element.
    auto num = redis.eval<long long>("return 1", {}, {});

    // Script returns an array of elements.
    std::vector<long long> nums;
    redis.eval("return {ARGV[1], ARGV[2]}", {}, {"1", "2"}, std::back_inserter(nums));

    // mset with TTL
    auto mset_with_ttl_script = R"(
        local len = #KEYS
        if (len == 0 or len + 1 ~= #ARGV) then return 0 end
        local ttl = tonumber(ARGV[len + 1])
        if (not ttl or ttl <= 0) then return 0 end
        for i = 1, len do redis.call("SET", KEYS[i], ARGV[i], "EX", ttl) end
        return 1
    )";

    // Set multiple key-value pairs with TTL of 60 seconds.
    auto keys = {"key1", "key2", "key3"};
    std::vector<std::string> args = {"val1", "val2", "val3", "60"};
    redis.eval<long long>(mset_with_ttl_script, keys.begin(), keys.end(), vals.begin(), vals.end());

    // ***** Pipeline *****

    // Create a pipeline.
    auto pipe = redis.pipeline();

    // Send mulitple commands and get all replies.
    auto pipe_replies = pipe.set("key", "value")
                            .get("key")
                            .rename("key", "new-key")
                            .rpush("list", {"a", "b", "c"})
                            .lrange("list", 0, -1)
                            .exec();

    // Parse reply with reply type and index.
    auto set_cmd_result = pipe_replies.get<bool>(0);

    auto get_cmd_result = pipe_replies.get<OptionalString>(1);

    // rename command result
    pipe_replies.get<void>(2);

    auto rpush_cmd_result = pipe_replies.get<long long>(3);

    std::vector<std::string> lrange_cmd_result;
    pipe_replies.get(4, back_inserter(lrange_cmd_result));

    // ***** Transaction *****

    // Create a transaction.
    auto tx = redis.transaction();

    // Run multiple commands in a transaction, and get all replies.
    auto tx_replies = tx.incr("num0")
                        .incr("num1")
                        .mget({"num0", "num1"})
                        .exec();

    // Parse reply with reply type and index.
    auto incr_result0 = tx_replies.get<long long>(0);

    auto incr_result1 = tx_replies.get<long long>(1);

    std::vector<OptionalString> mget_cmd_result;
    tx_replies.get(2, back_inserter(mget_cmd_result));

    // ***** Generic Command Interface *****

    // There's no *Redis::client_getname* interface.
    // But you can use *Redis::command* to get the client name.
    val = redis.command<OptionalString>("client", "getname");
    if (val) {
        std::cout << *val << std::endl;
    }

    // Same as above.
    auto getname_cmd_str = {"client", "getname"};
    val = redis.command<OptionalString>(getname_cmd_str.begin(), getname_cmd_str.end());

    // There's no *Redis::sort* interface.
    // But you can use *Redis::command* to send sort the list.
    std::vector<std::string> sorted_list;
    redis.command("sort", "list", "ALPHA", std::back_inserter(sorted_list));

    // Another *Redis::command* to do the same work.
    auto sort_cmd_str = {"sort", "list", "ALPHA"};
    redis.command(sort_cmd_str.begin(), sort_cmd_str.end(), std::back_inserter(sorted_list));

    // ***** Redis Cluster *****

    // Create a RedisCluster object, which is movable but NOT copyable.
    auto redis_cluster = RedisCluster("tcp://127.0.0.1:7000");

    // RedisCluster has similar interfaces as Redis.
    redis_cluster.set("key", "value");
    val = redis_cluster.get("key");
    if (val) {
        std::cout << *val << std::endl;
    }   // else key doesn't exist.

    // Keys with hash-tag.
    redis_cluster.set("key{tag}1", "val1");
    redis_cluster.set("key{tag}2", "val2");
    redis_cluster.set("key{tag}3", "val3");

    std::vector<OptionalString> hash_tag_res;
    redis_cluster.mget({"key{tag}1", "key{tag}2", "key{tag}3"},
            std::back_inserter(hash_tag_res));

} catch (const Error &e) {
    // Error handling.
}
```

## API Reference

You can also see [redis.h](https://github.com/sewenew/redis-plus-plus/blob/master/src/sw/redis%2B%2B/redis.h) for doxygen style documentation.

### Connection

`Redis` class maintains a connection pool to Redis server. If the connection is broken, `Redis` reconnects to Redis server automatically.

You can initialize a `Redis` instance with `ConnectionOptions` and `ConnectionPoolOptions`. `ConnectionOptions` specifies options for connection to Redis server, and `ConnectionPoolOptions` specifies options for conneciton pool. `ConnectionPoolOptions` is optional. If not specified, `Redis` maintains a single connection to Redis server.

```C++
ConnectionOptions connection_options;
connection_options.host = "127.0.0.1";  // Required.
connection_options.port = 6666; // Optional. The default port is 6379.
connection_options.password = "auth";   // Optional. No password by default.
connection_options.db = 1;  // Optional. Use the 0th database by default.

// Optional. Timeout before we successfully send request to or receive response from redis.
// By default, the timeout is 0ms, i.e. never timeout and block until we send or receive successfuly.
// NOTE: if any command is timed out, we throw a TimeoutError exception.
connection_options.socket_timeout = std::chrono::milliseconds(200);

// Connect to Redis server with a single connection.
Redis redis1(connection_options);

ConnectionPoolOptions pool_options;
pool_options.size = 3;  // Pool size, i.e. max number of connections.

// Optional. Max time to wait for a connection. 0ms by default, which means wait forever.
// Say, the pool size is 3, while 4 threds try to fetch the connection, one of them will be blocked.
pool_options.wait_timeout = std::chrono::milliseconds(100);

// Optional. Max lifetime of a connection. 0ms by default, which means never expire the connection.
// If the connection has been created for a long time, i.e. more than `connection_lifetime`,
// it will be expired and reconnected.
pool_options.connection_lifetime = std::chrono::minutes(10);

// Connect to Redis server with a connection pool.
Redis redis2(connection_options, pool_options);
```

**NOTE**: if you set `ConnectionOptions::socket_timeout`, and try to call blocking commands, e.g. `Redis::brpop`, `Redis::blpop`, `Redis::bzpopmax`, `Redis::bzpopmin`, you must ensure that `ConnectionOptions::socket_timeout` is larger than the timeout specified with these blocking commands. Otherwise, you might get `TimeoutError`, and lose message.

See [ConnectionOptions](https://github.com/sewenew/redis-plus-plus/blob/master/src/sw/redis%2B%2B/connection.h#L40) and [ConnectionPoolOptions](https://github.com/sewenew/redis-plus-plus/blob/master/src/sw/redis%2B%2B/connection_pool.h#L30) for more options. Also see [issue 80](https://github.com/sewenew/redis-plus-plus/issues/80) for discussion on connection pool.

**NOTE**: `Redis` class is movable but NOT copyable.

```C++
// auto redis3 = redis1;    // this won't compile.

// But it's movable.
auto redis3 = std::move(redis1);
```

*redis-plus-plus* also supports connecting to Redis server with Unix Domain Socket.

```C++
ConnectionOptions options;
options.type = ConnectionType::UNIX;
options.path = "/path/to/socket";
Redis redis(options);
```

You can also connect to Redis server with a URI:

```
tcp://[[username:]password@]host[:port][/db]

unix://[[username:]password@]path-to-unix-domain-socket[/db]
```

The *scheme* and *host* parts are required, and others are optional. If you're connecting to Redis with Unix Domain Socket, you should use the *unix* scheme, otherwise, you should use *tcp* scheme. The following is a list of default values for those optional parts:

- username: *default*
- password: empty string, i.e. no password
- port: 6379
- db: 0

**NOTE**: If your password or username contains '@', or your username contains ':', you cannot construct `Redis` object with URI. Because *redis-plus-plus* will incorrectly parse the URI. In this case, you need to use `ConnectionOptions` to construct `Redis` object.

**NOTE**: [Redis 6.0 supports ACL](https://redis.io/topics/acl), and you can specify a username for the connection. However, before Redis 6.0, you cannot do that.

Also, the following connection options can be specified with the query string of URI, e.g. *tcp://127.0.0.1?keep_alive=true&socket_timeout=100ms&connect_timeout=100ms*:

- `ConnectionOptions::keep_alive`: *false* by default.
- `ConnectionOptions::socket_timeout`: *0ms* by default.
- `ConnectionOptions::connect_timeout`: *0ms* by default.

**NOTE**: Options specified in query string are case-sensitive, i.e. all key-value pairs must be in lowercase.

So far, you cannot specify connection pool options with URI, e.g. `ConnectionPoolOptions::size`.

```C++
// Single connection to the given host and port.
Redis redis1("tcp://127.0.0.1:6666");

// Use default port, i.e. 6379.
Redis redis2("tcp://127.0.0.1");

// Connect to Redis with password, and default port.
Redis redis3("tcp://pass@127.0.0.1");

// Connect to Redis and select the 2nd (db number starts from 0) database.
Redis redis4("tcp://127.0.0.1:6379/2");

// Set keep_alive option to true with query string.
Redis redis5("tcp://127.0.0.1:6379/2?keep_alive=true");

// Set socket_timeout to 50 milliseconds, and connect_timeout to 1 second with query string.
Redis redis6("tcp://127.0.0.1?socket_timeout=50ms&connect_timeout=1s");

// Connect to Unix Domain Socket.
Redis redis7("unix://path/to/socket");
```

#### Lazily Create Connection

Connections in the pool are lazily created. When the connection pool is initialized, i.e. the constructor of `Redis`, `Redis` does NOT connect to the server. Instead, it connects to the server only when you try to send command. In this way, we can avoid unnecessary connections. So if the pool size is 5, but the number of max concurrent connections is 3, there will be only 3 connections in the pool.

#### Connection Failure

You don't need to check whether `Redis` object connects to server successfully. If `Redis` fails to create a connection to Redis server, or the connection is broken at some time, it throws an exception of type `Error` when you try to send command with `Redis`. Even when you get an exception, i.e. the connection is broken, you don't need to create a new `Redis` object. You can reuse the `Redis` object to send commands, and the `Redis` object will try to reconnect to server automatically. If it reconnects successfully, it sends command to server. Otherwise, it throws an exception again.

See the [Exception section](#exception) for details on exceptions.

#### Reuse Redis object As Much As Possible

It's NOT cheap to create a `Redis` object, since it will create new connections to Redis server. So you'd better reuse `Redis` object as much as possible. Also, it's safe to call `Redis`' member functions in multi-thread environment, and you can share `Redis` object in multiple threads.

```C++
// This is GOOD practice.
auto redis = Redis("tcp://127.0.0.1");
for (auto idx = 0; idx < 100; ++idx) {
    // Reuse the Redis object in the loop.
    redis.set("key", "val");
}

// This is VERY BAD! It's very inefficient.
// NEVER DO IT!!!
for (auto idx = 0; idx < 100; ++idx) {
    // Create a new Redis object for each iteration.
    auto redis = Redis("tcp://127.0.0.1");
    redis.set("key", "val");
}
```

#### TLS/SSL Support

*redis-plus-plus* also has TLS support. However, in order to use this feature, you need to enable it when building *hiredis* and *redis-plus-plus*.

**NOTE**: So far, TLS feature has not been tested on Windows platform. I'll fix it in the future.

##### Enable TLS/SSL support

When building *hiredis* with TLS support, you need to download *hiredis* of version *v1.0.0* or latter, and specify `USE_SSL=1` flag:

```
make PREFIX=/non/default/path USE_SSL=1

make PREFIX=/non/default/path USE_SSL=1 install
```

Then you can build *redis-plus-plus* to enable TLS support by specifying the `-DREDIS_PLUS_PLUS_USE_TLS=ON` option:

```
cmake -DREDIS_PLUS_PLUS_USE_TLS=ON ..
```

##### Connection Options

In order to connect to Redis with TLS support, you need to specify the following connection options:

```
ConnectionOptions opts;
opts.host = "127.0.0.1";
opts.port = 6379;

opts.tls.enabled = true;    // Required. `false` by default.
opts.tls.cert = "/path/to/client/certificate";  // Optional
opts.tls.key = "/path/to/private/key/file"; // Optional
opts.tls.cacert = "/path/to/CA/certificate/file";   // You can also set `opts.tls.cacertdir` instead.
opts.tls.sni = "server-name-indication";    // Optional
```

Although `tls.cert` and `tls.key` are optional, if you specify one of them, you must also specify the other. Instead of specifying `tls.cacert`, you can also specify `tls.cacertdir` to the directory where certificates are stored.

These options are the same as `redis-cli`'s TLS related command line arguments, so you can also run `redis-cli --help` to get the detailed explanation of these options.

Then you can use this `ConnectionOptions` to create a `Redis` object to connect to Redis server with TLS support.

**NOTE**: When building your application code, you also need to link it with `libhiredis.a`, `libhiredis_ssl.a`, `libredis++.a` (or the corresponding shared libraries), `-lssl` and `-lcrypto`.

##### Automatically Initialize OpenSSL Library

By default, *redis-plus-plus* automatically initializes OpenSSL library, i.e. calls `SSL_library_init` and initializes locks if needed. However, your application code might already initialize OpenSSL library. In this case, you can call `tls::disable_auto_init()` to disable the initialization. You should call this function only once and call it before any other *redis-plus-plus* operation. Otherwise, the behavior is undefined.

### Send Command to Redis Server

You can send [Redis commands](https://redis.io/commands) through `Redis` object. `Redis` has one or more (overloaded) methods for each Redis command. The method has the same (lowercased) name as the corresponding command. For example, we have 3 overload methods for the `DEL key [key ...]` command:

```C++
// Delete a single key.
long long Redis::del(const StringView &key);

// Delete a batch of keys: [first, last).
template <typename Input>
long long Redis::del(Input first, Input last);

// Delete keys in the initializer_list.
template <typename T>
long long Redis::del(std::initializer_list<T> il);
```

With input parameters, these methods build a Redis command based on [Redis protocol](https://redis.io/topics/protocol), and send the command to Redis server. Then synchronously receive the reply, parse it, and return to the caller.

Let's take a closer look at these methods' parameters and return values.

#### Parameter Type

Most of these methods have the same parameters as the corresponding commands. The following is a list of parameter types:

| Parameter Type | Explaination | Example | Note |
| :------------: | ------------ | ------- | ---- |
| **StringView** | Parameters of string type. Normally used for key, value, member name, field name and so on | ***bool Redis::hset(const StringView &key, const StringView &field, const StringView &val)*** | See the [StringView section](#stringview) for details on `StringView` |
| **long long** | Parameters of integer type. Normally used for index (e.g. list commands) or integer | ***void ltrim(const StringView &key, long long start, long long stop)*** <br> ***long long decrby(const StringView &key, long long decrement)*** | |
| **double** | Parameters of floating-point type. Normally used for score (e.g. sorted set commands) or number of floating-point type | ***double incrbyfloat(const StringView &key, double increment)*** | |
| **std::chrono::duration** <br> **std::chrono::time_point** | Time-related parameters | ***bool expire(const StringView &key, const std::chrono::seconds &timeout)*** <br> ***bool expireat(const StringView &key, const std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds> &tp)*** | |
| **std::pair<StringView, StringView>** | Used for Redis hash's (field, value) pair | ***bool hset(const StringView &key, const std::pair<StringView, StringView> &item)*** | |
| **std::pair<double, double>** | Used for Redis geo's (longitude, latitude) pair | ***OptionalLongLong georadius(const StringView &key, const std::pair<double, double> &location, double radius, GeoUnit unit, const StringView &destination, bool store_dist, long long count)*** | |
| **pair of iterators** | Use a pair of iterators to specify a range of input, so that we can pass the data in a STL container to these methods | ***template < typename Input >*** <br> ***long long del(Input first, Input last)*** | Throw an exception, if it's an empty range, i.e. *first == last* |
| **std::initializer_list< T >** | Use an initializer list to specify a batch of input | ***template < typename T >*** <br> ***long long del(std::initializer_list< T > il)*** | |
| **some options** | Options for some commands | ***UpdateType***, ***template < typename T > class BoundedInterval*** | See [command_options.h](https://github.com/sewenew/redis-plus-plus/blob/master/src/sw/redis%2B%2B/command_options.h) for details |

##### StringView

[std::string_view](http://en.cppreference.com/w/cpp/string/basic_string_view) is a good choice for read-only string parameter types. `std::string_view` was however only introduced in the C++ 17 standard, so if you build *redis-plus-plus* with the `-std=c++11` (i.e. by specifying `-DREDIS_PLUS_PLUS_CXX_STANDARD=11` with cmake command) or the `-std=c++14` standard, a [simple implementation](https://github.com/sewenew/redis-plus-plus/blob/master/src/sw/redis%2B%2B/cxx11/cxx_utils.h) of `std::string_view`, called `StringView`, is available. You could build *redis-plus-plus* with the `-std=c++17` standard (i.e. the default behavior), which will supply `std::string_view` natively. The `StringView` implementation will then be disregarded by aliasing it to `std::string_view`. This is done inside the *redis-plus-plus* library with: `using StringView = std::string_view`.

Since there are conversions from `std::string` and c-style string to `StringView`, you can just pass `std::string` or c-style string to methods that need a `StringView` parameter.

```C++
// bool Redis::hset(const StringView &key, const StringView &field, const StringView &val)

// Pass c-style string to StringView.
redis.hset("key", "field", "value");

// Pass std::string to StringView.
std::string key = "key";
std::string field = "field";
std::string val = "val";
redis.hset(key, field, val);

// Mix std::string and c-style string.
redis.hset(key, field, "value");
```

#### Return Type

[Redis protocol](https://redis.io/topics/protocol) defines 5 kinds of replies:
- *Status Reply*: Also known as *Simple String Reply*. It's a non-binary string reply.
- *Bulk String Reply*: Binary safe string reply.
- *Integer Reply*: Signed integer reply. Large enough to hold `long long`.
- *Array Reply*: (Nested) Array reply.
- *Error Reply*: Non-binary string reply that gives error info.

Also these replies might be *NULL*. For instance, when you try to `GET` the value of a nonexistent key, Redis returns a *NULL Bulk String Reply*.

As we mentioned above, replies are parsed into return values of these methods. The following is a list of return types:

| Return Type | Explaination | Example | Note |
| :---------: | ------------ | ------- | ---- |
| **void** | *Status Reply* that should always return a string of "OK" | *RENAME*, *SETEX* | |
| **std::string** | *Status Reply* that NOT always return "OK", and *Bulk String Reply* | *PING*, *INFO* | |
| **bool** | *Integer Reply* that always returns 0 or 1 | *EXPIRE*, *HSET* | See the [Boolean Return Value section](#boolean-return-value) for the meaning of a boolean return value |
| **long long** | *Integer Reply* that not always return 0 or 1 | *DEL*, *APPEND* | |
| **double** | *Bulk String Reply* that represents a double | *INCRBYFLOAT*, *ZINCRBY* | |
| **std::pair** | *Array Reply* with exactly 2 elements. Since the return value is always an array of 2 elements, we return the 2 elements as a `std::pair`'s first and second elements | *BLPOP* | |
| **std::tuple** | *Array Reply* with fixed length and has more than 2 elements. Since length of the returned array is fixed, we return the array as a `std::tuple` | *BZPOPMAX* | |
| **output iterator** | General *Array Reply* with non-fixed/dynamic length. We use STL-like interface to return this kind of array replies, so that you can insert the return value into a STL container easily | *MGET*, *LRANGE* | Also, sometimes the type of output iterator decides which options to send with the command. See the [Examples section](#command-overloads) for details |
| **Optional< T >** | For any reply of type `T` that might be *NULL* | *GET*, *LPOP*, *BLPOP*, *BZPOPMAX* | See the [Optional section](#optional) for details on `Optional<T>` |
| **Variant< Args... >** | For reply that might be of serval different types | *MEMORY STATS* | NOTE: so far, this type is only supported when compiling redis-plus-plus with C++ 17 standard. This is normally used with [generic command interface](https://github.com/sewenew/redis-plus-plus#generic-command-interface). See the [Variant section](#variant) for details on `Variant<Args...>` |
| **STL container** | General *Array Reply* | *CONFIG GET* | Both *output iterator* and *STL container* are used for array reply. The difference is that *STL container* is normally used with [generic command interface](https://github.com/sewenew/redis-plus-plus#generic-command-interface). See the [STL container section](#stl-container) for example |

##### Boolean Return Value

The return type of some methods, e.g. `EXPIRE`, `HSET`, is `bool`. If the method returns `false`, it DOES NOT mean that `Redis` failed to send the command to Redis server. Instead, it means that Redis server returns an *Integer Reply*, and the value of the reply is `0`. Accordingly, if the method returns `true`, it means that Redis server returns an *Integer Reply*, and the value of the reply is `1`. You can 
check [Redis commands manual](http://redis.io/commands) for what do `0` and `1` stand for.

For example, when we send `EXPIRE` command to Redis server, it returns `1` if the timeout was set, and it returns `0` if the key doesn't exist. Accordingly, if the timeout was set, `Redis::expire` returns `true`, and if the key doesn't exist, `Redis::expire` returns `false`.

So, never use the return value to check if the command has been successfully sent to Redis server. Instead, if `Redis` failed to send command to server, it throws an exception of type `Error`. See the [Exception section](#exception) for details on exceptions.

##### Optional

[std::optional](http://en.cppreference.com/w/cpp/utility/optional) is a good option for return type, if Redis might return *NULL REPLY*. However, `std::optional` is introduced in C++ 17 standard, and if you build *redis-plus-plus* with `-std=c++11` standard (i.e. by specifying `-DREDIS_PLUS_PLUS_CXX_STANDARD=11` with cmake command), we implement our own [simple version](https://github.com/sewenew/redis-plus-plus/blob/master/src/sw/redis%2B%2B/cxx11/cxx_utils.h), i.e. `template Optional<T>`. Instead, if you build *redis-plus-plus* with `-std=c++17` standard (i.e. the default behavior), you can use `std::optional`, and we have an alias for it: `template <typename T> using Optional = std::optional<T>`.

Take the [GET](https://redis.io/commands/get) and [MGET](https://redis.io/commands/mget) commands for example:

```C++
// Or just: auto val = redis.get("key");
Optional<std::string> val = redis.get("key");

// Optional<T> has a conversion to bool.
// If it's NOT a null Optional<T> object, it's converted to true.
// Otherwise, it's converted to false.
if (val) {
    // Key exists. Dereference val to get the string result.
    std::cout << *val << std::endl;
} else {
    // Redis server returns a NULL Bulk String Reply.
    // It's invalid to dereference a null Optional<T> object.
    std::cout << "key doesn't exist." << std::endl;
}

std::vector<Optional<std::string>> values;
redis.mget({"key1", "key2", "key3"}, std::back_inserter(values));
for (const auto &val : values) {
    if (val) {
        // Key exist, process the value.
    }
}
```

We also have some typedefs for some commonly used `Optional<T>`:

```C++
using OptionalString = Optional<std::string>;

using OptionalLongLong = Optional<long long>;

using OptionalDouble = Optional<double>;

using OptionalStringPair = Optional<std::pair<std::string, std::string>>;
```

##### Variant

[std::variant](https://en.cppreference.com/w/cpp/utility/variant) is a good option for return type, if the reply might be of different types. For example, the `MEMORY STATS` command returns an array reply, which is, in fact, a map of key-value pairs of configurations:

```
127.0.0.1:6379> memory stats
 1) "peak.allocated"
 2) (integer) 4471104
 ...
17) "db.0"
18) 1) "overhead.hashtable.main"
    2) (integer) 104
    3) "overhead.hashtable.expires"
    4) (integer) 32
...
27) "dataset.percentage"
28) "9.70208740234375"
...
```

However, as you can see, the value part of the result might be of type long long (key: *peak.allocated*), double (key: *dataset.percentage*) or even a map (key: *db.0*). So you cannot simply parse the result into a `std::unordered_map<std::string, long long>` or `std::unordered_map<std::string, double>`. A workaround is to parse the result into a `tuple`, however, this tuple solution is ugly and error-prone. Check [this issue](https://github.com/sewenew/redis-plus-plus/issues/138) for detail.

In this case, `Variant`, which is a typedef of `std::variant` if you build redis-plus-plus with C++17 standard, is very helpful. You can parse the result into a `std::unordered_map<std::string, Variant<double, long long, std::unordered_map<std::string, long long>>>`.

```
using Var = Variant<double, long long, std::unordered_map<std::string, long long>>;
auto r = Redis("tcp://127.0.0.1");
auto v = r.command<std::unordered_map<std::string, Var>>("memory", "stats");
```

There're some limitations on `Variant` support:

- The type arguments of `Variant`, cannot have duplicate items, e.g. `Variant<double, long long, double>` won't work.
- `double` must be placed before `std::string`. Because `double` reply is, in fact, string reply, and when parsing variant, we try to parse the reply into the first matched type, specified with the type arguments from left to right. So if `double` is placed after `std::string`, i.e. on the right side of `std::string`, the reply will always be parsed into `std::string`.

Also check the [generic command section](https://github.com/sewenew/redis-plus-plus#generic-command-interface) for more examples on generic command interface.

##### STL container

When using generic command interface, instead of parsing the reply to output iterator, you can also parse it into a STL container.

```
auto r = Redis("tcp://127.0.0.1");
auto v = r.command<std::unordered_map<std::string, std::string>>("config", "get", "*");
```

Also check the [generic command section](https://github.com/sewenew/redis-plus-plus#generic-command-interface) for more examples on generic command interface.

#### Examples

Let's see some examples on how to send commands to Redis server.

##### Various Parameter Types

```C++
// ***** Parameters of StringView type *****

// Implicitly construct StringView with c-style string.
redis.set("key", "value");

// Implicitly construct StringView with std::string.
std::string key("key");
std::string val("value");
redis.set(key, val);

// Explicitly pass StringView as parameter.
std::vector<char> large_data;
// Avoid copying.
redis.set("key", StringView(large_data.data(), large_data.size()));

// ***** Parameters of long long type *****

// For index.
redis.bitcount(key, 1, 3);

// For number.
redis.incrby("num", 100);

// ***** Parameters of double type *****

// For score.
redis.zadd("zset", "m1", 2.5);
redis.zadd("zset", "m2", 3.5);
redis.zadd("zset", "m3", 5);

// For (longitude, latitude).
redis.geoadd("geo", std::make_tuple("member", 13.5, 15.6));

// ***** Time-related parameters *****

using namespace std::chrono;

redis.expire(key, seconds(1000));

auto tp = time_point_cast<seconds>(system_clock::now() + seconds(100));
redis.expireat(key, tp);

// ***** Some options for commands *****

if (redis.set(key, "value", milliseconds(100), UpdateType::NOT_EXIST)) {
    std::cout << "set OK" << std::endl;
}

redis.linsert("list", InsertPosition::BEFORE, "pivot", "val");

std::vector<std::string> res;

// (-inf, inf)
redis.zrangebyscore("zset", UnboundedInterval<double>{}, std::back_inserter(res));

// [3, 6]
redis.zrangebyscore("zset",
    BoundedInterval<double>(3, 6, BoundType::CLOSED),
    std::back_inserter(res));

// (3, 6]
redis.zrangebyscore("zset",
    BoundedInterval<double>(3, 6, BoundType::LEFT_OPEN),
    std::back_inserter(res));

// (3, 6)
redis.zrangebyscore("zset",
    BoundedInterval<double>(3, 6, BoundType::OPEN),
    std::back_inserter(res));

// [3, 6)
redis.zrangebyscore("zset",
    BoundedInterval<double>(3, 6, BoundType::RIGHT_OPEN),
    std::back_inserter(res));

// [3, +inf)
redis.zrangebyscore("zset",
    LeftBoundedInterval<double>(3, BoundType::RIGHT_OPEN),
    std::back_inserter(res));

// (3, +inf)
redis.zrangebyscore("zset",
    LeftBoundedInterval<double>(3, BoundType::OPEN),
    std::back_inserter(res));

// (-inf, 6]
redis.zrangebyscore("zset",
    RightBoundedInterval<double>(6, BoundType::LEFT_OPEN),
    std::back_inserter(res));

// (-inf, 6)
redis.zrangebyscore("zset",
    RightBoundedInterval<double>(6, BoundType::OPEN),
    std::back_inserter(res));

// ***** Pair of iterators *****

std::vector<std::pair<std::string, std::string>> kvs = {{"k1", "v1"}, {"k2", "v2"}, {"k3", "v3"}};
redis.mset(kvs.begin(), kvs.end());

std::unordered_map<std::string, std::string> kv_map = {{"k1", "v1"}, {"k2", "v2"}, {"k3", "v3"}};
redis.mset(kv_map.begin(), kv_map.end());

std::unordered_map<std::string, std::string> str_map = {{"f1", "v1"}, {"f2", "v2"}, {"f3", "v3"}};
redis.hmset("hash", str_map.begin(), str_map.end());

std::unordered_map<std::string, double> score_map = {{"m1", 20}, {"m2", 12.5}, {"m3", 3.14}};
redis.zadd("zset", score_map.begin(), score_map.end());

std::vector<std::string> keys = {"k1", "k2", "k3"};
redis.del(keys.begin(), keys.end());

// ***** Parameters of initializer_list type *****

redis.mset({
    std::make_pair("k1", "v1"),
    std::make_pair("k2", "v2"),
    std::make_pair("k3", "v3")
});

redis.hmset("hash",
    {
        std::make_pair("f1", "v1"),
        std::make_pair("f2", "v2"),
        std::make_pair("f3", "v3")
    });

redis.zadd("zset",
    {
        std::make_pair("m1", 20.0),
        std::make_pair("m2", 34.5),
        std::make_pair("m3", 23.4)
    });

redis.del({"k1", "k2", "k3"});
```

##### Various Return Types

```C++
// ***** Return void *****

redis.save();

// ***** Return std::string *****

auto info = redis.info();

// ***** Return bool *****

if (!redis.expire("nonexistent", std::chrono::seconds(100))) {
    std::cerr << "key doesn't exist" << std::endl;
}

if (redis.setnx("key", "val")) {
    std::cout << "set OK" << std::endl;
}

// ***** Return long long *****

auto len = redis.strlen("key");
auto num = redis.del({"a", "b", "c"});
num = redis.incr("a");

// ***** Return double *****

auto real = redis.incrbyfloat("b", 23.4);
real = redis.hincrbyfloat("c", "f", 34.5);

// ***** Return Optional<std::string>, i.e. OptionalString *****

auto os = redis.get("kk");
if (os) {
    std::cout << *os << std::endl;
} else {
    std::cerr << "key doesn't exist" << std::endl;
}

os = redis.spop("set");
if (os) {
    std::cout << *os << std::endl;
} else {
    std::cerr << "set is empty" << std::endl;
}

// ***** Return Optional<long long>, i.e. OptionalLongLong *****

auto oll = redis.zrank("zset", "mem");
if (oll) {
    std::cout << "rank is " << *oll << std::endl;
} else {
    std::cerr << "member doesn't exist" << std::endl;
}

// ***** Return Optional<double>, i.e. OptionalDouble *****

auto ob = redis.zscore("zset", "m1");
if (ob) {
    std::cout << "score is " << *ob << std::endl;
} else {
    std::cerr << "member doesn't exist" << std::endl;
}

// ***** Return Optional<pair<string, string>> *****

auto op = redis.blpop({"list1", "list2"}, std::chrono::seconds(2));
if (op) {
    std::cout << "key is " << op->first << ", value is " << op->second << std::endl;
} else {
    std::cerr << "timeout" << std::endl;
}

// ***** Output iterators *****

std::vector<OptionalString> os_vec;
redis.mget({"k1", "k2", "k3"}, std::back_inserter(os_vec));

std::vector<std::string> s_vec;
redis.lrange("list", 0, -1, std::back_inserter(s_vec));

std::unordered_map<std::string, std::string> hash;
redis.hgetall("hash", std::inserter(hash, hash.end()));
// You can also save the result in a vecotr of string pair.
std::vector<std::pair<std::string, std::string>> hash_vec;
redis.hgetall("hash", std::back_inserter(hash_vec));

std::unordered_set<std::string> str_set;
redis.smembers("s1", std::inserter(str_set, str_set.end()));
// You can also save the result in a vecotr of string.
s_vec.clear();
redis.smembers("s1", std::back_inserter(s_vec));
```

##### SCAN Commands

```C++
auto cursor = 0LL;
auto pattern = "*pattern*";
auto count = 5;
std::unordered_set<std::string> keys;
while (true) {
    cursor = redis.scan(cursor, pattern, count, std::inserter(keys, keys.begin()));
    // Default pattern is "*", and default count is 10
    // cursor = redis.scan(cursor, std::inserter(keys, keys.begin()));

    if (cursor == 0) {
        break;
    }
}
```

##### Command Overloads

Sometimes the type of output iterator decides which options to send with the command.

```C++
// If the output iterator is an iterator of a container of string,
// we send *ZRANGE* command without the *WITHSCORES* option.
std::vector<std::string> members;
redis.zrange("list", 0, -1, std::back_inserter(members));

// If it's an iterator of a container of a <string, double> pair,
// we send *ZRANGE* command with *WITHSCORES* option.
std::vector<std::pair<std::string, double>> res_with_score;
redis.zrange("list", 0, -1, std::back_inserter(res_with_score));

// The above examples also apply to other command with the *WITHSCORES* options,
// e.g. *ZRANGEBYSCORE*, *ZREVRANGE*, *ZREVRANGEBYSCORE*.

// Another example is the *GEORADIUS* command.

// Only get members.
members.clear();
redis.georadius("geo",
            std::make_pair(10.1, 11.1),
            100,
            GeoUnit::KM,
            10,
            true,
            std::back_inserter(members));

// If the iterator is an iterator of a container of tuple<string, double>,
// we send the *GEORADIUS* command with *WITHDIST* option.
std::vector<std::tuple<std::string, double>> mem_with_dist;
redis.georadius("geo",
            std::make_pair(10.1, 11.1),
            100,
            GeoUnit::KM,
            10,
            true,
            std::back_inserter(mem_with_dist));

// If the iterator is an iterator of a container of tuple<string, double, string>,
// we send the *GEORADIUS* command with *WITHDIST* and *WITHHASH* options.
std::vector<std::tuple<std::string, double, std::string>> mem_with_dist_hash;
redis.georadius("geo",
            std::make_pair(10.1, 11.1),
            100,
            GeoUnit::KM,
            10,
            true,
            std::back_inserter(mem_with_dist_hash));

// If the iterator is an iterator of a container of
// tuple<string, string, pair<double, double>, double>,
// we send the *GEORADIUS* command with *WITHHASH*, *WITHCOORD* and *WITHDIST* options.
std::vector<std::tuple<std::string, double, std::string>> mem_with_hash_coord_dist;
redis.georadius("geo",
            std::make_pair(10.1, 11.1),
            100,
            GeoUnit::KM,
            10,
            true,
            std::back_inserter(mem_with_hash_coord_dist));
```

Please see [redis.h](https://github.com/sewenew/redis-plus-plus/blob/master/src/sw/redis%2B%2B/redis.h) for doxygen style API references and examples, and see the [tests](https://github.com/sewenew/redis-plus-plus/tree/master/test/src/sw/redis%2B%2B) for other examples.

### Exception

`Redis` throws exceptions if it receives an *Error Reply* or something bad happens, e.g. failed to create a connection to server, or connection to server is broken. All exceptions derived from `Error` class. See [errors.h](https://github.com/sewenew/redis-plus-plus/blob/master/src/sw/redis%2B%2B/errors.h) for details.

- `Error`: Generic error. It's derived from `std::exception`, and it's also the base class of other exceptions.
- `IoError`: There's some IO error with the connection.
- `TimeoutError`: Read or write operation was timed out. It's a derived class of `IoError`.
- `ClosedError`: Redis server closed the connection.
- `ProtoError`: The command or reply is invalid, and we cannot process it with Redis protocol.
- `OomError`: *hiredis* library got an out-of-memory error.
- `ReplyError`: Redis server returned an error reply, e.g. we try to call `redis::lrange` on a Redis hash.
- `WatchError`: Watched key has been modified. See [Watch section](#watch) for details.

**NOTE**: *NULL REPLY* is not taken as an exception. For example, if we try to `GET` a non-existent key, we'll get a *NULL Bulk String Reply*. Instead of throwing an exception, we return the *NULL REPLY* as a null `Optional<T>` object. Also see [Optional section](#optional).

Normally, when exception happens, you don't need to create a `Redis` object. It's exception safe, and you can reuse the `Redis` object. Even if the connection to Redis server is broken, and it throws some exception, say, `IoError`. The next time when you send command with the `Redis` object, it will try to reconnect to Redis server automatically. This rule also applies to `RedisCluster`. However, if `Pipeline`, `Transcation` and `Subscriber` throws exception, you need to destroy the object, and create a new one. See the corresponding documentation for details.

#### Examples

The following is an example on how to catch these exceptions:

```
try {
    redis.set("key", "value");

    // Wrong type error
    redis.lpush("key", {"a", "b", "c"});
} catch (const ReplyError &err) {
    // WRONGTYPE Operation against a key holding the wrong kind of value
    cout << err.what() << endl;
} catch (const TimeoutError &err) {
    // reading or writing timeout
} catch (const ClosedError &err) {
    // the connection has been closed.
} catch (const IoError &err) {
    // there's an IO error on the connection.
} catch (const Error &err) {
   // other errors
}
```

### Generic Command Interface

There're too many Redis commands, we haven't implemented all of them. However, you can use the generic `Redis::command` methods to send any commands to Redis. Unlike other client libraries, `Redis::command` doesn't use format string to combine command arguments into a command string. Instead, you can directly pass command arguments of `StringView` type or arithmetic type as parameters of `Redis::command`. For the reason why we don't use format string, please see [this discussion](https://github.com/sewenew/redis-plus-plus/pull/2).

```C++
auto redis = Redis("tcp://127.0.0.1");

// Redis class doesn't have built-in *CLIENT SETNAME* method.
// However, you can use Redis::command to send the command manually.
redis.command<void>("client", "setname", "name");
auto val = redis.command<OptionalString>("client", "getname");
if (val) {
    std::cout << *val << std::endl;
}

// NOTE: the following code is for example only. In fact, Redis has built-in
// methods for the following commands.

// Arguments of the command can be strings.
// NOTE: for SET command, the return value is NOT always void, I'll explain latter.
redis.command<void>("set", "key", "100");

// Arguments of the command can be a combination of strings and integers.
auto num = redis.command<long long>("incrby", "key", 1);

// Argument can also be double.
auto real = redis.command<double>("incrbyfloat", "key", 2.3);

// Even the key of the command can be of arithmetic type.
redis.command<void>("set", 100, "value");

val = redis.command<OptionalString>("get", 100);

// If the command returns an array of elements.
std::vector<OptionalString> result;
redis.command("mget", "k1", "k2", "k3", std::back_inserter(result));

// Or just parse it into a vector.
result = redis.command<std::vector<OptionalString>>("mget", "k1", "k2", "k3");

// Arguments of the command can be a range of strings.
auto set_cmd_strs = {"set", "key", "value"};
redis.command<void>(set_cmd_strs.begin(), set_cmd_strs.end());

auto get_cmd_strs = {"get", "key"};
val = redis.command<OptionalString>(get_cmd_strs.begin(), get_cmd_strs.end());

// If it returns an array of elements.
result.clear();
auto mget_cmd_strs = {"mget", "key1", "key2"};
redis.command(mget_cmd_strs.begin(), mget_cmd_strs.end(), std::back_inserter(result));
```

**NOTE**: The name of some Redis commands is composed with two strings, e.g. *CLIENT SETNAME*. In this case, you need to pass these two strings as two arguments for `Redis::command`.

```C++
// This is GOOD.
redis.command<void>("client", "setname", "name");

// This is BAD, and will fail to send command to Redis server.
// redis.command<void>("client setname", "name");
```

As I mentioned in the comments, the `SET` command not always returns `void`. Because if you try to set a (key, value) pair with *NX* or *XX* option, you might fail, and Redis will return a *NULL REPLY*. Besides the `SET` command, there're other commands whose return value is NOT a fixed type, you need to parse it by yourself. For example, `Redis::set` method rewrite the reply of `SET` command, and make it return `bool` type, i.e. if no *NX* or *XX* option specified, Redis server will always return an "OK" string, and `Redis::set` returns `true`; if *NX* or *XX* specified, and Redis server returns a *NULL REPLY*, `Redis::set` returns `false`.

So `Redis` class also has other overloaded `command` methods, these methods return a `ReplyUPtr`, i.e. `std::unique_ptr<redisReply, ReplyDeleter>`, object. Normally you don't need to parse it manually. Instead, you only need to pass the reply to `template <typename T> T reply::parse(redisReply &)` to get a value of type `T`. Check the [Return Type section](#return-type) for valid `T` types. If the command returns an array of elements, besides calling `reply::parse` to parse the reply to an STL container, you can also call `template <typename Output> reply::to_array(redisReply &reply, Output output)` to parse the result into an array or STL container with an output iterator.

Let's rewrite the above examples:

```C++
auto redis = Redis("tcp://127.0.0.1");

redis.command("client", "setname", "name");
auto r = redis.command("client", "getname");
assert(r);

// If the command returns a single element,
// use `reply::parse<T>(redisReply&)` to parse it.
auto val = reply::parse<OptionalString>(*r);
if (val) {
    std::cout << *val << std::endl;
}

// Arguments of the command can be strings.
redis.command("set", "key", "100");

// Arguments of the command can be a combination of strings and integers.
r = redis.command("incrby", "key", 1);
auto num = reply::parse<long long>(*r);

// Argument can also be double.
r = redis.command("incrbyfloat", "key", 2.3);
auto real = reply::parse<double>(*r);

// Even the key of the command can be of arithmetic type.
redis.command("set", 100, "value");

r = redis.command("get", 100);
val = reply::parse<OptionalString>(*r);

// If the command returns an array of elements.
r = redis.command("mget", "k1", "k2", "k3");
// Use `reply::to_array(redisReply&, OutputIterator)` to parse the result into an STL container.
std::vector<OptionalString> result;
reply::to_array(*r, std::back_inserter(result));

// Or just call `reply::parse` to parse it into vector.
result = reply::parse<std::vector<OptionalString>>(*r);

// Arguments of the command can be a range of strings.
auto get_cmd_strs = {"get", "key"};
r = redis.command(get_cmd_strs.begin(), get_cmd_strs.end());
val = reply::parse<OptionalString>(*r);

// If it returns an array of elements.
result.clear();
auto mget_cmd_strs = {"mget", "key1", "key2"};
r = redis.command(mget_cmd_strs.begin(), mget_cmd_strs.end());
reply::to_array(*r, std::back_inserter(result));
```

In fact, there's one more `Redis::command` method:

```C++
template <typename Cmd, typename ...Args>
auto command(Cmd cmd, Args &&...args)
    -> typename std::enable_if<!std::is_convertible<Cmd, StringView>::value, ReplyUPtr>::type;
```

However, this method exposes some implementation details, and is only for internal use. You should NOT use this method.

### Publish/Subscribe

You can use `Redis::publish` to publish messages to channels. `Redis` randomly picks a connection from the underlying connection pool, and publishes message with that connection. So you might publish two messages with two different connections.

When you subscribe to a channel with a connection, all messages published to the channel are sent back to that connection. So there's NO `Redis::subscribe` method. Instead, you can call `Redis::subscriber` to create a `Subscriber` and the `Subscriber` maintains a connection to Redis. The underlying connection is a new connection, NOT picked from the connection pool. This new connection has the same `ConnectionOptions` as the `Redis` object.

With `Subscriber`, you can call `Subscriber::subscribe`, `Subscriber::unsubscribe`, `Subscriber::psubscribe` and `Subscriber::punsubscribe` to send *SUBSCRIBE*, *UNSUBSCRIBE*, *PSUBSCRIBE* and *PUNSUBSCRIBE* commands to Redis.

#### Thread Safety

`Subscriber` is NOT thread-safe. If you want to call its member functions in multi-thread environment, you need to synchronize between threads manually.

#### Exception

If any of the `Subscriber`'s method throws an exception other than `ReplyError` or `TimeoutError`, you CANNOT use it any more. Instead, you have to destroy the `Subscriber` object, and create a new one.

#### Subscriber Callbacks

There are 6 kinds of messages:
- *MESSAGE*: message sent to a channel.
- *PMESSAGE*: message sent to channels of a given pattern.
- *SUBSCRIBE*: message sent when we successfully subscribe to a channel.
- *UNSUBSCRIBE*: message sent when we successfully unsubscribe to a channel.
- *PSUBSCRIBE*: message sent when we successfully subscribe to a channel pattern.
- *PUNSUBSCRIBE*: message sent when we successfully unsubscribe to a channel pattern.

We call messages of *SUBSCRIBE*, *UNSUBSCRIBE*, *PSUBSCRIBE* and *PUNSUBSCRIBE* types as *META MESSAGE*s.

In order to process these messages, you can set callback functions on `Subscriber`:
- `Subscriber::on_message(MsgCallback)`: set callback function for messages of *MESSAGE* type, and the callback interface is: `void (std::string channel, std::string msg)`.
- `Subscriber::on_pmessage(PatternMsgCallback)`: set the callback function for messages of *PMESSAGE* type, and the callback interface is: `void (std::string pattern, std::string channel, std::string msg)`.
- `Subscriber::on_meta(MetaCallback)`: set callback function for messages of *META MESSAGE* type, and the callback interface is: `void (Subscriber::MsgType type, OptionalString channel, long long num)`. `type` is an enum, it can be one of the following enum: `Subscriber::MsgType::SUBSCRIBE`, `Subscriber::MsgType::UNSUBSCRIBE`, `Subscriber::MsgType::PSUBSCRIBE`, `Subscriber::MsgType::PUNSUBSCRIBE`, `Subscriber::MsgType::MESSAGE`, and `Subscriber::MsgType::PMESSAGE`. If you haven't subscribe/psubscribe to any channel/pattern, and try to unsubscribe/punsubscribe without any parameter, i.e. unsubscribe/punsubscribe all channels/patterns, *channel* will be null. So the second parameter of meta callback is of type `OptionalString`.

All these callback interfaces pass `std::string` by value, and you can take their ownership (i.e. `std::move`) safely.

#### Consume Messages

You can call `Subscriber::consume` to consume messages published to channels/patterns that the `Subscriber` has been subscribed.

`Subscriber::consume` waits for message from the underlying connection. If the `ConnectionOptions::socket_timeout` is reached, and there's no message sent to this connection, `Subscriber::consume` throws a `TimeoutError` exception. If `ConnectionOptions::socket_timeout` is `0ms`, `Subscriber::consume` blocks until it receives a message.

After receiving the message, `Subscriber::consume` calls the callback function to process the message based on message type. However, if you don't set callback for a specific kind of message, `Subscriber::consume` will consume the received message and discard it, i.e. `Subscriber::consume` returns without running the callback.

#### Examples

The following example is a common pattern for using `Subscriber`:

```C++
// Create a Subscriber.
auto sub = redis.subscriber();

// Set callback functions.
sub.on_message([](std::string channel, std::string msg) {
            // Process message of MESSAGE type.
        });

sub.on_pmessage([](std::string pattern, std::string channel, std::string msg) {
            // Process message of PMESSAGE type.
        });

sub.on_meta([](Subscriber::MsgType type, OptionalString channel, long long num) {
            // Process message of META type.
        });

// Subscribe to channels and patterns.
sub.subscribe("channel1");
sub.subscribe({"channel2", "channel3"});

sub.psubscribe("pattern1*");

// Consume messages in a loop.
while (true) {
    try {
        sub.consume();
    } catch (const Error &err) {
        // Handle exceptions.
    }
}
```

If `ConnectionOptions::socket_timeout` is set, you might get `TimeoutError` exception before receiving a message:

```C++
while (true) {
    try {
        sub.consume();
    } catch (const TimeoutError &e) {
        // Try again.
        continue;
    } catch (const Error &err) {
        // Handle other exceptions.
    }
}
```

The above examples use lambda as callback. If you're not familiar with lambda, you can also set a free function as callback. Check [this issue](https://github.com/sewenew/redis-plus-plus/issues/16) for detail.

### Pipeline

[Pipeline](https://redis.io/topics/pipelining) is used to reduce *RTT* (Round Trip Time), and speed up Redis queries. *redis-plus-plus* supports pipeline with the `Pipeline` class.

#### Create Pipeline

You can create a pipeline with `Redis::pipeline` method, which returns a `Pipeline` object.

```C++
ConnectionOptions connection_options;
ConnectionPoolOptions pool_options;

Redis redis(connection_options, pool_options);

auto pipe = redis.pipeline();
```

When creating a `Pipeline` object, by default, `Redis::pipeline` method creates a new connection to Redis server. This connection is NOT picked from the connection pool, but a newly created connection. This connection has the same `ConnectionOptions` as other connections in the connection pool. `Pipeline` object maintains the new connection, and all piped commands are sent through this connection.

**NOTE**: By default, creating a `Pipeline` object is NOT cheap, since it creates a new connection. So you'd better reuse the `Pipeline` object as much as possible. Check [this](#create-pipeline-without-creating-new-connection) to see how to create a `Pipeline` object without creating a new connection.

#### Send Commands

You can send Redis commands through the `Pipeline` object. Just like the `Redis` class, `Pipeline` has one or more (overloaded) methods for each Redis command. However, you CANNOT get the replies until you call `Pipeline::exec`. So these methods do NOT return the reply, instead they return the `Pipeline` object itself. And you can chain these methods calls.

```C++
pipe.set("key", "val").incr("num").rpush("list", {0, 1, 2}).command("hset", "key", "field", "value");
```

#### Get Replies

Once you finish sending commands to Redis, you can call `Pipeline::exec` to get replies of these commands. You can also chain `Pipeline::exec` with other commands.

```C++
pipe.set("key", "val").incr("num");
auto replies = pipe.exec();

// The same as:
replies = pipe.set("key", "val").incr("num).exec();
```

In fact, these commands won't be sent to Redis, until you call `Pipeline::exec`. So `Pipeline::exec` does 2 work in order: send all piped commands, then get all replies from Redis.

Also you can call `Pipeline::discard` to discard those piped commands.

```C++
pipe.set("key", "val").incr("num");

pipe.discard();
```

#### Parse Replies

`Pipeline::exec` returns a `QueuedReplies` object, which contains replies of all commands that have been sent to Redis. You can use `QueuedReplies::get` method to get and parse the `ith` reply. It has 3 overloads:

- `template <typename Result> Result get(std::size_t idx)`: Return the `ith` reply as a return value, and you need to specify the return type as tempalte parameter.
- `template <typename Output> void get(std::size_t idx, Output output)`: If the reply is of type *Array Reply*, you can call this method to write the `ith` reply to an output iterator. Normally, compiler will deduce the type of the output iterator, and you don't need to specify the type parameter explicitly.
- `redisReply& get(std::size_t idx)`: If the reply is NOT a fixed type, call this method to get a reference to `redisReply` object. In this case, you need to call `template <typename T> T reply::parse(redisReply &)` to parse the reply manually.

Check the [Return Type section](#return-type) for details on the return types of the result.

```C++
auto replies = pipe.set("key", "val").incr("num").lrange("list", 0, -1).exec();

auto set_cmd_result = replies.get<bool>(0);

auto incr_cmd_result = replies.get<long long>(1);

std::vector<std::string> list_cmd_result;
replies.get(2, std::back_inserter(list_cmd_result));
```

#### Exception

If any of `Pipeline`'s method throws an exception other than `ReplyError`, the `Pipeline` object enters an invalid state. You CANNOT use it any more, but only destroy the object, and create a new one.

#### Thread Safety

`Pipeline` is NOT thread-safe. If you want to call its member functions in multi-thread environment, you need to synchronize between threads manually.

#### Create Pipeline Without Creating New Connection

**YOU MUST CAREFULLY READ ALL WORDS IN THIS SECTION AND THE VERY IMPORTANT NOTES BEFORE USING THIS FEATURE!!!**

In fact, you can also create a `Pipeline` object with a connection from the underlying connection pool, so that calling `Redis::pipeline` method can be much cheaper (since it doesn't need to create a new connection).

The prototype of `Redis::pipeline` is as follows: `Pipeline pipeline(bool new_connection = true);`. If `new_connection` is false, the `Pipeline` object will be created with a connection from the underlying pool.

```
ConnectionOptions connection_options;
ConnectionPoolOptions pool_options;

Redis redis(connection_options, pool_options);

// Create a Pipeline without creating a new connection.
auto pipe = redis.pipeline(false);
```

##### VERY IMPORTANT NOTES

However, in this case, you MUST be very careful, otherwise, you might get bad performance or even dead lock. Because when you run command with `Pipeline` object, it will hold the connection until `Pipeline::exec`, `Pipeline::discard` or `Pipeline`'s destructor is called (the connection will also be released if any method of `Pipeline` throws `Exception`). If the `Pipeline` object holds the connection for a long time, other `Redis` methods might not be able to get a connection from the underlying pool.

Check the following dead lock example:

```
// By defaul, create a `Redis` object with only ONE connection in pool.
// Also by default, the `ConnectionPoolOptions::wait_timeout` is 0ms,
// which means if the pool is empty, `Redis` method will be blocked until
// the pool is not empty.
Redis redis("tcp://127.0.0.1");

// Create a `Pipeline` with a connection in the underlying pool.
// In fact, the connection hasn't been fetched from the pool
// until some method of `Pipeline` has been called.
auto pipe = redis.pipeline(false);

// Now the `Pipeline` object fetches a connection from the pool.
pipe.set("key1", "val");

// `Pipeline` object still holds the connection until `Pipeline::exec`,
// `Pipeline::discard` or the destructor is called.
pipe.set("key2", "val");

// Try to send a command with `Redis` object.
// However, the pool is empty, since the `Pipeline` object still holds
// the connection, and this call will be blocked forever.
// DEAD LOCK!!!
redis.get("key");

// NEVER goes here.
pipe.exec();
```

**BEST PRACTICE**:

When creating `Pipeline` without creating a new connection:

- Always set `ConnectionPoolOptions::wait_timeout` larger than 0ms (i.e. when pool is empty, never block forever).
- Avoid doing slow operation between `Pipeline`'s methods.
- Better chain `Pipeline` methods and the `Pipeline::exec` in one statements.
- Better leave `Pipeline` related code in a block scope.

```
ConnectionOptions opts;
opts.host = "127.0.0.1";
opts.port = 6379;
opts.socket_timeout = std::chrono::milliseconds(50);

ConnectionPoolOptions pool_opts;
pool_opts.size = 3;

// Always set `wait_timeout` larger than 0ms.
pool_opts.wait_timeout = std::chrono::milliseconds(50);

auto redis = Redis(opts, pool_opts);

{
    // Better put `Pipeline` related code in a block scope.
    auto pipe = redis.pipeline(false);

    pipe.set("key1", "val");

    // DON'T run slow operations here, since `Pipeline` object still holds
    // the connection, other threads using this `Redis` object, might be blocked.

    pipe.set("key2", "val");

    // When `Pipeline::exec` finishes, `Pipeline` releases the connection, and returns it to pool.
    auto replies = pipe.exec();

    // This is even better, i.e. chain `Pipeline` methods with `Pipeline::exec`.
    replies = pipe.set("key1", "val").set("key2", "val").exec();
}

for (auto i = 0; i < 10; ++i) {
    // This operation, i.e. creating a `Pipeline` object with connection in pool, is cheap
    auto pipe = redis.pipeline(false);

    // Fetch a connection from the underlying pool, and hold it.
    pipe.set("key1", "val").set("key2", "val");

    // Although `Pipeline::exec` and `Pipeline::discard` haven't been called,
    // when `Pipeline`'s destructor is called, the connection will also be
    // returned to the pool.
}
```

### Transaction

[Transaction](https://redis.io/topics/transactions) is used to make multiple commands runs atomically.

#### Create Transaction

You can create a transaction with `Redis::transaction` method, which returns a `Transaction` object.

```C++
ConnectionOptions connection_options;
ConnectionPoolOptions pool_options;

Redis redis(connection_options, pool_options);

auto tx = redis.transaction();
```

As the `Pipeline` class, `Transaction` maintains a newly created connection to Redis. This connection has the same `ConnectionOptions` as the `Redis` object.

**NOTE**: Creating a `Transaction` object is NOT cheap, since it creates a new connection. So you'd better reuse the `Transaction` as much as possible. Check [this](#create-transaction-without-creating-new-connection) to see how to create a `Transaction` object without creating a new connection.

Also you don't need to send [MULTI](https://redis.io/commands/multi) command to Redis. `Transaction` will do that for you automatically.

#### Send Commands

`Transaction` shares most of implementation with `Pipeline`. It has the same interfaces as `Pipeline`. You can send commands as what you do with `Pipeline` object.

```C++
tx.set("key", "val").incr("num").lpush("list", {0, 1, 2}).command("hset", "key", "field", "val");
```

#### Execute Transaction

When you call `Transaction::exec`, you explicitly ask Redis to execute those queued commands, and return the replies. Otherwise, these commands won't be executed. Also, you can call `Transaction::discard` to discard the execution, i.e. no command will be executed. Both `Transaction::exec` and `Transaction::discard` can be chained with other commands.

```C++
auto replies = tx.set("key", "val").incr("num").exec();

tx.set("key", "val").incr("num");

// Discard the transaction.
tx.discard();
```

#### Parse Replies

See [Pipeline's Parse Replies section](#parse-replies) for how to parse the replies.

#### Piped Transaction

Normally, we always send multiple commnds in a transaction. In order to improve the performance, you can send these commands in a pipeline. You can create a piped transaction by passing `true` as parameter of `Redis::transaction` method.

```C++
// Create a piped transaction
auto tx = redis.transaction(true);
```

With this piped transaction, all commands are sent to Redis in a pipeline.

#### Exception

If any of `Transaction`'s method throws an exception other than `WatchError` or `ReplyError`, the `Transaction` object enters an invalid state. You CANNOT use it any more, but only destroy the object and create a new one.

#### Thread Safety

`Transacation` is NOT thread-safe. If you want to call its member functions in multi-thread environment, you need to synchronize between threads manually.

#### Watch

[WATCH is used to provide a check-and-set(CAS) behavior to Redis transactions](https://redis.io/topics/transactions#optimistic-locking-using-check-and-set).

The `WATCH` command must be sent in the same connection as the transaction. And normally after the `WATCH` command, we also need to send some other commands to get data from Redis before executing the transaction. Take the following check-and-set case as an example:

```
WATCH key           // watch a key
val = GET key       // get value of the key
new_val = val + 1   // incr the value
MULTI               // begin the transaction
SET key new_val     // set value only if the value is NOT modified by others
EXEC                // try to execute the transaction.
                    // if val has been modified, the transaction won't be executed.
```

However, with `Transaction` object, you CANNOT get the result of commands until the whole transaction has been finished. Instead, you need to create a `Redis` object from the `Transaction` object. The created `Redis` object shares the connection with `Transaction` object. With this created `Redis` object, you can send `WATCH` command and any other Redis commands to Redis server, and get the result immediately.

Let's see how to implement the above example with *redis-plus-plus*:

```C++
auto redis = Redis("tcp://127.0.0.1");

// Create a transaction.
auto tx = redis.transaction();

// Create a Redis object from the Transaction object. Both objects share the same connection.
auto r = tx.redis();

// If the watched key has been modified by other clients, the transaction might fail.
// So we need to retry the transaction in a loop.
while (true) {
    try {
        // Watch a key.
        r.watch("key");

        // Get the old value.
        auto val = r.get("key");
        auto num = 0;
        if (val) {
            num = std::stoi(*val);
        } // else use default value, i.e. 0.

        // Incr value.
        ++num;

        // Execute the transaction.
        auto replies = tx.set("key", std::to_string(num)).exec();

        // Transaction has been executed successfully. Check the result and break.

        assert(replies.size() == 1 && replies.get<bool>(0) == true);

        break;
    } catch (const WatchError &err) {
        // Key has been modified by other clients, retry.
        continue;
    } catch (const Error &err) {
        // Something bad happens, and the Transaction object is no longer valid.
        throw;
    }
}
```

**NOTE**: in the example above, we create `Transaction` object outside the while loop, in order to avoid creating new connection again and again.

#### Create Transaction Without Creating New Connection

**NOTE**: YOU MUST CAREFULLY READ ALL WORDS AND THE VERY IMPORTANT NOTES LINK IN THIS SECTION BEFORE USING THIS FEATURE!!!

In fact, you can also create a `transaction` object with a connection from the underlying connection pool, so that calling `Redis::transaction` method can be much cheaper (since it doesn't need to create a new connection).

The prototype of `Redis::transaction` is as follows: `Transaction transaction(bool piped = false, bool new_connection = true);`. If `new_connection` is false, the `Transaction` object will be created with a connection from the underlying pool.

```
ConnectionOptions connection_options;
ConnectionPoolOptions pool_options;

Redis redis(connection_options, pool_options);

// Create a Transaction without creating a new connection.
auto tx = redis.transaction(false, false);
```

However, in this case, you MUST be very careful, otherwise, you might get bad performance or even dead lock. Please carefully check the similar pipeline's [VERY IMPORTANT NOTES section](#very-important-notes), before you use it!

Besides those very important notes, there's another important note for `Transaction`:

- Limit the scope of `Redis` object created by `Transaction::Redis`, i.e. destroy it ASAP.

Check the following example:

```C++
auto redis = Redis(opts, pool_opts);

// Create a `Transaction` object without creating a new connection.
auto tx = redis.Transaction(false, false);

// Create a `Redis`, and this `Redis` object shares the same connection with the `Transaction` object.
auto r = tx.redis();

// Other code here...

// Execute the transaction.
auto replies = tx.set("key", "val").exec();

// Although `Transaction::exec` has been called, the connection has not been returned to pool.
// Because the `Redis` object, i.e. `r`, still holds the connection.
```

So the above watch example should be modified as follows:

```C++
auto redis = Redis(opts, pool_opts);

// If the watched key has been modified by other clients, the transaction might fail.
// So we need to retry the transaction in a loop.
while (true) {
    try {
        // Create a transaction without creating a new connection.
        auto tx = redis.transaction(false, false);

        // Create a Redis object from the Transaction object. Both objects share the same connection.
        auto r = tx.redis();

        // Watch a key.
        r.watch("key");

        // Get the old value.
        auto val = r.get("key");
        auto num = 0;
        if (val) {
            num = std::stoi(*val);
        } // else use default value, i.e. 0.

        // Incr value.
        ++num;

        // Execute the transaction.
        auto replies = tx.set("key", std::to_string(num)).exec();

        // Transaction has been executed successfully. Check the result and break.

        assert(replies.size() == 1 && replies.get<bool>(0) == true);

        break;
    } catch (const WatchError &err) {
        // Key has been modified by other clients, retry.
        continue;
    } catch (const Error &err) {
        // Something bad happens, and the Transaction object is no longer valid.
        throw;
    }
}
```

**NOTE**: The difference is that we create the `Transaction` object in the while loop (it's cheap, since it doesn't need to create a new connection). When the `Transaction` object and the `Redis` object created by `Transaction::redis` have been destroyed, the connection will be return to pool.

### Redis Cluster

*redis-plus-plus* supports [Redis Cluster](https://redis.io/topics/cluster-tutorial). You can use `RedisCluster` class to send commands to Redis Cluster. It has similar interfaces as `Redis` class.

#### Connection

By default, `RedisCluster` connects to all master nodes in the cluster. For each master node, it maintains a connection pool. If you want to read from slave nodes, you need to explicitly set an option (see [below](#read-from-replica) for reference).

You can initialize a `RedisCluster` instance with `ConnectionOptions` and `ConnectionPoolOptions`. You only need to set one master node's host & port in `ConnectionOptions`, and `RedisCluster` will get other nodes' info automatically (with the *CLUSTER SLOTS* command). For each master node, it creates a connection pool with the specified `ConnectionPoolOptions`. If `ConnectionPoolOptions` is not specified, `RedisCluster` maintains a single connection to every master node.

```C++
// Set a master node's host & port.
ConnectionOptions connection_options;
connection_options.host = "127.0.0.1";  // Required.
connection_options.port = 7000; // Optional. The default port is 6379.
connection_options.password = "auth"; // Optional. No password by default.

// Automatically get other nodes' info,
// and connect to every master node with a single connection.
RedisCluster cluster1(connection_options);

ConnectionPoolOptions pool_options;
pool_options.size = 3;

// For each master node, maintains a connection pool of size 3.
RedisCluster cluster2(connection_options, pool_options);
```

You can also specify connection option with an URI. However, in this way, you can only use default `ConnectionPoolOptions`, i.e. pool of size 1, and CANNOT specify password.

```C++
// Specify a master node's host & port.
RedisCluster cluster3("tcp://127.0.0.1:7000");

// Use default port, i.e. 6379.
RedisCluster cluster4("tcp://127.0.0.1");
```

##### Read From Replica

If you want to scale read by reading (possible stale) data from slave nodes, you can specifiy `Role::SLAVE` as the third parameter of `RedisCluster`'s constructor. In this case, *redis-plus-plus* will randomly pick a replica node for each master node of the cluster, and create a connection pool for the replica node.

```C++
RedisCluster cluster(connection_options, pool_options, Role::SLAVE);

auto val = cluster.get("key");
```

In this case, you can only send readonly commands to Redis Cluster. If you try to send a write command, e.g. `set`, `hset`, *redis-plus-plus* will throw an exception. Currently, *redis-plus-plus* doesn't handle this case, i.e. sending write command in `Role::SLAVE` mode, elegantly, and you might get some performance problem. So, NEVER send write command in `Role::SLAVE` mode. I'll fix this issue in the future.

**NOTE**: In `Role::SLAVE` mode, you don't need to manually send [READONLY](https://redis.io/commands/readonly) command to slave nodes. Instead, *redis-plus-plus* will send *READONLY* command to slave nodes automatically.

##### Note

- `RedisCluster` only works with tcp connection. It CANNOT connect to Unix Domain Socket. If you specify Unix Domain Socket in `ConnectionOptions`, it throws an exception.
- All nodes in the cluster should have the same password.
- Since [Redis Cluster does NOT support multiple databses](https://redis.io/topics/cluster-spec#implemented-subset), `ConnectionOptions::db` is ignored.

#### Interfaces

As we mentioned above, `RedisCluster`'s interfaces are similar to `Redis`. It supports most of `Redis`' interfaces, including the [generic command interface](#generic-command-interface) (see `Redis`' [API Reference section](#api-reference) for details), except the following:

- Not support commands without key as argument, e.g. `PING`, `INFO`.
- Not support Lua script without key parameters.

Since there's no key parameter, `RedisCluster` has no idea on to which node these commands should be sent. However there're 2 workarounds for this problem:

- If you want to send these commands to a specific node, you can create a `Redis` object with that node's host and port, and use the `Redis` object to do the work.
- Instead of host and port, you can also call `Redis RedisCluster::redis(const StringView &hash_tag)` to create a `Redis` object with a hash-tag specifying the node. In this case, the returned `Redis` object creates a new connection to Redis server. **NOTE**: the returned `Redis` object, **IS NOT THREAD SAFE!**. Also, when using the returned `Redis` object, if it throws exception, you need to destroy it, and create a new one with the `RedisCluster::redis` method.

Also you can use the [hash tags](https://redis.io/topics/cluster-spec#keys-hash-tags) to send multiple-key commands.

See the [example section](#examples-2) for details.

##### Publish/Subscribe

You can publish and subscribe messages with `RedisCluster`. The interfaces are exactly the same as `Redis`, i.e. use `RedisCluster::publish` to publish messages, and use `RedisCluster::subscriber` to create a subscriber to consume messages. See [Publish/Subscribe section](#publishsubscribe) for details.

##### Pipeline and Transaction

You can also create `Pipeline` and `Transaction` objects with `RedisCluster`, but the interfaces are different from `Redis`. Since all commands in the pipeline and transaction should be sent to a single node in a single connection, we need to tell `RedisCluster` with which node the pipeline or transaction should be created.

Instead of specifying the node's IP and port, `RedisCluster`'s pipeline and transaction interfaces allow you to specify the node with a *hash tag*. `RedisCluster` will calculate the slot number with the given *hash tag*, and create a pipeline or transaction with the node holding the slot.

```C++
Pipeline RedisCluster::pipeline(const StringView &hash_tag, bool new_connection = true);

Transaction RedisCluster::transaction(const StringView &hash_tag, bool piped = false, bool new_connection = true);
```

With the created `Pipeline` or `Transaction` object, you can send commands with keys located on the same node as the given *hash_tag*. See [Examples section](#examples-2) for an example.

**NOTE**: By default, `Pipeline` and `Transaction` will be created with a new connection. In order to avoid creating new connection, you can pass `false` as the last parameter. However, in this case, you MUST be very careful, otherwise, you might get bad performance or even dead lock. Please carefully check the related [pipeline section](#very-important-notes) before using this feature.

#### Examples

```C++
#include <sw/redis++/redis++.h>

using namespace sw::redis;

auto redis_cluster = RedisCluster("tcp://127.0.0.1:7000");

redis_cluster.set("key", "value");
auto val = redis_cluster.get("key");
if (val) {
    std::cout << *val << std::endl;
}

// With hash-tag.
redis_cluster.set("key{tag}1", "val1");
redis_cluster.set("key{tag}2", "val2");
redis_cluster.set("key{tag}3", "val3");
std::vector<OptionalString> hash_tag_res;
redis_cluster.mget({"key{tag}1", "key{tag}2", "key{tag}3"},
        std::back_inserter(hash_tag_res));

redis_cluster.lpush("list", {"1", "2", "3"});
std::vector<std::string> list;
redis_cluster.lrange("list", 0, -1, std::back_inserter(list));

// Pipeline.
auto pipe = redis_cluster.pipeline("counter");
auto replies = pipe.incr("{counter}:1").incr("{counter}:2").exec();

// Transaction.
auto tx = redis_cluster.transaction("key");
replies = tx.incr("key").get("key").exec();

// Create a Redis object with hash-tag.
// It connects to the Redis instance that holds the given key, i.e. hash-tag.
auto r = redis_cluster.redis("hash-tag");

// And send command without key parameter to the server.
r.command("client", "setname", "connection-name");
```

**NOTE**: By default, when you use `RedisCluster::redis(const StringView &hash_tag, bool new_connection = true)` to create a `Redis` object, instead of picking a connection from the underlying connection pool, it creates a new connection to the corresponding Redis server. So this is NOT a cheap operation, and you should try to reuse this newly created `Redis` object as much as possible. If you pass `false` as the second parameter, you can create a `Redis` object without creating a new connection. However, in this case, you should be very careful, otherwise, you might get bad performance or even dead lock. Please carefully check the related [pipeline section](#very-important-notes) before using this feature.

```C++
// This is BAD! It's very inefficient.
// NEVER DO IT!!!
// After sending PING command, the newly created Redis object will be destroied.
cluster.redis("key").ping();

// Then it creates a connection to Redis, and closes the connection after sending the command.
cluster.redis("key").command("client", "setname", "hello");

// Instead you should reuse the Redis object.
// This is GOOD!
auto redis = cluster.redis("key");

redis.ping();
redis.command("client", "setname", "hello");

// This is GOOD! Create `Redis` object without creating a new connection. Use it, and destroy it ASAP.
cluster.redis("key", false).ping();
```

#### Details

`RedisCluster` maintains the newest slot-node mapping, and sends command directly to the right node. Normally it works as fast as `Redis`. If the cluster reshards, `RedisCluster` will follow the redirection, and it will finally update the slot-node mapping. It can correctly handle the following resharding cases:

- Data migration between exist nodes.
- Add new node to the cluster.
- Remove node from the cluster.

`redis-plus-plus` is able to handle both [MOVED](https://redis.io/topics/cluster-spec#moved-redirection) and [ASK](https://redis.io/topics/cluster-spec#ask-redirection) redirections, so it's a complete Redis Cluster client.

If master is down, the cluster will promote one of its replicas to be the new master. *redis-plus-plus* can also handle this case:

- When the master is down, *redis-plus-plus* losts connection to it. In this case, if you try to send commands to this master, *redis-plus-plus* will try to update slot-node mapping from other nodes. If the mapping remains unchanged, i.e. new master hasn't been elected yet, it fails to send command to Redis Cluster and throws exception.
- When the new master has been elected, the slot-node mapping will be updated by the cluster. In this case, if you send commands to the cluster, *redis-plus-plus* can get an update-to-date mapping, and sends commands to the new master.

### Redis Sentinel

[Redis Sentinel provides high availability for Redis](https://redis.io/topics/sentinel). If Redis master is down, Redis Sentinels will elect a new master from slaves, i.e. failover. Besides, Redis Sentinel can also act like a configuration provider for clients, and clients can query master or slave address from Redis Sentinel. So that if a failover occurs, clients can ask the new master address from Redis Sentinel.

*redis-plus-plus* supports getting Redis master or slave's IP and port from Redis Sentinel. In order to use this feature, you only need to initialize `Redis` object with Redis Sentinel info, which is composed with 3 parts: `std::shared_ptr<Sentinel>`, master name and role (master or slave).

Before using Redis Sentinel with *redis-plus-plus*, ensure that you have read Redis Sentinel's [doc](https://redis.io/topics/sentinel).

#### Sentinel

You can create a `std::shared_ptr<Sentinel>` object with `SentinelOptions`.

```C++
SentinelOptions sentinel_opts;
sentinel_opts.nodes = {{"127.0.0.1", 9000},
                        {"127.0.0.1", 9001},
                        {"127.0.0.1", 9002}};   // Required. List of Redis Sentinel nodes.

// Optional. Timeout before we successfully connect to Redis Sentinel.
// By default, the timeout is 100ms.
sentinel_opts.connect_timeout = std::chrono::milliseconds(200);

// Optional. Timeout before we successfully send request to or receive response from Redis Sentinel.
// By default, the timeout is 100ms.
sentinel_opts.socket_timeout = std::chrono::milliseconds(200);

auto sentinel = std::make_shared<Sentinel>(sentinel_opts);
```

`SentinelOptions::connect_timeout` and `SentinelOptions::socket_timeout` CANNOT be 0ms, i.e. no timeout and block forever. Otherwise, *redis-plus-plus* will throw an exception.

See [SentinelOptions](https://github.com/sewenew/redis-plus-plus/blob/master/src/sw/redis%2B%2B/sentinel.h#L33) for more options.

#### Role

Besides `std::shared_ptr<Sentinel>` and master name, you also need to specify a role. There are two roles: `Role::MASTER`, and `Role::SLAVE`.

With `Role::MASTER`, *redis-plus-plus* will always connect to current master instance, even if a failover occurs. Each time when *redis-plus-plus* needs to create a new connection to master, or a connection is broken, and it needs to reconnect to master, *redis-plus-plus* will ask master address from Redis Sentinel, and connects to current master. If a failover occurs, *redis-plus-plus* can automatically get the address of the new master, and refresh all connections in the underlying connection pool.

Similarly, with `Role::SLAVE`, *redis-plus-plus* will always connect to a slave instance. A master might have several slaves, *redis-plus-plus* will randomly pick one, and connect to it, i.e. all connections in the underlying connection pool, connect to the same slave instance (check [this discussion](https://github.com/sewenew/redis-plus-plus/issues/99) on why *redis-plus-plus* not connect to all slaves). If the connection is broken, while this slave instance is still an alive slave, *redis-plus-plus* will reconnect to this slave. However, if this slave instance is down, or it has been promoted to be the master, *redis-plus-plus* will randomly connect to another slave. If there's no slave alive, it throws an exception.

#### Create Redis With Sentinel

When creating a `Redis` object with sentinel, besides the sentinel info, you should also provide `ConnectionOptions` and `ConnectionPoolOptions`. These two options are used to connect to Redis instance. `ConnectionPoolOptions` is optional, if not specified, it creates a single connection the instance.

```C++
ConnectionOptions connection_opts;
connection_opts.password = "auth";  // Optional. No password by default.
connection_opts.connect_timeout = std::chrono::milliseconds(100);   // Required.
connection_opts.socket_timeout = std::chrono::milliseconds(100);    // Required.

ConnectionPoolOptions pool_opts;
pool_opts.size = 3; // Optional. The default size is 1.

auto redis = Redis(sentinel, "master_name", Role::MASTER, connection_opts, pool_opts);
```

You might have noticed that we didn't specify the `host` and `port` fields for `ConnectionOptions`. Because, `Redis` will get these info from Redis Sentinel. Also, in this case, `ConnectionOptions::connect_timeout` and `ConnectionOptions::socket_timeout` CANNOT be 0ms, otherwise, it throws an exception. So you always need to specify these two timeouts manually.

After creating the `Redis` object with sentinel, you can send commands with it, just like an ordinary `Redis` object.

If you want to write to master, and scale read with slaves. You can use the following pattern:

```C++
auto sentinel = std::make_shared<Sentinel>(sentinel_opts);

auto master = Redis(sentinel, "master_name", Role::MASTER, connection_opts, pool_opts);

auto slave = Redis(sentinel, "master_name", Role::SLAVE, connection_opts, pool_opts);

// Write to master.
master.set("key", "value");

// Read from slave.
slave.get("key");
```

### Redis Stream

Since Redis 5.0, it introduces a new data type: *Redis Stream*. *redis-plus-plus* has built-in methods for all stream commands except the *XINFO* command (of course, you can use the [Generic Command Interface](#generic-command-interface) to send *XINFO* command).

However, the replies of some streams commands, i.e. *XPENDING*, *XREAD*, are complex. So I'll give some examples to show you how to work with these built-in methods.

#### Examples

```C++
auto redis = Redis("tcp://127.0.0.1");

using Attrs = std::vector<std::pair<std::string, std::string>>;

// You can also use std::unordered_map, if you don't care the order of attributes:
// using Attrs = std::unordered_map<std::string, std::string>;

Attrs attrs = { {"f1", "v1"}, {"f2", "v2"} };

// Add an item into the stream. This method returns the auto generated id.
auto id = redis.xadd("key", "*", attrs.begin(), attrs.end());

// Each item is assigned with an id: pair<id, Optional<attributes>>.
// NOTE: the attribute part might be nil reply, check [this issue](https://github.com/sewenew/redis-plus-plus/issues/283) for detail.
using Item = std::pair<std::string, Optional<Attrs>>;
using ItemStream = std::vector<Item>;

// If you don't care the order of items in the stream, you can also use unordered_map:
// using ItemStream = std::unordered_map<std::string, Attrs>;

// Read items from a stream, and return at most 10 items.
// You need to specify a key and an id (timestamp + offset).
std::unordered_map<std::string, ItemStream> result;
redis.xread("key", id, 10, std::inserter(result, result.end()));

// Read from multiple streams. For each stream, you need to specify a key and an id.
std::unordered_map<std::string, std::string> keys = { {"key", id}, {"another-key", "0-0"} };
redis.xread(keys.begin(), keys.end(), 10, std::inserter(result, result.end()));

// Block for at most 1 second if currently there's no data in the stream.
redis.xread("key", id, std::chrono::seconds(1), 10, std::inserter(result, result.end()));

// Block for multiple streams.
redis.xread(keys.begin(), keys.end(), std::chrono::seconds(1), 10, std::inserter(result, result.end()));

// Read items in a range:
ItemStream item_stream;
redis.xrange("key", "-", "+", std::back_inserter(item_stream));

// Trim the stream to a given number of items. After the operation, the stream length is NOT exactly
// 10. Instead, it might be much larger than 10.
// `XTRIM key MAXLEN 10`
redis.xtrim("key", 10);

// In order to trim the stream to exactly 10 items, specify the third argument, i.e. approx, as false.
// `XTRIM key MAXLEN ~ 10`
redis.xtrim("key", 10, false);

// Delete an item from the stream.
redis.xdel("key", id);

// Create a consumer group.
redis.xgroup_create("key", "group", "$");

// If the stream doesn't exist, you can set the fourth argument, i.e. MKSTREAM, to be true.
// redis.xgroup_create("key", "group", "$", true);

id = redis.xadd("key", "*", attrs.begin(), attrs.end());

// Read item by a consumer of a consumer group.
redis.xreadgroup("group", "consumer", "key", ">", 1, std::inserter(result, result.end()));

using PendingItem = std::tuple<std::string, std::string, long long, long long>;
std::vector<PendingItem> pending_items;

// Get pending items of a speicified consumer.
redis.xpending("key", "group", "-", "+", 1, "consumer", std::back_inserter(pending_items));

redis.xack("key", "group", id);

redis.xgroup_delconsumer("key", "group", "consumer");
redis.xgroup_destroy("key", "group");
```

If you have any problem on sending stream commands to Redis, please feel free to let me know.

### Redis Modules

[Redis Modules](https://redis.io/modules) enrich Redis. However, *redis-plus-plus* does not have built-in support/method for these modules, although you can use the [generic interface](#generic-command-interface) to send commands related to these modules.

Fortunately, [@wingunder](https://github.com/wingunder) did a great job to make the work easier. He wrote [redis-plus-plus-modules](https://github.com/wingunder/redis-plus-plus-modules), which is a header only project that has built-in support for some popular modules. If you need to work with Redis Modules, you should have a try.

@wingunder also contributes a lot to *redis-plus-plus*. Many thanks to @wingunder!

### Async Interface

*redis-plus-plus* also supports async interface, however, async support for Transaction and Subscriber is still on the way.

The async interface depends on third-party event library, and so far, only libuv is supported.

#### Installation

You must install *libuv*(e.g. *apt-get install libuv1-dev*) before install *hiredis* and *redis-plus-plus*.

*hiredis* v1.0.0's async interface is different from older version, and *redis-plus-plus* only supports *hiredis* v1.0.0 or later. So you need to ensure you've installed the right version of hiredis before installing *redis-plus-plus*. Also, you should NEVER install multiple versions of *hiredis*, otherwise, you'll get some wired problems. If you already installed an older version, remove it, and install a newer version.

When installing *redis-plus-plus*, you should specify the following command line option: `-DREDIS_PLUS_PLUS_BUILD_ASYNC=libuv`.

```
cmake -DCMAKE_PREFIX_PATH=/installation/path/to/libuv/and/hiredis -DREDIS_PLUS_PLUS_BUILD_ASYNC=libuv ..

make

make install
```

#### Getting Started

The async interface is similar to sync interface, except that you should include *sw/redis++/async_redis++.h*, and define an object of `sw::redis::AsyncRedis`, and the related methods return `Future` object (so far, only `std::future` and `boost::future` are supported, support for other implementations of *future* is on the way).

**NOTE**: When building your application code, don't forget to link libuv.

```
#include <sw/redis++/async_redis++.h>

ConnectionOptions opts;
opts.host = "127.0.0.1";
opts.port = 6379;

ConnectionPoolOptions pool_opts;
pool_opts.size = 3;

auto async_redis = AsyncRedis(opts, pool_opts);

Future<string> ping_res = async_redis.ping();

Future<bool> set_res = async_redis.set("key", "val");

Future<Optional<string>> get_res = async_redis.get("key");

unordered_map<string, string> m = {{"a", "b"}, {"c", "d"}};
Future<void> hmset_res = async_redis.hmset("hash", m.begin(), m.end());

auto hgetall_res = async_redis.hgetall<vector<string>>("hash");

cout << ping_res.get() << endl;
cout << set_res.get() << endl;
auto val = get_res.get();
if (val)
    cout << *val << endl;
else
    cout << "not exist" << endl;

hmset_res.get();

for (const auto &ele : hgetall_res.get())
    cout << ele << endl;

// Generic interface.

// There's no *AsyncRedis::client_getname* interface.
// But you can use *Redis::command* to get the client name.
auto getname_res = redis.command<OptionalString>("client", "getname");
val = getname_res.get();
if (val) {
    std::cout << *val << std::endl;
}
```

#### Redis Sentinel

Aysnc interface also supports Redis Sentinel.

```
#include <sw/redis++/async_redis++.h>

SentinelOptions sentinel_opts;
sentinel_opts.nodes = {
    {"127.0.0.1", 8000},
    {"127.0.0.1", 8001},
    {"127.0.0.1", 8002}
};

sentinel_opts.connect_timeout = std::chrono::milliseconds(100);
sentinel_opts.socket_timeout = std::chrono::milliseconds(100);

auto sentinel = std::make_shared<AsyncSentinel>(sentinel_opts);

onnectionOptions connection_opts;
connection_opts.connect_timeout = std::chrono::milliseconds(100);   // Required.
connection_opts.socket_timeout = std::chrono::milliseconds(100);   // Required.

ConnectionPoolOptions pool_opts;
pool_opts.size = 3; // Optional. The default size is 1.

// Connect to master node.
AsyncRedis redis(sentinel, "mymaster", Role::MASTER, connection_opts, pool_opts);

// The following code randomly connects to one of the slave nodes.
// AsyncRedis redis(sentinel, "mymaster", Role::SLAVE, connection_opts, pool_opts);

redis.set("key", "value");

auto value = redis.get("key").get();
```

The async support for sentinel is similar with the sync one, except that you need to create an `AsyncSentinel` object instead of a `Sentinel` object. Check [Redis Sentinel](#redis-sentinel) for more details on `SentinelOptions`, `ConnectionOptions` and `Role`.

#### Redis Cluster

Aysnc interface also supports Redis Cluster. Instead of `AsyncRedis`, you need to create an `AsyncRedisCluster` object.

```
ConnectionOptions opts;
opts.host = "127.0.0.1";
opts.port = 6379;

ConnectionPoolOptions pool_opts;
pool_opts.size = 3;

auto async_cluster = AsyncRedisCluster(opts, pool_opts);

Future<bool> set_res = async_cluster.set("key", "val");

Future<Optional<string>> get_res = async_cluster.get("key");

auto mget_res = async_cluster.mget<std::vector<OptionalString>>({"{hashtag}key1", "{hashhag}key2", "{hashtag}key3"});

unordered_map<string, string> m = {{"a", "b"}, {"c", "d"}};
Future<void> hmset_res = async_redis.hmset("hash", m.begin(), m.end());
```

#### Event Loop

By default, `AsyncRedis` and `AsyncRedisCluster` create a default event loop, and runs the loop in a dedicated thread to handle read and write operations. However, you can also share the underlying event loop with multiple `AsyncRedis` and `AsyncRedisCluster` objects. In order to do that, you need to create a `std::shared_ptr<EventLoop>`, and pass it to the constructors of `AsyncRedis` and `AsyncRedisCluster`.

```
auto event_loop = std::make_shared<EventLoop>();

auto redis = AsyncRedis(connection_opts, pool_opts, loop);

auto cluster = AsyncRedisCluster(connection_opts, pool_opts, Role::MASTER, loop);
```

#### Future with Continuation

Unfortunately, `std::future` doesn't support [continuation](https://en.cppreference.com/w/cpp/experimental/future/then) so far, which is inconvenient. However, some other libraries, e.g. boost and folly, have continuation support.

By default, *redis-plus-plus* returns `std::future` for async interface. However, you can also make it return `boost::future` by specifying `-DREDIS_PLUS_PLUS_ASYNC_FUTURE=boost` when running cmake (`folly` and other libraries might be supported in the future). Of course, in this case, you need to install boost first.

```
cmake -DREDIS_PLUS_PLUS_BUILD_ASYNC=libuv -DREDIS_PLUS_PLUS_ASYNC_FUTURE=boost ..
```

**NOTE**: When building your application code, don't forget to link boost related libs, e.g. -lboost_thread, -lboost_system.

Then you can take advantage of `boost::future`'s continuation support:

```
#include <sw/redis++/async_redis++.h>

ConnectionOptions opts;
opts.host = "127.0.0.1";
opts.port = 6379;
auto redis = AsyncRedis(opts);
auto fut = redis.get("key").then([](sw::redis::Future<sw::redis::Optional<std::string>> fut) {
                                    auto val = fut.get();
                                    if (val) cout << *val << endl;
                                });
// Do other things

// Wait for the continuation finishes.
fut.get();
```

You can also use a thread pool to run the continuation:

```
#define BOOST_THREAD_PROVIDES_EXECUTORS

// You might also need to `#define BOOST_THREAD_USES_MOVE` with some version of Boost.
// See [this issue](https://github.com/sewenew/redis-plus-plus/issues/272) for detail.

#include <sw/redis++/async_redis++.h>
#include <boost/thread/executors/basic_thread_pool.hpp>

boost::executors::basic_thread_pool pool(3);
auto fut = redis.get("key").then(pool,
        [](sw::redis::Future<sw::redis::Optional<std::string>> fut) {
            auto val = fut.get();
            if (val) cout << *val << endl;
        });

// Do other things

fut.get();
```

## Redis Recipes

We can create many interesting data structures and algorithms based on Redis, such as [Redlock](https://redis.io/topics/distlock). We call these data structures and algorithms as **Redis Recipes**. *redis-plus-plus* will support some of these recipes.

**NOTE**: These recipes will be first implemented on the [recipes branch](https://github.com/sewenew/redis-plus-plus/tree/recipes). I'd like to hear your feedback on the API of these recipes, and when these APIs become stable, I'll merge the code into the master branch. So APIs on the *recipes* branch are NOT stable, and might be changed in the future.

### Redlock

[Redlock](https://redis.io/topics/distlock) is a distributed lock based on Redis. Thanks to @wingunder's [suggestion](https://github.com/sewenew/redis-plus-plus/issues/24), *redis-plus-plus* supports Redlock now. @wingunder and I made two different implementation of Redlock: one based on Lua script, and the other based on transaction. The Lua script version should be faster, and also it has many other parameters to control the behavior. However, if you are not allowed to, or don't want to run Lua scripts inside Redis, you could try using the transaction version.

#### Examples

```
auto redis1 = Redis("tcp://127.0.0.1:7000");
auto redis2 = Redis("tcp://127.0.0.1:7001");
auto redis3 = Redis("tcp://127.0.0.1:7002");

// Lua script version:
{
    RedLockMutex mtx({redis1, redis2, redis3}, "resource");

    // Not locked.
    RedLock<RedLockMutex> lock(mtx, std::defer_lock);

    // Try to get the lock, and keep 30 seconds.
    // It returns the validity time of the lock, i.e. the lock is only
    // valid in *validity_time*, after that the lock might be acquired by others.
    // If failed to acquire the lock, throw an exception of Error type.
    auto validity_time = lock.try_lock(std::chrono::seconds(30));

    // Extend the lock before the lock expired.
    validity_time = lock.extend_lock(std::chrono::seconds(10));

    // You can unlock explicitly.
    lock.unlock();
} // If unlock() is not called, the lock will be unlocked automatically when it's destroied.

// Transaction version:
{
    RedMutex mtx({redis1, redis2, redis3}, "resource");

    RedLock<RedMutex> lock(mtx, std::defer_lock);
    auto validity_time = lock.try_lock(std::chrono::seconds(30));
    validity_time = lock.extend_lock(std::chrono::seconds(30));

    // You can unlock explicitly.
    lock.unlock();
}
```

Please refer to the [code](https://github.com/sewenew/redis-plus-plus/blob/recipes/src/sw/redis%2B%2B/recipes/redlock.h) for detail. I'll enhance the doc in the future.

## Author

*redis-plus-plus* is written by sewenew, who is also active on [StackOverflow](https://stackoverflow.com/users/5384363/for-stack).
