# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "metrics",
    srcs = glob(["**/*.cc"]),
    include_prefix = "src/metrics",
    deps = [
        "//api",
        "//sdk:headers",
        "//sdk/src/common:disabled",
        "//sdk/src/common:global_log_handler",
        "//sdk/src/common:random",
        "//sdk/src/resource",
    ],
)
