# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

add_library(metrics_common_test_utils common.cc)
target_link_libraries(metrics_common_test_utils opentelemetry_metrics)

foreach(
  testname
  meter_config_test
  meter_provider_set_test
  meter_provider_sdk_test
  meter_test
  view_registry_test
  aggregation_test
  sum_aggregation_test
  histogram_aggregation_test
  attributes_processor_test
  attributes_hashmap_test
  base2_exponential_histogram_indexer_test
  circular_buffer_counter_test
  cardinality_limit_test
  histogram_test
  sync_metric_storage_counter_test
  sync_metric_storage_gauge_test
  sync_metric_storage_histogram_test
  sync_metric_storage_up_down_counter_test
  async_metric_storage_test
  multi_metric_storage_test
  observer_result_test
  sync_instruments_test
  async_instruments_test
  metric_collector_test
  metric_reader_test
  observable_registry_test
  periodic_exporting_metric_reader_test
  instrument_metadata_validator_test
  metric_test_stress
  instrument_descriptor_test)
  add_executable(${testname} "${testname}.cc")
  target_link_libraries(
    ${testname} ${GTEST_BOTH_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT}
    metrics_common_test_utils opentelemetry_resources)
  target_compile_definitions(${testname} PRIVATE UNIT_TESTING)
  gtest_add_tests(
    TARGET ${testname}
    TEST_PREFIX metrics.
    TEST_LIST ${testname})
endforeach()

if(WITH_BENCHMARK)
  add_executable(attributes_processor_benchmark
                 attributes_processor_benchmark.cc)
  target_link_libraries(attributes_processor_benchmark benchmark::benchmark
                        ${CMAKE_THREAD_LIBS_INIT} opentelemetry_common)

  add_executable(attributes_hashmap_benchmark attributes_hashmap_benchmark.cc)
  target_link_libraries(attributes_hashmap_benchmark benchmark::benchmark
                        ${CMAKE_THREAD_LIBS_INIT} opentelemetry_common)

  add_executable(base2_exponential_histogram_indexer_benchmark
                 base2_exponential_histogram_indexer_benchmark.cc)
  target_link_libraries(
    base2_exponential_histogram_indexer_benchmark benchmark::benchmark
    opentelemetry_metrics ${CMAKE_THREAD_LIBS_INIT} opentelemetry_common)

  add_executable(histogram_aggregation_benchmark
                 histogram_aggregation_benchmark.cc)
  target_link_libraries(
    histogram_aggregation_benchmark benchmark::benchmark
    ${CMAKE_THREAD_LIBS_INIT} metrics_common_test_utils opentelemetry_common
    opentelemetry_resources)

  add_executable(measurements_benchmark measurements_benchmark.cc)
  target_link_libraries(
    measurements_benchmark benchmark::benchmark opentelemetry_metrics
    opentelemetry_resources ${CMAKE_THREAD_LIBS_INIT} opentelemetry_common)
  add_executable(sum_aggregation_benchmark sum_aggregation_benchmark.cc)
  target_link_libraries(
    sum_aggregation_benchmark benchmark::benchmark ${CMAKE_THREAD_LIBS_INIT}
    metrics_common_test_utils opentelemetry_common opentelemetry_resources)
endif()

add_subdirectory(exemplar)
