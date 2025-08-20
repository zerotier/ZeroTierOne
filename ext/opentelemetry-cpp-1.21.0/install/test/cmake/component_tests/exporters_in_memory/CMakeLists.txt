# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.14)
project(opentelemetry-cpp-exporters-in-memory-install-test LANGUAGES CXX)

find_package(opentelemetry-cpp REQUIRED COMPONENTS exporters_in_memory)

find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(exporters_in_memory_test
               ${INSTALL_TEST_SRC_DIR}/test_exporters_in_memory.cc)
target_link_libraries(
  exporters_in_memory_test
  PRIVATE opentelemetry-cpp::in_memory_span_exporter
          opentelemetry-cpp::in_memory_metric_exporter GTest::gtest
          GTest::gtest_main)

gtest_discover_tests(exporters_in_memory_test)
