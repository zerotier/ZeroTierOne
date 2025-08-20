# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

add_executable(metrics_ostream_example metrics_ostream.cc)

target_link_libraries(metrics_ostream_example
                      PRIVATE common_metrics_foo_library)

if(DEFINED OPENTELEMETRY_BUILD_DLL)
  target_compile_definitions(metrics_ostream_example
                             PRIVATE OPENTELEMETRY_BUILD_IMPORT_DLL)
  target_link_libraries(metrics_ostream_example
                        PRIVATE opentelemetry-cpp::opentelemetry_cpp)
else()
  target_link_libraries(
    metrics_ostream_example PRIVATE opentelemetry-cpp::metrics
                                    opentelemetry-cpp::ostream_metrics_exporter)
endif()
