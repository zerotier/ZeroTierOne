# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

add_library(opentelemetry_exporter_in_memory
            src/in_memory_span_exporter_factory.cc)

target_include_directories(
  opentelemetry_exporter_in_memory
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
         "$<INSTALL_INTERFACE:include>")

set_target_properties(opentelemetry_exporter_in_memory
                      PROPERTIES EXPORT_NAME in_memory_span_exporter)
set_target_version(opentelemetry_exporter_in_memory)

target_link_libraries(opentelemetry_exporter_in_memory
                      PUBLIC opentelemetry_trace)

add_library(
  opentelemetry_exporter_in_memory_metric
  src/in_memory_metric_exporter_factory.cc src/in_memory_metric_data.cc)

target_include_directories(
  opentelemetry_exporter_in_memory_metric
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
         "$<INSTALL_INTERFACE:include>")

set_target_properties(opentelemetry_exporter_in_memory_metric
                      PROPERTIES EXPORT_NAME in_memory_metric_exporter)
set_target_version(opentelemetry_exporter_in_memory_metric)

target_link_libraries(opentelemetry_exporter_in_memory_metric
                      PUBLIC opentelemetry_metrics)

otel_add_component(
  COMPONENT
  exporters_in_memory
  TARGETS
  opentelemetry_exporter_in_memory
  opentelemetry_exporter_in_memory_metric
  FILES_DIRECTORY
  "include/opentelemetry/exporters/memory"
  FILES_DESTINATION
  "include/opentelemetry/exporters"
  FILES_MATCHING
  PATTERN
  "*.h")

if(BUILD_TESTING)
  add_executable(in_memory_span_data_test test/in_memory_span_data_test.cc)
  add_executable(in_memory_span_exporter_test
                 test/in_memory_span_exporter_test.cc)
  add_executable(in_memory_metric_data_test test/in_memory_metric_data_test.cc)
  add_executable(in_memory_metric_exporter_test
                 test/in_memory_metric_exporter_test.cc)

  target_link_libraries(
    in_memory_span_data_test ${GTEST_BOTH_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT}
    opentelemetry_exporter_in_memory opentelemetry_resources)

  target_link_libraries(
    in_memory_metric_data_test ${GTEST_BOTH_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT} opentelemetry_exporter_in_memory_metric
    opentelemetry_resources)

  target_link_libraries(
    in_memory_span_exporter_test ${GTEST_BOTH_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT} opentelemetry_exporter_in_memory
    opentelemetry_resources)

  target_link_libraries(
    in_memory_metric_exporter_test ${GTEST_BOTH_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT} opentelemetry_exporter_in_memory_metric
    opentelemetry_resources)

  gtest_add_tests(
    TARGET in_memory_span_data_test
    TEST_PREFIX exporter.
    TEST_LIST in_memory_span_data_test)
  gtest_add_tests(
    TARGET in_memory_span_exporter_test
    TEST_PREFIX exporter.
    TEST_LIST in_memory_span_exporter_test)
  gtest_add_tests(
    TARGET in_memory_metric_data_test
    TEST_PREFIX exporter.
    TEST_LIST in_memory_metric_data_test)
  gtest_add_tests(
    TARGET in_memory_metric_exporter_test
    TEST_PREFIX exporter.
    TEST_LIST in_memory_metric_exporter_test)
endif()
