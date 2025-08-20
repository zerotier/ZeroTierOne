# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "zipkin_recordable",
    srcs = [
        "src/recordable.cc",
    ],
    hdrs = [
        "include/opentelemetry/exporters/zipkin/recordable.h",
    ],
    strip_include_prefix = "include",
    tags = ["zipkin"],
    deps = [
        "//sdk/src/resource",
        "//sdk/src/trace",
        "@github_nlohmann_json//:json",
    ],
)

cc_library(
    name = "zipkin_exporter",
    srcs = [
        "src/zipkin_exporter.cc",
        "src/zipkin_exporter_factory.cc",
    ],
    hdrs = [
        "include/opentelemetry/exporters/zipkin/zipkin_exporter.h",
        "include/opentelemetry/exporters/zipkin/zipkin_exporter_factory.h",
        "include/opentelemetry/exporters/zipkin/zipkin_exporter_options.h",
    ],
    copts = [
        "-DCURL_STATICLIB",
    ],
    strip_include_prefix = "include",
    tags = ["zipkin"],
    deps = [
        ":zipkin_recordable",
        "//ext/src/http/client/curl:http_client_curl",
    ],
)

cc_test(
    name = "zipkin_recordable_test",
    srcs = ["test/zipkin_recordable_test.cc"],
    tags = [
        "test",
        "zipkin",
    ],
    deps = [
        ":zipkin_recordable",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "zipkin_exporter_test",
    srcs = ["test/zipkin_exporter_test.cc"],
    tags = [
        "test",
        "zipkin",
    ],
    deps = [
        ":zipkin_exporter",
        ":zipkin_recordable",
        "@com_google_googletest//:gtest_main",
    ],
)
