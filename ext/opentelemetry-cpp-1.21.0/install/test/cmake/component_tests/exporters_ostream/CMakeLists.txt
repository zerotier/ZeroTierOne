# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.14)
project(opentelemetry-cpp-exporters-ostream-install-test LANGUAGES CXX)

find_package(opentelemetry-cpp REQUIRED COMPONENTS exporters_ostream)

find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(exporters_ostream_test
               ${INSTALL_TEST_SRC_DIR}/test_exporters_ostream.cc)
target_link_libraries(
  exporters_ostream_test
  PRIVATE opentelemetry-cpp::ostream_log_record_exporter
          opentelemetry-cpp::ostream_metrics_exporter
          opentelemetry-cpp::ostream_span_exporter
          GTest::gtest
          GTest::gtest_main)

gtest_discover_tests(exporters_ostream_test)
