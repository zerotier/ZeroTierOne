# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

foreach(testname exporter_test collector_test exporter_utils_test)
  add_executable(${testname} "${testname}.cc")
  target_link_libraries(
    ${testname} ${GTEST_BOTH_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT}
    opentelemetry_exporter_prometheus prometheus-cpp::pull
    opentelemetry_resources)
  gtest_add_tests(
    TARGET ${testname}
    TEST_PREFIX exporter.
    TEST_LIST ${testname})
endforeach()
