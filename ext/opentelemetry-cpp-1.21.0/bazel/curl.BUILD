# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

# Builds CURL from a distribution.

load("@io_opentelemetry_cpp//bazel:curl.bzl", "CURL_COPTS")

package(features = ["no_copts_tokenization"])

config_setting(
    name = "windows",
    constraint_values = [
        "@platforms//os:windows",
    ],
    visibility = ["//visibility:private"],
)

config_setting(
    name = "osx",
    constraint_values = [
        "@platforms//os:osx",
    ],
    visibility = ["//visibility:private"],
)

cc_library(
    name = "curl",
    srcs = glob([
        "lib/**/*.c",
    ]),
    hdrs = glob([
        "include/curl/*.h",
        "lib/**/*.h",
    ]),
    copts = CURL_COPTS + [
        "-DOS=\"os\"",
    ],
    defines = ["CURL_STATICLIB"],
    includes = [
        "include/",
        "lib/",
    ],
    linkopts = select({
        "//:windows": [
            "-DEFAULTLIB:ws2_32.lib",
            "-DEFAULTLIB:advapi32.lib",
            "-DEFAULTLIB:crypt32.lib",
            "-DEFAULTLIB:Normaliz.lib",
        ],
        "//:osx": [
            "-framework SystemConfiguration",
            "-lpthread",
        ],
        "//conditions:default": [
            "-lpthread",
        ],
    }),
    visibility = ["//visibility:public"],
)
