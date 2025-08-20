# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0
add_executable(batch_span_processor_example main.cc)

target_link_libraries(
  batch_span_processor_example PRIVATE opentelemetry-cpp::ostream_span_exporter
                                       opentelemetry-cpp::trace)
