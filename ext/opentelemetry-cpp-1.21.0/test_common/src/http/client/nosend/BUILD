# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "http_client_nosend",
    srcs = [
        "http_client_nosend.cc",
        "http_client_test_factory.cc",
    ],
    include_prefix = "src/http/client/nosend",
    tags = [
        "test",
    ],
    deps = [
        "//api",
        "//ext:headers",
        "//sdk:headers",
        "//test_common:headers",
        "@com_google_googletest//:gtest_main",
    ],
)
