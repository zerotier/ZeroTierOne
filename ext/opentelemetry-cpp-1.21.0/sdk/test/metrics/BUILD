# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

load("//bazel:otel_cc_benchmark.bzl", "otel_cc_benchmark")

cc_library(
    name = "metrics_common_test_utils",
    srcs = [
        "common.cc",
    ],
    hdrs = ["common.h"],
    tags = [
        "metrics",
        "test",
    ],
    deps = [
        "//sdk/src/metrics",
    ],
)

cc_test(
    name = "meter_config_test",
    srcs = [
        "meter_config_test.cc",
    ],
    tags = [
        "metrics",
        "test",
    ],
    deps = [
        "metrics_common_test_utils",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "all_tests",
    srcs = glob(["*_test.cc"]),
    copts = [
        "-DUNIT_TESTING",
    ],
    tags = [
        "metrics",
        "test",
    ],
    deps = [
        "metrics_common_test_utils",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "stress_tests",
    timeout = "long",
    srcs = glob(["*_test_stress.cc"]),
    copts = [
        "-DUNIT_TESTING",
    ],
    tags = [
        "metrics",
        "test",
    ],
    deps = [
        "metrics_common_test_utils",
        "@com_google_googletest//:gtest_main",
    ],
)

otel_cc_benchmark(
    name = "attributes_processor_benchmark",
    srcs = [
        "attributes_processor_benchmark.cc",
    ],
    tags = [
        "benchmark",
        "metrics",
        "test",
    ],
    deps = [
        "metrics_common_test_utils",
    ],
)

otel_cc_benchmark(
    name = "attributes_hashmap_benchmark",
    srcs = [
        "attributes_hashmap_benchmark.cc",
    ],
    tags = [
        "benchmark",
        "metrics",
        "test",
    ],
    deps = [
        "metrics_common_test_utils",
    ],
)

otel_cc_benchmark(
    name = "base2_exponential_histogram_indexer_benchmark",
    srcs = [
        "base2_exponential_histogram_indexer_benchmark.cc",
    ],
    tags = [
        "benchmark",
        "metrics",
        "test",
    ],
    deps = [
        "metrics_common_test_utils",
    ],
)

otel_cc_benchmark(
    name = "histogram_aggregation_benchmark",
    srcs = [
        "histogram_aggregation_benchmark.cc",
    ],
    tags = [
        "benchmark",
        "metrics",
        "test",
    ],
    deps = [
        "metrics_common_test_utils",
        "//sdk/src/metrics",
        "//sdk/src/resource",
    ],
)

otel_cc_benchmark(
    name = "sum_aggregation_benchmark",
    srcs = [
        "sum_aggregation_benchmark.cc",
    ],
    tags = [
        "benchmark",
        "metrics",
        "test",
    ],
    deps = [
        "metrics_common_test_utils",
        "//sdk/src/metrics",
        "//sdk/src/resource",
    ],
)

otel_cc_benchmark(
    name = "measurements_benchmark",
    srcs = [
        "measurements_benchmark.cc",
    ],
    tags = [
        "benchmark",
        "metrics",
        "test",
    ],
    deps = [
        "//sdk/src/metrics",
        "//sdk/src/resource",
    ],
)
