# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

add_executable(example_simple main.cc)
target_link_libraries(example_simple PRIVATE common_foo_library)

if(DEFINED OPENTELEMETRY_BUILD_DLL)
  target_compile_definitions(example_simple
                             PRIVATE OPENTELEMETRY_BUILD_IMPORT_DLL)
  target_link_libraries(example_simple
                        PRIVATE opentelemetry-cpp::opentelemetry_cpp)
else()
  target_link_libraries(
    example_simple PRIVATE opentelemetry-cpp::trace
                           opentelemetry-cpp::ostream_span_exporter)
endif()
