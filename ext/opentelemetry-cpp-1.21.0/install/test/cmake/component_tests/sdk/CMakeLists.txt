# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.14)
project(opentelemetry-cpp-sdk-install-test LANGUAGES CXX)

find_package(opentelemetry-cpp REQUIRED COMPONENTS sdk)

if(NOT TARGET Threads::Threads)
  message(FATAL_ERROR "Threads::Threads target not found")
endif()

find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(sdk_test ${INSTALL_TEST_SRC_DIR}/test_sdk.cc)
target_link_libraries(
  sdk_test
  PRIVATE opentelemetry-cpp::api
          opentelemetry-cpp::sdk
          opentelemetry-cpp::version
          opentelemetry-cpp::common
          opentelemetry-cpp::resources
          opentelemetry-cpp::trace
          opentelemetry-cpp::metrics
          opentelemetry-cpp::logs
          GTest::gtest
          GTest::gtest_main)

gtest_discover_tests(sdk_test)
