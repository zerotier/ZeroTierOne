# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

# This test uses CMake's FetchContent module to build opentelemetry-cpp from src
# and make its targets available within an external project.

cmake_minimum_required(VERSION 3.14)

project(opentelemetry-cpp-fetch-content-test LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

set(BUILD_TESTING
    ON
    CACHE BOOL "Build tests" FORCE)

if(NOT DEFINED OPENTELEMETRY_CPP_SRC_DIR)
  message(
    FATAL_ERROR
      "OPENTELEMETRY_CPP_SRC_DIR must be defined when running cmake on this test project"
  )
endif()

message(
  STATUS
    "Adding opentelemetry-cpp as a subdirectory with FetchContent from ${OPENTELEMETRY_CPP_SRC_DIR}"
)

include(FetchContent)
FetchContent_Declare(opentelemetry-cpp SOURCE_DIR ${OPENTELEMETRY_CPP_SRC_DIR})
FetchContent_MakeAvailable(opentelemetry-cpp)

add_executable(
  fetch_content_src_test
  ${OPENTELEMETRY_CPP_SRC_DIR}/install/test/src/test_api.cc
  ${OPENTELEMETRY_CPP_SRC_DIR}/install/test/src/test_sdk.cc
  ${OPENTELEMETRY_CPP_SRC_DIR}/install/test/src/test_ext_common.cc
  ${OPENTELEMETRY_CPP_SRC_DIR}/install/test/src/test_ext_http_curl.cc
  ${OPENTELEMETRY_CPP_SRC_DIR}/install/test/src/test_exporters_ostream.cc
  ${OPENTELEMETRY_CPP_SRC_DIR}/install/test/src/test_exporters_in_memory.cc
  ${OPENTELEMETRY_CPP_SRC_DIR}/install/test/src/test_exporters_otlp_common.cc
  ${OPENTELEMETRY_CPP_SRC_DIR}/install/test/src/test_exporters_otlp_grpc.cc
  ${OPENTELEMETRY_CPP_SRC_DIR}/install/test/src/test_exporters_otlp_http.cc
  ${OPENTELEMETRY_CPP_SRC_DIR}/install/test/src/test_exporters_otlp_file.cc
  ${OPENTELEMETRY_CPP_SRC_DIR}/install/test/src/test_exporters_prometheus.cc
  ${OPENTELEMETRY_CPP_SRC_DIR}/install/test/src/test_exporters_zipkin.cc
  ${OPENTELEMETRY_CPP_SRC_DIR}/install/test/src/test_exporters_elasticsearch.cc)

target_link_libraries(
  fetch_content_src_test
  PRIVATE opentelemetry-cpp::api
          opentelemetry-cpp::version
          opentelemetry-cpp::metrics
          opentelemetry-cpp::trace
          opentelemetry-cpp::logs
          opentelemetry-cpp::http_client_curl
          opentelemetry-cpp::in_memory_span_exporter
          opentelemetry-cpp::in_memory_metric_exporter
          opentelemetry-cpp::ostream_log_record_exporter
          opentelemetry-cpp::ostream_metrics_exporter
          opentelemetry-cpp::ostream_span_exporter
          opentelemetry-cpp::otlp_file_exporter
          opentelemetry-cpp::otlp_file_log_record_exporter
          opentelemetry-cpp::otlp_file_metric_exporter
          opentelemetry-cpp::otlp_grpc_exporter
          opentelemetry-cpp::otlp_grpc_log_record_exporter
          opentelemetry-cpp::otlp_grpc_metrics_exporter
          opentelemetry-cpp::otlp_http_exporter
          opentelemetry-cpp::otlp_http_log_record_exporter
          opentelemetry-cpp::otlp_http_metric_exporter
          opentelemetry-cpp::prometheus_exporter
          opentelemetry-cpp::zipkin_trace_exporter
          opentelemetry-cpp::elasticsearch_log_record_exporter
          GTest::gtest
          GTest::gtest_main)

include(CTest)
include(GoogleTest)

gtest_add_tests(
  TARGET fetch_content_src_test
  TEST_PREFIX fetch_content.
  TEST_LIST fetch_content_src_test)
