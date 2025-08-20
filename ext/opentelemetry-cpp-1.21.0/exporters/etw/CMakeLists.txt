# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

add_library(opentelemetry_exporter_etw INTERFACE)

target_include_directories(
  opentelemetry_exporter_etw
  INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
            "$<INSTALL_INTERFACE:include>")

set_target_properties(opentelemetry_exporter_etw PROPERTIES EXPORT_NAME
                                                            etw_exporter)

target_link_libraries(
  opentelemetry_exporter_etw INTERFACE opentelemetry_api opentelemetry_trace
                                       nlohmann_json::nlohmann_json)
target_link_libraries(opentelemetry_exporter_etw INTERFACE opentelemetry_logs)
if(nlohmann_json_clone)
  add_dependencies(opentelemetry_exporter_etw nlohmann_json::nlohmann_json)
endif()

otel_add_component(
  COMPONENT
  exporters_etw
  TARGETS
  opentelemetry_exporter_etw
  FILES_DIRECTORY
  "include/opentelemetry/exporters/etw"
  FILES_DESTINATION
  "include/opentelemetry/exporters"
  FILES_MATCHING
  PATTERN
  "*.h")

if(BUILD_TESTING)
  add_executable(etw_provider_test test/etw_provider_test.cc)
  add_executable(etw_tracer_test test/etw_tracer_test.cc)
  add_executable(etw_logger_test test/etw_logger_test.cc)
  add_executable(etw_tracer_test_enable_env_properties test/etw_tracer_test.cc)
  add_executable(etw_logger_test_enable_env_properties test/etw_logger_test.cc)

  target_link_libraries(etw_provider_test ${GTEST_BOTH_LIBRARIES}
                        opentelemetry_exporter_etw ${CMAKE_THREAD_LIBS_INIT})

  target_link_libraries(etw_tracer_test ${GTEST_BOTH_LIBRARIES}
                        opentelemetry_exporter_etw ${CMAKE_THREAD_LIBS_INIT})

  target_link_libraries(etw_logger_test ${GTEST_BOTH_LIBRARIES}
                        opentelemetry_exporter_etw ${CMAKE_THREAD_LIBS_INIT})

  target_link_libraries(
    etw_tracer_test_enable_env_properties ${GTEST_BOTH_LIBRARIES}
    opentelemetry_exporter_etw ${CMAKE_THREAD_LIBS_INIT})
  target_compile_definitions(etw_tracer_test_enable_env_properties
                             PRIVATE ENABLE_ENV_PROPERTIES)

  target_link_libraries(
    etw_logger_test_enable_env_properties ${GTEST_BOTH_LIBRARIES}
    opentelemetry_exporter_etw ${CMAKE_THREAD_LIBS_INIT})
  target_compile_definitions(etw_logger_test_enable_env_properties
                             PRIVATE ENABLE_ENV_PROPERTIES)

  if(WITH_BENCHMARK)
    add_executable(etw_perf_test test/etw_perf_test.cc)
    target_link_libraries(
      etw_perf_test benchmark::benchmark ${GTEST_BOTH_LIBRARIES}
      opentelemetry_exporter_etw ${CMAKE_THREAD_LIBS_INIT})
  endif()

  gtest_add_tests(
    TARGET etw_provider_test
    TEST_PREFIX exporter.
    TEST_LIST etw_provider_test)
  gtest_add_tests(
    TARGET etw_tracer_test
    TEST_PREFIX exporter.
    TEST_LIST etw_tracer_test)
  gtest_add_tests(
    TARGET etw_logger_test
    TEST_PREFIX exporter.
    TEST_LIST etw_logger_test)
  gtest_add_tests(
    TARGET etw_tracer_test_enable_env_properties
    TEST_PREFIX exporter.with_env_properties.
    TEST_LIST etw_tracer_test_enable_env_properties)
  gtest_add_tests(
    TARGET etw_logger_test_enable_env_properties
    TEST_PREFIX exporter.with_env_properties.
    TEST_LIST etw_logger_test_enable_env_properties)

endif() # BUILD_TESTING
