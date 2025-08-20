# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.14)
project(opentelemetry-cpp-exporters-otlp-http-install-test LANGUAGES CXX)

find_package(opentelemetry-cpp REQUIRED COMPONENTS exporters_otlp_http)

if(NOT TARGET protobuf::libprotobuf)
  message(FATAL_ERROR "protobuf::libprotobuf target not found")
endif()

find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(exporters_otlp_http_test
               ${INSTALL_TEST_SRC_DIR}/test_exporters_otlp_http.cc)
target_link_libraries(
  exporters_otlp_http_test
  PRIVATE opentelemetry-cpp::otlp_http_client
          opentelemetry-cpp::otlp_http_exporter
          opentelemetry-cpp::otlp_http_log_record_exporter
          opentelemetry-cpp::otlp_http_metric_exporter
          GTest::gtest
          GTest::gtest_main)

gtest_discover_tests(exporters_otlp_http_test)
