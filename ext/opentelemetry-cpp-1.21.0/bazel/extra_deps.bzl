# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

# Load prometheus C++ dependencies.

load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")
load("@com_github_jupp0r_prometheus_cpp//bazel:repositories.bzl", "prometheus_cpp_repositories")
load("@rules_foreign_cc//foreign_cc:repositories.bzl", "rules_foreign_cc_dependencies")

def opentelemetry_extra_deps():
    prometheus_cpp_repositories()
    bazel_skylib_workspace()
    rules_foreign_cc_dependencies()
