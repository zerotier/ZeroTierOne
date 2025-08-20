# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

add_library(example_plugin SHARED tracer.cc factory_impl.cc)
target_link_libraries(example_plugin PUBLIC opentelemetry-cpp::api)
