# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.14)

project(opentelemetry-cpp-api-install-test LANGUAGES CXX)

find_package(opentelemetry-cpp REQUIRED COMPONENTS api)

find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(api_test ${INSTALL_TEST_SRC_DIR}/test_api.cc)
target_link_libraries(api_test PRIVATE opentelemetry-cpp::api GTest::gtest
                                       GTest::gtest_main)

gtest_discover_tests(api_test)
