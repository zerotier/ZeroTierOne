# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "ostream_log_record_exporter",
    srcs = [
        "src/log_record_exporter.cc",
        "src/log_record_exporter_factory.cc",
    ],
    hdrs = [
        "include/opentelemetry/exporters/ostream/common_utils.h",
        "include/opentelemetry/exporters/ostream/log_record_exporter.h",
        "include/opentelemetry/exporters/ostream/log_record_exporter_factory.h",
    ],
    strip_include_prefix = "include",
    tags = ["ostream"],
    deps = [
        "//sdk/src/logs",
    ],
)

cc_test(
    name = "ostream_log_test",
    srcs = ["test/ostream_log_test.cc"],
    tags = [
        "ostream",
        "test",
    ],
    deps = [
        ":ostream_log_record_exporter",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "ostream_metric_exporter",
    srcs = [
        "src/metric_exporter.cc",
        "src/metric_exporter_factory.cc",
    ],
    hdrs = [
        "include/opentelemetry/exporters/ostream/common_utils.h",
        "include/opentelemetry/exporters/ostream/metric_exporter.h",
        "include/opentelemetry/exporters/ostream/metric_exporter_factory.h",
    ],
    strip_include_prefix = "include",
    tags = [
        "metrics",
        "ostream",
    ],
    deps = [
        "//sdk/src/metrics",
    ],
)

cc_test(
    name = "ostream_metric_test",
    srcs = ["test/ostream_metric_test.cc"],
    tags = [
        "ostream",
        "test",
    ],
    deps = [
        ":ostream_metric_exporter",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "ostream_span_exporter",
    srcs = [
        "src/span_exporter.cc",
        "src/span_exporter_factory.cc",
    ],
    hdrs = [
        "include/opentelemetry/exporters/ostream/common_utils.h",
        "include/opentelemetry/exporters/ostream/span_exporter.h",
        "include/opentelemetry/exporters/ostream/span_exporter_factory.h",
    ],
    strip_include_prefix = "include",
    tags = ["ostream"],
    deps = [
        "//sdk/src/trace",
    ],
)

cc_library(
    name = "ostream_capture",
    hdrs = [
        "test/ostream_capture.h",
    ],
    tags = ["ostream"],
    deps = [
        "//api",
    ],
)

cc_test(
    name = "ostream_span_test",
    srcs = ["test/ostream_span_test.cc"],
    tags = [
        "ostream",
        "test",
    ],
    deps = [
        ":ostream_capture",
        ":ostream_span_exporter",
        "@com_google_googletest//:gtest_main",
    ],
)
