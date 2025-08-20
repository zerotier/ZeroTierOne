# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

include(GoogleTest)

foreach(testname instrumentationscope_test)
  add_executable(${testname} "${testname}.cc")
  target_link_libraries(${testname} ${GTEST_BOTH_LIBRARIES}
                        ${CMAKE_THREAD_LIBS_INIT} opentelemetry_sdk)
  gtest_add_tests(
    TARGET ${testname}
    TEST_PREFIX instrumentationscope.
    TEST_LIST ${testname})
endforeach()
