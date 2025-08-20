# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

add_executable(example_zipkin main.cc)
target_link_libraries(
  example_zipkin PRIVATE common_foo_library
                         opentelemetry-cpp::zipkin_trace_exporter)
