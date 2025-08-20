# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

add_library(opentelemetry_exporter_elasticsearch_logs
            src/es_log_record_exporter.cc src/es_log_recordable.cc)

set_target_properties(opentelemetry_exporter_elasticsearch_logs
                      PROPERTIES EXPORT_NAME elasticsearch_log_record_exporter)
set_target_version(opentelemetry_exporter_elasticsearch_logs)

target_include_directories(
  opentelemetry_exporter_elasticsearch_logs
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
         "$<INSTALL_INTERFACE:include>")

target_link_libraries(
  opentelemetry_exporter_elasticsearch_logs
  PUBLIC opentelemetry_trace opentelemetry_logs opentelemetry_http_client_curl
         nlohmann_json::nlohmann_json)

otel_add_component(
  COMPONENT
  exporters_elasticsearch
  TARGETS
  opentelemetry_exporter_elasticsearch_logs
  FILES_DIRECTORY
  "include/opentelemetry/exporters/elasticsearch"
  FILES_DESTINATION
  "include/opentelemetry/exporters"
  FILES_MATCHING
  PATTERN
  "*.h"
  PATTERN
  "es_log_recordable.h"
  EXCLUDE)

if(BUILD_TESTING)
  add_executable(es_log_record_exporter_test
                 test/es_log_record_exporter_test.cc)

  target_link_libraries(
    es_log_record_exporter_test ${GTEST_BOTH_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT} opentelemetry_exporter_elasticsearch_logs)

  gtest_add_tests(
    TARGET es_log_record_exporter_test
    TEST_PREFIX exporter.
    TEST_LIST es_log_record_exporter_test)
endif() # BUILD_TESTING
