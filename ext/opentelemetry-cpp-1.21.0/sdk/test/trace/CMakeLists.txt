# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

foreach(
  testname
  tracer_provider_set_test
  tracer_provider_test
  span_data_test
  simple_processor_test
  tracer_test
  always_off_sampler_test
  always_on_sampler_test
  parent_sampler_test
  trace_id_ratio_sampler_test
  batch_span_processor_test
  tracer_config_test)
  add_executable(${testname} "${testname}.cc")
  target_link_libraries(
    ${testname}
    ${GTEST_BOTH_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    opentelemetry_common
    opentelemetry_trace
    opentelemetry_resources
    opentelemetry_exporter_in_memory)
  gtest_add_tests(
    TARGET ${testname}
    TEST_PREFIX trace.
    TEST_LIST ${testname})
endforeach()

if(WITH_BENCHMARK)
  add_executable(sampler_benchmark sampler_benchmark.cc)
  target_link_libraries(
    sampler_benchmark benchmark::benchmark ${CMAKE_THREAD_LIBS_INIT}
    opentelemetry_trace opentelemetry_resources
    opentelemetry_exporter_in_memory)
endif()
