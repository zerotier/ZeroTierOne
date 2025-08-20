# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "random",
    srcs = [
        "random.cc",
    ],
    hdrs = [
        "fast_random_number_generator.h",
        "random.h",
    ],
    include_prefix = "src/common",
    deps = [
        "//api",
        "//sdk:headers",
        "//sdk/src/common/platform:fork",
    ],
)

cc_library(
    name = "base64",
    srcs = [
        "base64.cc",
    ],
    include_prefix = "src/common",
    deps = [
        "//api",
        "//sdk:headers",
        "//sdk/src/common/platform:fork",
    ],
)

cc_library(
    name = "disabled",
    srcs = [
        "disabled.cc",
    ],
    include_prefix = "src/common",
    deps = [
        "//api",
        "//sdk:headers",
    ],
)

cc_library(
    name = "env_variables",
    srcs = [
        "env_variables.cc",
    ],
    deps = [
        "//api",
        "//sdk:headers",
        "//sdk/src/common:global_log_handler",
    ],
)

cc_library(
    name = "global_log_handler",
    srcs = [
        "global_log_handler.cc",
    ],
    deps = [
        "//api",
        "//sdk:headers",
    ],
)
