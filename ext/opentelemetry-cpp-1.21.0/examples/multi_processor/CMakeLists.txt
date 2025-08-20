# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

add_executable(example_multi_processor main.cc)
target_link_libraries(
  example_multi_processor
  PRIVATE common_foo_library opentelemetry-cpp::trace
          opentelemetry-cpp::ostream_span_exporter
          opentelemetry-cpp::in_memory_span_exporter)
