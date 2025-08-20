# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "opentracing_shim",
    srcs = [
        "src/shim_utils.cc",
        "src/span_context_shim.cc",
        "src/span_shim.cc",
        "src/tracer_shim.cc",
    ],
    hdrs = [
        "include/opentelemetry/opentracingshim/propagation.h",
        "include/opentelemetry/opentracingshim/shim_utils.h",
        "include/opentelemetry/opentracingshim/span_context_shim.h",
        "include/opentelemetry/opentracingshim/span_shim.h",
        "include/opentelemetry/opentracingshim/tracer_shim.h",
    ],
    strip_include_prefix = "include",
    tags = ["opentracing"],
    deps = [
        "//api",
        "@com_github_opentracing//:opentracing",
    ],
)

cc_test(
    name = "propagation_test",
    srcs = [
        "test/propagation_test.cc",
        "test/shim_mocks.h",
    ],
    tags = [
        "opentracing_shim",
        "test",
    ],
    deps = [
        ":opentracing_shim",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "shim_utils_test",
    srcs = [
        "test/shim_mocks.h",
        "test/shim_utils_test.cc",
    ],
    tags = [
        "opentracing_shim",
        "test",
    ],
    deps = [
        ":opentracing_shim",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "span_shim_test",
    srcs = [
        "test/shim_mocks.h",
        "test/span_shim_test.cc",
    ],
    tags = [
        "opentracing_shim",
        "test",
    ],
    deps = [
        ":opentracing_shim",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "span_context_shim_test",
    srcs = [
        "test/span_context_shim_test.cc",
    ],
    tags = [
        "opentracing_shim",
        "test",
    ],
    deps = [
        ":opentracing_shim",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "tracer_shim_test",
    srcs = [
        "test/shim_mocks.h",
        "test/tracer_shim_test.cc",
    ],
    tags = [
        "opentracing_shim",
        "test",
    ],
    deps = [
        ":opentracing_shim",
        "@com_google_googletest//:gtest_main",
    ],
)
