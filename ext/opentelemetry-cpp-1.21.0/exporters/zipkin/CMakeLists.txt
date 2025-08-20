# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

add_library(
  opentelemetry_exporter_zipkin_trace
  src/zipkin_exporter.cc src/zipkin_exporter_factory.cc src/recordable.cc)

target_include_directories(
  opentelemetry_exporter_zipkin_trace
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
         "$<INSTALL_INTERFACE:include>")

set_target_properties(opentelemetry_exporter_zipkin_trace
                      PROPERTIES EXPORT_NAME zipkin_trace_exporter)

set_target_version(opentelemetry_exporter_zipkin_trace)

target_link_libraries(
  opentelemetry_exporter_zipkin_trace
  PUBLIC opentelemetry_trace opentelemetry_http_client_curl
         nlohmann_json::nlohmann_json)

otel_add_component(
  COMPONENT
  exporters_zipkin
  TARGETS
  opentelemetry_exporter_zipkin_trace
  FILES_DIRECTORY
  "include/opentelemetry/exporters/zipkin"
  FILES_DESTINATION
  "include/opentelemetry/exporters"
  FILES_MATCHING
  PATTERN
  "*.h"
  PATTERN
  "recordable.h"
  EXCLUDE)

if(BUILD_TESTING)
  add_definitions(-DGTEST_LINKED_AS_SHARED_LIBRARY=1)

  add_executable(zipkin_recordable_test test/zipkin_recordable_test.cc)

  target_link_libraries(
    zipkin_recordable_test ${GTEST_BOTH_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT}
    opentelemetry_exporter_zipkin_trace opentelemetry_resources)

  gtest_add_tests(
    TARGET zipkin_recordable_test
    TEST_PREFIX exporter.
    TEST_LIST zipkin_recordable_test)

  add_executable(zipkin_exporter_test test/zipkin_exporter_test.cc)

  target_link_libraries(
    zipkin_exporter_test
    ${GTEST_BOTH_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${GMOCK_LIB}
    opentelemetry_exporter_zipkin_trace
    opentelemetry_resources
    ${CURL_LIBRARIES})

  gtest_add_tests(
    TARGET zipkin_exporter_test
    TEST_PREFIX exporter.
    TEST_LIST zipkin_exporter_test)
endif() # BUILD_TESTING
