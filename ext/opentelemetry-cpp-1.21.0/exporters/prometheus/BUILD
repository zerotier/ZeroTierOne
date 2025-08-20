# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "prometheus_exporter",
    srcs = [
        "src/exporter.cc",
        "src/exporter_factory.cc",
        "src/exporter_options.cc",
    ],
    hdrs = [
        "include/opentelemetry/exporters/prometheus/exporter.h",
        "include/opentelemetry/exporters/prometheus/exporter_factory.h",
        "include/opentelemetry/exporters/prometheus/exporter_options.h",
    ],
    strip_include_prefix = "include",
    tags = ["prometheus"],
    deps = [
        ":prometheus_collector",
        ":prometheus_exporter_utils",
        "//api",
        "//sdk:headers",
        "@com_github_jupp0r_prometheus_cpp//core",
        "@com_github_jupp0r_prometheus_cpp//pull",
    ],
)

cc_library(
    name = "prometheus_exporter_utils",
    srcs = [
        "src/exporter_utils.cc",
    ],
    hdrs = [
        "include/opentelemetry/exporters/prometheus/exporter_utils.h",
    ],
    strip_include_prefix = "include",
    tags = ["prometheus"],
    deps = [
        "//api",
        "//sdk:headers",
        "@com_github_jupp0r_prometheus_cpp//core",
        "@com_github_jupp0r_prometheus_cpp//pull",
    ],
)

cc_library(
    name = "prometheus_collector",
    srcs = [
        "src/collector.cc",
    ],
    hdrs = [
        "include/opentelemetry/exporters/prometheus/collector.h",
    ],
    strip_include_prefix = "include",
    tags = ["prometheus"],
    deps = [
        ":prometheus_exporter_utils",
        "//api",
        "//sdk:headers",
        "@com_github_jupp0r_prometheus_cpp//core",
        "@com_github_jupp0r_prometheus_cpp//pull",
    ],
)

cc_library(
    name = "prometheus_test_helper",
    hdrs = [
        "test/prometheus_test_helper.h",
    ],
    tags = ["prometheus"],
    deps = [
        "//api",
        "//sdk:headers",
        "//sdk/src/trace",
    ],
)

cc_test(
    name = "prometheus_exporter_test",
    srcs = [
        "test/exporter_test.cc",
    ],
    tags = [
        "prometheus",
        "test",
    ],
    deps = [
        ":prometheus_exporter",
        ":prometheus_test_helper",
        "//sdk/src/metrics",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "prometheus_collector_test",
    srcs = [
        "test/collector_test.cc",
    ],
    tags = [
        "prometheus",
        "test",
    ],
    deps = [
        ":prometheus_collector",
        ":prometheus_test_helper",
        "//sdk/src/metrics",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "prometheus_exporter_utils_test",
    srcs = [
        "test/exporter_utils_test.cc",
    ],
    tags = [
        "prometheus",
        "test",
    ],
    deps = [
        ":prometheus_exporter_utils",
        ":prometheus_test_helper",
        "@com_google_googletest//:gtest_main",
    ],
)
