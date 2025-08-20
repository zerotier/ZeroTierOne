# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

load("//bazel:otel_cc_benchmark.bzl", "otel_cc_benchmark")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "otlp_recordable",
    srcs = [
        "src/otlp_environment.cc",
        "src/otlp_log_recordable.cc",
        "src/otlp_metric_utils.cc",
        "src/otlp_populate_attribute_utils.cc",
        "src/otlp_recordable.cc",
        "src/otlp_recordable_utils.cc",
    ],
    hdrs = [
        "include/opentelemetry/exporters/otlp/otlp_environment.h",
        "include/opentelemetry/exporters/otlp/otlp_log_recordable.h",
        "include/opentelemetry/exporters/otlp/otlp_metric_utils.h",
        "include/opentelemetry/exporters/otlp/otlp_populate_attribute_utils.h",
        "include/opentelemetry/exporters/otlp/otlp_preferred_temporality.h",
        "include/opentelemetry/exporters/otlp/otlp_recordable.h",
        "include/opentelemetry/exporters/otlp/otlp_recordable_utils.h",
        "include/opentelemetry/exporters/otlp/protobuf_include_prefix.h",
        "include/opentelemetry/exporters/otlp/protobuf_include_suffix.h",
    ],
    strip_include_prefix = "include",
    tags = ["otlp"],
    deps = [
        "//sdk/src/logs",
        "//sdk/src/resource",
        "//sdk/src/trace",
        "@com_github_opentelemetry_proto//:logs_service_proto_cc",
        "@com_github_opentelemetry_proto//:metrics_service_proto_cc",
        "@com_github_opentelemetry_proto//:trace_service_proto_cc",
    ],
)

cc_library(
    name = "otlp_grpc_client",
    srcs = [
        "src/otlp_grpc_client.cc",
        "src/otlp_grpc_client_factory.cc",
        "src/otlp_grpc_utils.cc",
    ],
    hdrs = [
        "include/opentelemetry/exporters/otlp/otlp_environment.h",
        "include/opentelemetry/exporters/otlp/otlp_grpc_client.h",
        "include/opentelemetry/exporters/otlp/otlp_grpc_client_factory.h",
        "include/opentelemetry/exporters/otlp/otlp_grpc_client_options.h",
        "include/opentelemetry/exporters/otlp/otlp_grpc_utils.h",
        "include/opentelemetry/exporters/otlp/protobuf_include_prefix.h",
        "include/opentelemetry/exporters/otlp/protobuf_include_suffix.h",
    ],
    strip_include_prefix = "include",
    tags = [
        "otlp",
        "otlp_grpc",
    ],
    deps = [
        "//ext:headers",
        "//sdk/src/common:global_log_handler",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_opentelemetry_proto//:common_proto_cc",
        "@com_github_opentelemetry_proto//:logs_service_grpc_cc",
        "@com_github_opentelemetry_proto//:metrics_service_grpc_cc",
        "@com_github_opentelemetry_proto//:trace_service_grpc_cc",
    ],
)

cc_library(
    name = "otlp_grpc_exporter",
    srcs = [
        "src/otlp_grpc_exporter.cc",
        "src/otlp_grpc_exporter_factory.cc",
        "src/otlp_grpc_exporter_options.cc",
    ],
    hdrs = [
        "include/opentelemetry/exporters/otlp/otlp_environment.h",
        "include/opentelemetry/exporters/otlp/otlp_grpc_client_options.h",
        "include/opentelemetry/exporters/otlp/otlp_grpc_exporter.h",
        "include/opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h",
        "include/opentelemetry/exporters/otlp/otlp_grpc_exporter_options.h",
        "include/opentelemetry/exporters/otlp/otlp_grpc_utils.h",
        "include/opentelemetry/exporters/otlp/protobuf_include_prefix.h",
        "include/opentelemetry/exporters/otlp/protobuf_include_suffix.h",
    ],
    strip_include_prefix = "include",
    tags = [
        "otlp",
        "otlp_grpc",
    ],
    deps = [
        ":otlp_recordable",
        ":otlp_grpc_client",
        "//ext:headers",
        "//sdk/src/trace",

        # For gRPC
        "@com_github_opentelemetry_proto//:trace_service_grpc_cc",
    ],
)

cc_library(
    name = "otlp_http_client",
    srcs = [
        "src/otlp_http.cc",
        "src/otlp_http_client.cc",
    ],
    hdrs = [
        "include/opentelemetry/exporters/otlp/otlp_environment.h",
        "include/opentelemetry/exporters/otlp/otlp_http.h",
        "include/opentelemetry/exporters/otlp/otlp_http_client.h",
        "include/opentelemetry/exporters/otlp/protobuf_include_prefix.h",
        "include/opentelemetry/exporters/otlp/protobuf_include_suffix.h",
    ],
    copts = [
        "-DCURL_STATICLIB",
    ],
    linkopts = select({
        "//bazel:windows": [
            "-DEFAULTLIB:advapi32.lib",
            "-DEFAULTLIB:crypt32.lib",
        ],
        "//conditions:default": [],
    }),
    strip_include_prefix = "include",
    tags = [
        "otlp",
        "otlp_http",
        "otlp_http_log",
    ],
    deps = [
        "//api",
        "//ext/src/http/client/curl:http_client_curl",
        "//sdk:headers",
        "//sdk/src/common:base64",
        "@com_github_opentelemetry_proto//:common_proto_cc",
        "@com_google_absl//absl/strings",
        "@github_nlohmann_json//:json",
    ],
)

cc_library(
    name = "otlp_http_exporter",
    srcs = [
        "src/otlp_http_exporter.cc",
        "src/otlp_http_exporter_factory.cc",
        "src/otlp_http_exporter_options.cc",
    ],
    hdrs = [
        "include/opentelemetry/exporters/otlp/otlp_environment.h",
        "include/opentelemetry/exporters/otlp/otlp_http_exporter.h",
        "include/opentelemetry/exporters/otlp/otlp_http_exporter_factory.h",
        "include/opentelemetry/exporters/otlp/otlp_http_exporter_options.h",
        "include/opentelemetry/exporters/otlp/otlp_http_exporter_runtime_options.h",
        "include/opentelemetry/exporters/otlp/protobuf_include_prefix.h",
        "include/opentelemetry/exporters/otlp/protobuf_include_suffix.h",
    ],
    strip_include_prefix = "include",
    tags = [
        "otlp",
        "otlp_http",
    ],
    deps = [
        ":otlp_http_client",
        ":otlp_recordable",
        "//sdk/src/trace",
        "@com_github_opentelemetry_proto//:trace_service_proto_cc",
    ],
)

cc_library(
    name = "otlp_file_client",
    srcs = [
        "src/otlp_file_client.cc",
    ],
    hdrs = [
        "include/opentelemetry/exporters/otlp/otlp_environment.h",
        "include/opentelemetry/exporters/otlp/otlp_file_client.h",
        "include/opentelemetry/exporters/otlp/otlp_file_client_options.h",
        "include/opentelemetry/exporters/otlp/otlp_file_client_runtime_options.h",
        "include/opentelemetry/exporters/otlp/protobuf_include_prefix.h",
        "include/opentelemetry/exporters/otlp/protobuf_include_suffix.h",
    ],
    strip_include_prefix = "include",
    tags = [
        "otlp",
        "otlp_file",
    ],
    deps = [
        "//api",
        "//sdk:headers",
        "//sdk/src/common:base64",
        "@com_github_opentelemetry_proto//:common_proto_cc",
        "@com_google_absl//absl/strings",
        "@github_nlohmann_json//:json",
    ],
)

cc_library(
    name = "otlp_file_exporter",
    srcs = [
        "src/otlp_file_exporter.cc",
        "src/otlp_file_exporter_factory.cc",
        "src/otlp_file_exporter_options.cc",
    ],
    hdrs = [
        "include/opentelemetry/exporters/otlp/otlp_environment.h",
        "include/opentelemetry/exporters/otlp/otlp_file_exporter.h",
        "include/opentelemetry/exporters/otlp/otlp_file_exporter_factory.h",
        "include/opentelemetry/exporters/otlp/otlp_file_exporter_options.h",
        "include/opentelemetry/exporters/otlp/otlp_file_exporter_runtime_options.h",
        "include/opentelemetry/exporters/otlp/protobuf_include_prefix.h",
        "include/opentelemetry/exporters/otlp/protobuf_include_suffix.h",
    ],
    strip_include_prefix = "include",
    tags = [
        "otlp",
        "otlp_file",
    ],
    deps = [
        ":otlp_file_client",
        ":otlp_recordable",
        "//sdk/src/trace",
        "@com_github_opentelemetry_proto//:trace_service_proto_cc",
    ],
)

cc_library(
    name = "otlp_grpc_metric_exporter",
    srcs = [
        "src/otlp_grpc_metric_exporter.cc",
        "src/otlp_grpc_metric_exporter_factory.cc",
        "src/otlp_grpc_metric_exporter_options.cc",
    ],
    hdrs = [
        "include/opentelemetry/exporters/otlp/otlp_environment.h",
        "include/opentelemetry/exporters/otlp/otlp_grpc_client_options.h",
        "include/opentelemetry/exporters/otlp/otlp_grpc_metric_exporter.h",
        "include/opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h",
        "include/opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_options.h",
        "include/opentelemetry/exporters/otlp/protobuf_include_prefix.h",
        "include/opentelemetry/exporters/otlp/protobuf_include_suffix.h",
    ],
    strip_include_prefix = "include",
    tags = [
        "otlp",
        "otlp_grpc_metric",
    ],
    deps = [
        ":otlp_recordable",
        ":otlp_grpc_client",
        "//ext:headers",
        "//sdk/src/metrics",

        # For gRPC
        "@com_github_opentelemetry_proto//:metrics_service_grpc_cc",
    ],
)

cc_library(
    name = "otlp_http_metric_exporter",
    srcs = [
        "src/otlp_http_metric_exporter.cc",
        "src/otlp_http_metric_exporter_factory.cc",
        "src/otlp_http_metric_exporter_options.cc",
    ],
    hdrs = [
        "include/opentelemetry/exporters/otlp/otlp_environment.h",
        "include/opentelemetry/exporters/otlp/otlp_http_metric_exporter.h",
        "include/opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h",
        "include/opentelemetry/exporters/otlp/otlp_http_metric_exporter_options.h",
        "include/opentelemetry/exporters/otlp/otlp_http_metric_exporter_runtime_options.h",
        "include/opentelemetry/exporters/otlp/protobuf_include_prefix.h",
        "include/opentelemetry/exporters/otlp/protobuf_include_suffix.h",
    ],
    strip_include_prefix = "include",
    tags = [
        "otlp",
        "otlp_http_metric",
    ],
    deps = [
        ":otlp_http_client",
        ":otlp_recordable",
        "//sdk/src/metrics",
        "@com_github_opentelemetry_proto//:metrics_service_proto_cc",
    ],
)

cc_library(
    name = "otlp_file_metric_exporter",
    srcs = [
        "src/otlp_file_metric_exporter.cc",
        "src/otlp_file_metric_exporter_factory.cc",
        "src/otlp_file_metric_exporter_options.cc",
    ],
    hdrs = [
        "include/opentelemetry/exporters/otlp/otlp_environment.h",
        "include/opentelemetry/exporters/otlp/otlp_file_metric_exporter.h",
        "include/opentelemetry/exporters/otlp/otlp_file_metric_exporter_factory.h",
        "include/opentelemetry/exporters/otlp/otlp_file_metric_exporter_options.h",
        "include/opentelemetry/exporters/otlp/otlp_file_metric_exporter_runtime_options.h",
        "include/opentelemetry/exporters/otlp/protobuf_include_prefix.h",
        "include/opentelemetry/exporters/otlp/protobuf_include_suffix.h",
    ],
    strip_include_prefix = "include",
    tags = [
        "otlp",
        "otlp_file_metric",
    ],
    deps = [
        ":otlp_file_client",
        ":otlp_recordable",
        "//sdk/src/metrics",
        "@com_github_opentelemetry_proto//:metrics_service_proto_cc",
    ],
)

cc_library(
    name = "otlp_http_log_record_exporter",
    srcs = [
        "src/otlp_http_log_record_exporter.cc",
        "src/otlp_http_log_record_exporter_factory.cc",
        "src/otlp_http_log_record_exporter_options.cc",
    ],
    hdrs = [
        "include/opentelemetry/exporters/otlp/otlp_environment.h",
        "include/opentelemetry/exporters/otlp/otlp_http_log_record_exporter.h",
        "include/opentelemetry/exporters/otlp/otlp_http_log_record_exporter_factory.h",
        "include/opentelemetry/exporters/otlp/otlp_http_log_record_exporter_options.h",
        "include/opentelemetry/exporters/otlp/otlp_http_log_record_exporter_runtime_options.h",
        "include/opentelemetry/exporters/otlp/protobuf_include_prefix.h",
        "include/opentelemetry/exporters/otlp/protobuf_include_suffix.h",
    ],
    strip_include_prefix = "include",
    tags = [
        "otlp",
        "otlp_http_log",
    ],
    deps = [
        ":otlp_http_client",
        ":otlp_recordable",
        "//sdk/src/logs",
        "@com_github_opentelemetry_proto//:logs_service_proto_cc",
    ],
)

cc_library(
    name = "otlp_file_log_record_exporter",
    srcs = [
        "src/otlp_file_log_record_exporter.cc",
        "src/otlp_file_log_record_exporter_factory.cc",
        "src/otlp_file_log_record_exporter_options.cc",
    ],
    hdrs = [
        "include/opentelemetry/exporters/otlp/otlp_environment.h",
        "include/opentelemetry/exporters/otlp/otlp_file_log_record_exporter.h",
        "include/opentelemetry/exporters/otlp/otlp_file_log_record_exporter_factory.h",
        "include/opentelemetry/exporters/otlp/otlp_file_log_record_exporter_options.h",
        "include/opentelemetry/exporters/otlp/otlp_file_log_record_exporter_runtime_options.h",
        "include/opentelemetry/exporters/otlp/protobuf_include_prefix.h",
        "include/opentelemetry/exporters/otlp/protobuf_include_suffix.h",
    ],
    strip_include_prefix = "include",
    tags = [
        "otlp",
        "otlp_file_log",
    ],
    deps = [
        ":otlp_file_client",
        ":otlp_recordable",
        "//sdk/src/logs",
        "@com_github_opentelemetry_proto//:logs_service_proto_cc",
    ],
)

cc_library(
    name = "otlp_grpc_log_record_exporter",
    srcs = [
        "src/otlp_grpc_log_record_exporter.cc",
        "src/otlp_grpc_log_record_exporter_factory.cc",
        "src/otlp_grpc_log_record_exporter_options.cc",
    ],
    hdrs = [
        "include/opentelemetry/exporters/otlp/otlp_environment.h",
        "include/opentelemetry/exporters/otlp/otlp_grpc_client_options.h",
        "include/opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter.h",
        "include/opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_factory.h",
        "include/opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_options.h",
        "include/opentelemetry/exporters/otlp/protobuf_include_prefix.h",
        "include/opentelemetry/exporters/otlp/protobuf_include_suffix.h",
    ],
    strip_include_prefix = "include",
    tags = [
        "otlp",
        "otlp_grpc_log",
    ],
    deps = [
        ":otlp_recordable",
        ":otlp_grpc_client",
        "//ext:headers",
        "//sdk/src/logs",
        # For gRPC
        "@com_github_opentelemetry_proto//:logs_service_grpc_cc",
    ],
)

cc_test(
    name = "otlp_recordable_test",
    srcs = ["test/otlp_recordable_test.cc"],
    tags = [
        "otlp",
        "test",
    ],
    deps = [
        ":otlp_recordable",
        "//sdk/src/metrics",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "otlp_log_recordable_test",
    srcs = [
        "test/otlp_log_recordable_test.cc",
        "test/otlp_metrics_serialization_test.cc",
    ],
    tags = [
        "otlp",
        "test",
    ],
    deps = [
        ":otlp_recordable",
        "//sdk/src/metrics",
        "@com_github_opentelemetry_proto//:logs_service_proto_cc",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "otlp_file_client_test",
    srcs = ["test/otlp_file_client_test.cc"],
    tags = [
        "otlp",
        "otlp_file",
        "test",
    ],
    deps = [
        ":otlp_file_client",
        ":otlp_file_exporter",
        ":otlp_recordable",
        "//api",
        "//sdk/src/metrics",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "otlp_grpc_exporter_test",
    srcs = ["test/otlp_grpc_exporter_test.cc"],
    tags = [
        "otlp",
        "otlp_grpc",
        "test",
    ],
    deps = [
        ":otlp_grpc_exporter",
        "//api",
        "//sdk/src/metrics",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "otlp_grpc_target_test",
    srcs = ["test/otlp_grpc_target_test.cc"],
    tags = [
        "otlp",
        "otlp_grpc",
        "test",
    ],
    deps = [
        ":otlp_grpc_client",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "otlp_grpc_exporter_factory_test",
    srcs = ["test/otlp_grpc_exporter_factory_test.cc"],
    tags = [
        "otlp",
        "otlp_grpc",
        "test",
    ],
    deps = [
        ":otlp_grpc_exporter",
        "//api",
        "//sdk/src/metrics",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "otlp_http_exporter_test",
    srcs = ["test/otlp_http_exporter_test.cc"],
    tags = [
        "otlp",
        "otlp_http",
        "test",
    ],
    deps = [
        ":otlp_http_exporter",
        "//api",
        "//sdk/src/metrics",
        "//test_common/src/http/client/nosend:http_client_nosend",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "otlp_http_exporter_factory_test",
    srcs = ["test/otlp_http_exporter_factory_test.cc"],
    tags = [
        "otlp",
        "otlp_http",
        "test",
    ],
    deps = [
        ":otlp_http_exporter",
        "//api",
        "//sdk/src/metrics",
        "//test_common/src/http/client/nosend:http_client_nosend",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "otlp_file_exporter_test",
    srcs = ["test/otlp_file_exporter_test.cc"],
    tags = [
        "otlp",
        "otlp_file",
        "test",
    ],
    deps = [
        ":otlp_file_exporter",
        "//api",
        "//sdk/src/metrics",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "otlp_file_exporter_factory_test",
    srcs = ["test/otlp_file_exporter_factory_test.cc"],
    tags = [
        "otlp",
        "otlp_file",
        "test",
    ],
    deps = [
        ":otlp_file_exporter",
        "//api",
        "//sdk/src/metrics",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "otlp_http_log_record_exporter_test",
    srcs = ["test/otlp_http_log_record_exporter_test.cc"],
    tags = [
        "otlp",
        "otlp_http_log",
        "test",
    ],
    deps = [
        ":otlp_http_log_record_exporter",
        "//api",
        "//sdk/src/metrics",
        "//test_common/src/http/client/nosend:http_client_nosend",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "otlp_http_log_record_exporter_factory_test",
    srcs = ["test/otlp_http_log_record_exporter_factory_test.cc"],
    tags = [
        "otlp",
        "otlp_http_log",
        "test",
    ],
    deps = [
        ":otlp_http_log_record_exporter",
        "//api",
        "//sdk/src/metrics",
        "//test_common/src/http/client/nosend:http_client_nosend",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "otlp_file_log_record_exporter_test",
    srcs = ["test/otlp_file_log_record_exporter_test.cc"],
    tags = [
        "otlp",
        "otlp_file_log",
        "test",
    ],
    deps = [
        ":otlp_file_log_record_exporter",
        "//api",
        "//sdk/src/metrics",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "otlp_file_log_record_exporter_factory_test",
    srcs = ["test/otlp_file_log_record_exporter_factory_test.cc"],
    tags = [
        "otlp",
        "otlp_file_log",
        "test",
    ],
    deps = [
        ":otlp_file_log_record_exporter",
        "//api",
        "//sdk/src/metrics",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "otlp_grpc_log_record_exporter_test",
    srcs = ["test/otlp_grpc_log_record_exporter_test.cc"],
    tags = [
        "otlp",
        "otlp_grpc_log",
        "test",
    ],
    deps = [
        ":otlp_grpc_exporter",
        ":otlp_grpc_log_record_exporter",
        "//api",
        "//sdk/src/logs",
        "//sdk/src/metrics",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "otlp_grpc_log_record_exporter_factory_test",
    srcs = ["test/otlp_grpc_log_record_exporter_factory_test.cc"],
    tags = [
        "otlp",
        "otlp_grpc_log",
        "test",
    ],
    deps = [
        ":otlp_grpc_log_record_exporter",
        "//api",
        "//sdk/src/logs",
        "//sdk/src/metrics",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "otlp_grpc_metric_exporter_test",
    srcs = ["test/otlp_grpc_metric_exporter_test.cc"],
    tags = [
        "otlp",
        "otlp_grpc_metric",
        "test",
    ],
    deps = [
        ":otlp_grpc_metric_exporter",
        "//api",
        "//sdk/src/metrics",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "otlp_grpc_metric_exporter_factory_test",
    srcs = ["test/otlp_grpc_metric_exporter_factory_test.cc"],
    tags = [
        "otlp",
        "otlp_grpc_metric",
        "test",
    ],
    deps = [
        ":otlp_grpc_metric_exporter",
        "//api",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "otlp_http_metric_exporter_test",
    srcs = ["test/otlp_http_metric_exporter_test.cc"],
    tags = [
        "otlp",
        "otlp_http_metric",
        "test",
    ],
    deps = [
        ":otlp_http_metric_exporter",
        "//api",
        "//test_common/src/http/client/nosend:http_client_nosend",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "otlp_http_metric_exporter_factory_test",
    srcs = ["test/otlp_http_metric_exporter_factory_test.cc"],
    tags = [
        "otlp",
        "otlp_http_metric",
        "test",
    ],
    deps = [
        ":otlp_http_metric_exporter",
        "//api",
        "//test_common/src/http/client/nosend:http_client_nosend",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "otlp_file_metric_exporter_test",
    srcs = ["test/otlp_file_metric_exporter_test.cc"],
    tags = [
        "otlp",
        "otlp_file_metric",
        "test",
    ],
    deps = [
        ":otlp_file_metric_exporter",
        "//api",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "otlp_file_metric_exporter_factory_test",
    srcs = ["test/otlp_file_metric_exporter_factory_test.cc"],
    tags = [
        "otlp",
        "otlp_file_metric",
        "test",
    ],
    deps = [
        ":otlp_file_metric_exporter",
        "//api",
        "@com_google_googletest//:gtest_main",
    ],
)

otel_cc_benchmark(
    name = "otlp_grpc_exporter_benchmark",
    srcs = ["test/otlp_grpc_exporter_benchmark.cc"],
    tags = [
        "benchmark",
        "otlp",
        "otlp_grpc",
        "test",
    ],
    deps = [
        ":otlp_grpc_exporter",
        "//examples/common/foo_library:common_foo_library",
        "//sdk/src/metrics",
    ],
)
