# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

cc_binary(
    name = "example_plugin.so",
    srcs = [
        "factory_impl.cc",
        "tracer.cc",
        "tracer.h",
    ],
    linkshared = 1,
    deps = [
        "//api",
    ],
)
