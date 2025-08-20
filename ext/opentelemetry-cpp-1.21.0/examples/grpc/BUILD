# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

package(default_visibility = ["//visibility:public"])

proto_library(
    name = "messages_proto",
    srcs = ["protos/messages.proto"],
)

cc_proto_library(
    name = "messages_cc_proto",
    deps = [":messages_proto"],
)

cc_grpc_library(
    name = "messages_cc_grpc",
    srcs = [":messages_proto"],
    grpc_only = True,
    deps = [":messages_cc_proto"],
)

cc_library(
    name = "tracer_common",
    srcs = ["tracer_common.h"],
    defines = ["BAZEL_BUILD"],
    tags = ["ostream"],
    deps = [
        "//exporters/ostream:ostream_span_exporter",
    ],
)

cc_binary(
    name = "client_grpc",
    srcs = [
        "client.cc",
    ],
    defines = ["BAZEL_BUILD"],
    tags = ["ostream"],
    deps = [
        "messages_cc_grpc",
        ":tracer_common",
        "//api",
        "//sdk/src/trace",
        "@com_github_grpc_grpc//:grpc++",
    ],
)

cc_binary(
    name = "server_grpc",
    srcs = [
        "server.cc",
    ],
    defines = ["BAZEL_BUILD"],
    tags = ["ostream"],
    deps = [
        "messages_cc_grpc",
        ":tracer_common",
        "//api",
        "//sdk/src/trace",
        "@com_github_grpc_grpc//:grpc++",
    ],
)
