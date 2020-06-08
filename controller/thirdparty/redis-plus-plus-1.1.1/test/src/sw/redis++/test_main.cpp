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

#include <unistd.h>
#include <chrono>
#include <tuple>
#include <iostream>
#include <sw/redis++/redis++.h>
#include "sanity_test.h"
#include "connection_cmds_test.h"
#include "keys_cmds_test.h"
#include "string_cmds_test.h"
#include "list_cmds_test.h"
#include "hash_cmds_test.h"
#include "set_cmds_test.h"
#include "zset_cmds_test.h"
#include "hyperloglog_cmds_test.h"
#include "geo_cmds_test.h"
#include "script_cmds_test.h"
#include "pubsub_test.h"
#include "pipeline_transaction_test.h"
#include "threads_test.h"
#include "stream_cmds_test.h"
#include "benchmark_test.h"

namespace {

void print_help();

auto parse_options(int argc, char **argv)
    -> std::tuple<sw::redis::Optional<sw::redis::ConnectionOptions>,
                    sw::redis::Optional<sw::redis::ConnectionOptions>,
                    sw::redis::Optional<sw::redis::test::BenchmarkOptions>>;

template <typename RedisInstance>
void run_test(const sw::redis::ConnectionOptions &opts);

template <typename RedisInstance>
void run_benchmark(const sw::redis::ConnectionOptions &opts,
        const sw::redis::test::BenchmarkOptions &benchmark_opts);

}

int main(int argc, char **argv) {
    try {
        sw::redis::Optional<sw::redis::ConnectionOptions> opts;
        sw::redis::Optional<sw::redis::ConnectionOptions> cluster_node_opts;
        sw::redis::Optional<sw::redis::test::BenchmarkOptions> benchmark_opts;
        std::tie(opts, cluster_node_opts, benchmark_opts) = parse_options(argc, argv);

        if (opts) {
            std::cout << "Testing Redis..." << std::endl;

            if (benchmark_opts) {
                run_benchmark<sw::redis::Redis>(*opts, *benchmark_opts);
            } else {
                run_test<sw::redis::Redis>(*opts);
            }
        }

        if (cluster_node_opts) {
            std::cout << "Testing RedisCluster..." << std::endl;

            if (benchmark_opts) {
                run_benchmark<sw::redis::RedisCluster>(*cluster_node_opts, *benchmark_opts);
            } else {
                run_test<sw::redis::RedisCluster>(*cluster_node_opts);
            }
        }

        std::cout << "Pass all tests" << std::endl;
    } catch (const sw::redis::Error &e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}

namespace {

void print_help() {
    std::cerr << "Usage: test_redis++ -h host -p port"
        << " -n cluster_node -c cluster_port [-a auth] [-b]\n\n";
    std::cerr << "See https://github.com/sewenew/redis-plus-plus#run-tests-optional"
        << " for details on how to run test" << std::endl;
}

auto parse_options(int argc, char **argv)
    -> std::tuple<sw::redis::Optional<sw::redis::ConnectionOptions>,
                    sw::redis::Optional<sw::redis::ConnectionOptions>,
                    sw::redis::Optional<sw::redis::test::BenchmarkOptions>> {
    std::string host;
    int port = 0;
    std::string auth;
    std::string cluster_node;
    int cluster_port = 0;
    bool benchmark = false;
    sw::redis::test::BenchmarkOptions tmp_benchmark_opts;

    int opt = 0;
    while ((opt = getopt(argc, argv, "h:p:a:n:c:k:v:r:t:bs:")) != -1) {
        try {
            switch (opt) {
            case 'h':
                host = optarg;
                break;

            case 'p':
                port = std::stoi(optarg);
                break;

            case 'a':
                auth = optarg;
                break;

            case 'n':
                cluster_node = optarg;
                break;

            case 'c':
                cluster_port = std::stoi(optarg);
                break;

            case 'b':
                benchmark = true;
                break;

            case 'k':
                tmp_benchmark_opts.key_len = std::stoi(optarg);
                break;

            case 'v':
                tmp_benchmark_opts.val_len = std::stoi(optarg);
                break;

            case 'r':
                tmp_benchmark_opts.total_request_num = std::stoi(optarg);
                break;

            case 't':
                tmp_benchmark_opts.thread_num = std::stoi(optarg);
                break;

            case 's':
                tmp_benchmark_opts.pool_size = std::stoi(optarg);
                break;

            default:
                throw sw::redis::Error("Unknow command line option");
                break;
            }
        } catch (const sw::redis::Error &e) {
            print_help();
            throw;
        } catch (const std::exception &e) {
            print_help();
            throw sw::redis::Error("Invalid command line option");
        }
    }

    sw::redis::Optional<sw::redis::ConnectionOptions> opts;
    if (!host.empty() && port > 0) {
        sw::redis::ConnectionOptions tmp;
        tmp.host = host;
        tmp.port = port;
        tmp.password = auth;

        opts = sw::redis::Optional<sw::redis::ConnectionOptions>(tmp);
    }

    sw::redis::Optional<sw::redis::ConnectionOptions> cluster_opts;
    if (!cluster_node.empty() && cluster_port > 0) {
        sw::redis::ConnectionOptions tmp;
        tmp.host = cluster_node;
        tmp.port = cluster_port;
        tmp.password = auth;

        cluster_opts = sw::redis::Optional<sw::redis::ConnectionOptions>(tmp);
    }

    if (!opts && !cluster_opts) {
        print_help();
        throw sw::redis::Error("Invalid connection options");
    }

    sw::redis::Optional<sw::redis::test::BenchmarkOptions> benchmark_opts;
    if (benchmark) {
        benchmark_opts = sw::redis::Optional<sw::redis::test::BenchmarkOptions>(tmp_benchmark_opts);
    }

    return std::make_tuple(std::move(opts), std::move(cluster_opts), std::move(benchmark_opts));
}

template <typename RedisInstance>
void run_test(const sw::redis::ConnectionOptions &opts) {
    auto instance = RedisInstance(opts);

    sw::redis::test::SanityTest<RedisInstance> sanity_test(opts, instance);
    sanity_test.run();

    std::cout << "Pass sanity tests" << std::endl;

    sw::redis::test::ConnectionCmdTest<RedisInstance> connection_test(instance);
    connection_test.run();

    std::cout << "Pass connection commands tests" << std::endl;

    sw::redis::test::KeysCmdTest<RedisInstance> keys_test(instance);
    keys_test.run();

    std::cout << "Pass keys commands tests" << std::endl;

    sw::redis::test::StringCmdTest<RedisInstance> string_test(instance);
    string_test.run();

    std::cout << "Pass string commands tests" << std::endl;

    sw::redis::test::ListCmdTest<RedisInstance> list_test(instance);
    list_test.run();

    std::cout << "Pass list commands tests" << std::endl;

    sw::redis::test::HashCmdTest<RedisInstance> hash_test(instance);
    hash_test.run();

    std::cout << "Pass hash commands tests" << std::endl;

    sw::redis::test::SetCmdTest<RedisInstance> set_test(instance);
    set_test.run();

    std::cout << "Pass set commands tests" << std::endl;

    sw::redis::test::ZSetCmdTest<RedisInstance> zset_test(instance);
    zset_test.run();

    std::cout << "Pass zset commands tests" << std::endl;

    sw::redis::test::HyperloglogCmdTest<RedisInstance> hll_test(instance);
    hll_test.run();

    std::cout << "Pass hyperloglog commands tests" << std::endl;

    sw::redis::test::GeoCmdTest<RedisInstance> geo_test(instance);
    geo_test.run();

    std::cout << "Pass geo commands tests" << std::endl;

    sw::redis::test::ScriptCmdTest<RedisInstance> script_test(instance);
    script_test.run();

    std::cout << "Pass script commands tests" << std::endl;

    sw::redis::test::PubSubTest<RedisInstance> pubsub_test(instance);
    pubsub_test.run();

    std::cout << "Pass pubsub tests" << std::endl;

    sw::redis::test::PipelineTransactionTest<RedisInstance> pipe_tx_test(instance);
    pipe_tx_test.run();

    std::cout << "Pass pipeline and transaction tests" << std::endl;

    sw::redis::test::ThreadsTest<RedisInstance> threads_test(opts);
    threads_test.run();

    std::cout << "Pass threads tests" << std::endl;

    sw::redis::test::StreamCmdsTest<RedisInstance> stream_test(instance);
    stream_test.run();

    std::cout << "Pass stream commands tests" << std::endl;
}

template <typename RedisInstance>
void run_benchmark(const sw::redis::ConnectionOptions &opts,
        const sw::redis::test::BenchmarkOptions &benchmark_opts) {
    std::cout << "Benchmark test options:" << std::endl;
    std::cout << "  Thread number: " << benchmark_opts.thread_num << std::endl;
    std::cout << "  Connection pool size: " << benchmark_opts.pool_size << std::endl;
    std::cout << "  Length of key: " << benchmark_opts.key_len << std::endl;
    std::cout << "  Length of value: " << benchmark_opts.val_len << std::endl;

    auto instance = RedisInstance(opts);

    sw::redis::test::BenchmarkTest<RedisInstance> benchmark_test(benchmark_opts, instance);
    benchmark_test.run();
}

}
