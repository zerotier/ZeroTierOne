# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

load("//bazel:otel_cc_benchmark.bzl", "otel_cc_benchmark")

cc_test(
    name = "tracer_provider_test",
    srcs = [
        "tracer_provider_test.cc",
    ],
    tags = [
        "test",
        "trace",
    ],
    deps = [
        "//sdk/src/resource",
        "//sdk/src/trace",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "tracer_provider_set_test",
    srcs = [
        "tracer_provider_set_test.cc",
    ],
    tags = [
        "test",
        "trace",
    ],
    deps = [
        "//sdk/src/resource",
        "//sdk/src/trace",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "span_data_test",
    srcs = [
        "span_data_test.cc",
    ],
    tags = [
        "test",
        "trace",
    ],
    deps = [
        "//sdk/src/trace",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "simple_processor_test",
    srcs = [
        "simple_processor_test.cc",
    ],
    tags = [
        "test",
        "trace",
    ],
    deps = [
        "//exporters/memory:in_memory_span_exporter",
        "//sdk/src/trace",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "batch_span_processor_test",
    srcs = [
        "batch_span_processor_test.cc",
    ],
    tags = [
        "test",
        "trace",
    ],
    deps = [
        "//sdk/src/trace",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "tracer_test",
    srcs = [
        "tracer_test.cc",
    ],
    tags = [
        "test",
        "trace",
    ],
    deps = [
        "//exporters/memory:in_memory_span_exporter",
        "//sdk/src/resource",
        "//sdk/src/trace",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "always_on_sampler_test",
    srcs = [
        "always_on_sampler_test.cc",
    ],
    tags = [
        "test",
        "trace",
    ],
    deps = [
        "//sdk/src/trace",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "always_off_sampler_test",
    srcs = [
        "always_off_sampler_test.cc",
    ],
    tags = [
        "test",
        "trace",
    ],
    deps = [
        "//sdk/src/trace",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "parent_sampler_test",
    srcs = [
        "parent_sampler_test.cc",
    ],
    tags = [
        "test",
        "trace",
    ],
    deps = [
        "//sdk/src/trace",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "trace_id_ratio_sampler_test",
    srcs = [
        "trace_id_ratio_sampler_test.cc",
    ],
    tags = [
        "test",
        "trace",
    ],
    deps = [
        "//sdk/src/common:random",
        "//sdk/src/trace",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "tracer_config_test",
    srcs = [
        "tracer_config_test.cc",
    ],
    tags = [
        "test",
        "trace",
    ],
    deps = [
        "//sdk/src/trace",
        "@com_google_googletest//:gtest_main",
    ],
)

otel_cc_benchmark(
    name = "sampler_benchmark",
    srcs = ["sampler_benchmark.cc"],
    tags = [
        "benchmark",
        "test",
        "trace",
    ],
    deps = [
        "//exporters/memory:in_memory_span_exporter",
        "//sdk/src/resource",
        "//sdk/src/trace",
    ],
)
