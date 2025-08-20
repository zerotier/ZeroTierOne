# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

foreach(
  testname
  random_test
  fast_random_number_generator_test
  atomic_unique_ptr_test
  circular_buffer_range_test
  circular_buffer_test
  attribute_utils_test
  attributemap_hash_test
  global_log_handle_test
  global_log_handle_singleton_lifetime_test
  env_var_test)

  add_executable(${testname} "${testname}.cc")
  target_link_libraries(
    ${testname} ${GTEST_BOTH_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT}
    opentelemetry_common opentelemetry_trace)

  gtest_add_tests(
    TARGET ${testname}
    TEST_PREFIX trace.
    TEST_LIST ${testname})
endforeach()

add_executable(base64_test base64_test.cc)
target_link_libraries(base64_test ${GTEST_BOTH_LIBRARIES} opentelemetry_common)
gtest_add_tests(
  TARGET base64_test
  TEST_PREFIX common.
  TEST_LIST base64_test)

add_executable(random_fork_test random_fork_test.cc)
target_link_libraries(random_fork_test opentelemetry_common)
add_test(random_fork_test random_fork_test)

if(WITH_BENCHMARK)
  add_executable(base64_benchmark base64_benchmark.cc)
  target_link_libraries(base64_benchmark benchmark::benchmark
                        ${CMAKE_THREAD_LIBS_INIT} opentelemetry_common)

  add_executable(random_benchmark random_benchmark.cc)
  target_link_libraries(random_benchmark benchmark::benchmark
                        ${CMAKE_THREAD_LIBS_INIT} opentelemetry_common)

  add_executable(circular_buffer_benchmark circular_buffer_benchmark.cc)
  target_link_libraries(circular_buffer_benchmark benchmark::benchmark
                        ${CMAKE_THREAD_LIBS_INIT} opentelemetry_common)

  add_executable(attributemap_hash_benchmark attributemap_hash_benchmark.cc)
  target_link_libraries(attributemap_hash_benchmark benchmark::benchmark
                        ${CMAKE_THREAD_LIBS_INIT} opentelemetry_common)
endif()
