# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.14)
project(opentelemetry-cpp-exporters-otlp-common-install-test LANGUAGES CXX)

find_package(opentelemetry-cpp REQUIRED COMPONENTS exporters_otlp_common)

if(NOT TARGET protobuf::libprotobuf)
  message(FATAL_ERROR "protobuf::libprotobuf target not found")
endif()

find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(exporters_otlp_common_test
               ${INSTALL_TEST_SRC_DIR}/test_exporters_otlp_common.cc)
target_link_libraries(
  exporters_otlp_common_test
  PRIVATE opentelemetry-cpp::proto opentelemetry-cpp::otlp_recordable
          GTest::gtest GTest::gtest_main)

gtest_discover_tests(exporters_otlp_common_test)
