# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

add_executable(example_logs_simple main.cc)
target_link_libraries(example_logs_simple PRIVATE common_logs_foo_library)

if(DEFINED OPENTELEMETRY_BUILD_DLL)
  target_compile_definitions(example_logs_simple
                             PRIVATE OPENTELEMETRY_BUILD_IMPORT_DLL)
  target_link_libraries(example_logs_simple
                        PRIVATE opentelemetry-cpp::opentelemetry_cpp)
else()
  target_link_libraries(
    example_logs_simple
    PRIVATE opentelemetry-cpp::trace opentelemetry-cpp::logs
            opentelemetry-cpp::ostream_span_exporter
            opentelemetry-cpp::ostream_log_record_exporter)
endif()
