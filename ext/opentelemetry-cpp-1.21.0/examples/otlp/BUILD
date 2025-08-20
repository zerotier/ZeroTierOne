# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

cc_binary(
    name = "example_otlp_grpc",
    srcs = [
        "grpc_main.cc",
    ],
    tags = [
        "examples",
        "otlp",
        "otlp_grpc",
    ],
    deps = [
        "//api",
        "//examples/common/foo_library:common_foo_library",
        "//exporters/otlp:otlp_grpc_exporter",
        "//sdk/src/trace",
    ],
)

cc_binary(
    name = "example_otlp_http",
    srcs = [
        "http_main.cc",
    ],
    tags = [
        "examples",
        "otlp",
        "otlp_http",
    ],
    deps = [
        "//api",
        "//examples/common/foo_library:common_foo_library",
        "//exporters/otlp:otlp_http_exporter",
        "//sdk/src/trace",
    ],
)

cc_binary(
    name = "example_otlp_file",
    srcs = [
        "file_main.cc",
    ],
    tags = [
        "examples",
        "otlp",
        "otlp_file",
    ],
    deps = [
        "//api",
        "//examples/common/foo_library:common_foo_library",
        "//exporters/otlp:otlp_file_exporter",
        "//sdk/src/trace",
    ],
)

cc_binary(
    name = "example_otlp_http_log",
    srcs = [
        "http_log_main.cc",
    ],
    tags = [
        "examples",
        "otlp",
        "otlp_http_log",
    ],
    deps = [
        "//api",
        "//examples/common/logs_foo_library:common_logs_foo_library",
        "//exporters/otlp:otlp_http_exporter",
        "//exporters/otlp:otlp_http_log_record_exporter",
        "//sdk/src/trace",
    ],
)

cc_binary(
    name = "example_otlp_grpc_log",
    srcs = [
        "grpc_log_main.cc",
    ],
    tags = [
        "examples",
        "otlp",
        "otlp_grpc_log",
    ],
    deps = [
        "//api",
        "//examples/common/logs_foo_library:common_logs_foo_library",
        "//exporters/otlp:otlp_grpc_exporter",
        "//exporters/otlp:otlp_grpc_log_record_exporter",
        "//sdk/src/trace",
    ],
)

cc_binary(
    name = "example_otlp_file_log",
    srcs = [
        "file_log_main.cc",
    ],
    tags = [
        "examples",
        "otlp",
        "otlp_file_log",
    ],
    deps = [
        "//api",
        "//examples/common/logs_foo_library:common_logs_foo_library",
        "//exporters/otlp:otlp_file_exporter",
        "//exporters/otlp:otlp_file_log_record_exporter",
        "//sdk/src/trace",
    ],
)

cc_binary(
    name = "example_otlp_grpc_metric",
    srcs = [
        "grpc_metric_main.cc",
    ],
    tags = [
        "examples",
        "metrics",
        "otlp",
    ],
    deps = [
        "//api",
        "//examples/common/metrics_foo_library:common_metrics_foo_library",
        "//exporters/otlp:otlp_grpc_exporter",
        "//exporters/otlp:otlp_grpc_metric_exporter",
        "//sdk/src/metrics",
    ],
)

cc_binary(
    name = "example_otlp_http_metric",
    srcs = [
        "http_metric_main.cc",
    ],
    tags = [
        "examples",
        "metrics",
        "otlp",
    ],
    deps = [
        "//api",
        "//examples/common/metrics_foo_library:common_metrics_foo_library",
        "//exporters/otlp:otlp_http_exporter",
        "//exporters/otlp:otlp_http_metric_exporter",
        "//sdk/src/metrics",
    ],
)

cc_binary(
    name = "example_otlp_file_metric",
    srcs = [
        "file_metric_main.cc",
    ],
    tags = [
        "examples",
        "metrics",
        "otlp",
    ],
    deps = [
        "//api",
        "//examples/common/metrics_foo_library:common_metrics_foo_library",
        "//exporters/otlp:otlp_file_exporter",
        "//exporters/otlp:otlp_file_metric_exporter",
        "//sdk/src/metrics",
    ],
)

cc_binary(
    name = "example_otlp_instrumented_http",
    srcs = [
        "http_instrumented_main.cc",
    ],
    tags = [
        "examples",
        "otlp",
        "otlp_http",
    ],
    deps = [
        "//api",
        "//examples/common/logs_foo_library:common_logs_foo_library",
        "//examples/common/metrics_foo_library:common_metrics_foo_library",
        "//exporters/otlp:otlp_http_exporter",
        "//exporters/otlp:otlp_http_log_record_exporter",
        "//exporters/otlp:otlp_http_metric_exporter",
        "//sdk/src/metrics",
        "//sdk/src/trace",
    ],
)
