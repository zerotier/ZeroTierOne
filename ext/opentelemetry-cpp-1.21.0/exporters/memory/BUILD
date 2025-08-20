# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "in_memory_metric_data",
    srcs = [
        "src/in_memory_metric_data.cc",
    ],
    hdrs = [
        "include/opentelemetry/exporters/memory/in_memory_metric_data.h",
    ],
    strip_include_prefix = "include",
    tags = [
        "memory",
        "test",
    ],
    deps = [
        ":in_memory_data",
        "//sdk/src/metrics",
    ],
)

cc_test(
    name = "in_memory_metric_data_test",
    srcs = ["test/in_memory_metric_data_test.cc"],
    tags = [
        "memory",
        "test",
    ],
    deps = [
        ":in_memory_metric_data",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "in_memory_metric_exporter_factory",
    srcs = [
        "src/in_memory_metric_exporter_factory.cc",
    ],
    hdrs = [
        "include/opentelemetry/exporters/memory/in_memory_metric_exporter_factory.h",
    ],
    strip_include_prefix = "include",
    tags = [
        "memory",
        "test",
    ],
    deps = [
        ":in_memory_metric_data",
        "//sdk/src/metrics",
    ],
)

cc_test(
    name = "in_memory_metric_exporter_test",
    srcs = ["test/in_memory_metric_exporter_test.cc"],
    tags = [
        "memory",
        "test",
    ],
    deps = [
        ":in_memory_metric_exporter_factory",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "in_memory_data",
    hdrs = [
        "include/opentelemetry/exporters/memory/in_memory_data.h",
    ],
    strip_include_prefix = "include",
    tags = ["memory"],
    deps = [
        "//sdk:headers",
    ],
)

cc_library(
    name = "in_memory_span_data",
    hdrs = [
        "include/opentelemetry/exporters/memory/in_memory_span_data.h",
    ],
    strip_include_prefix = "include",
    tags = ["memory"],
    deps = [
        ":in_memory_data",
        "//api",
        "//sdk/src/resource",
        "//sdk/src/trace",
    ],
)

cc_test(
    name = "in_memory_span_data_test",
    srcs = ["test/in_memory_span_data_test.cc"],
    tags = [
        "memory",
        "test",
    ],
    deps = [
        ":in_memory_span_data",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "in_memory_span_exporter",
    srcs = [
        "src/in_memory_span_exporter_factory.cc",
    ],
    hdrs = [
        "include/opentelemetry/exporters/memory/in_memory_span_exporter.h",
        "include/opentelemetry/exporters/memory/in_memory_span_exporter_factory.h",
    ],
    strip_include_prefix = "include",
    tags = [
        "memory",
        "test",
    ],
    deps = [
        ":in_memory_span_data",
        "//sdk/src/trace",
    ],
)

cc_test(
    name = "in_memory_span_exporter_test",
    srcs = ["test/in_memory_span_exporter_test.cc"],
    tags = [
        "memory",
        "test",
    ],
    deps = [
        ":in_memory_span_exporter",
        "@com_google_googletest//:gtest_main",
    ],
)
