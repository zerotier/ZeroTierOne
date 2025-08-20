# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

foreach(
  testname
  logger_provider_set_test
  logger_provider_sdk_test
  logger_sdk_test
  log_record_test
  simple_log_record_processor_test
  batch_log_record_processor_test
  logger_config_test)
  add_executable(${testname} "${testname}.cc")
  target_link_libraries(
    ${testname} ${GTEST_BOTH_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT}
    opentelemetry_logs opentelemetry_trace)
  gtest_add_tests(
    TARGET ${testname}
    TEST_PREFIX logs.
    TEST_LIST ${testname})
endforeach()
