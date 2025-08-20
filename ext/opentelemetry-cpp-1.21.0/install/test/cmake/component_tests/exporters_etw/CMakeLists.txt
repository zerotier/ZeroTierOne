# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.14)
project(opentelemetry-cpp-exporters_etw-install-test LANGUAGES CXX)

find_package(opentelemetry-cpp REQUIRED COMPONENTS exporters_etw)

if(NOT TARGET nlohmann_json::nlohmann_json)
  message(FATAL_ERROR "nlohmann_json::nlohmann_json target not found")
endif()

find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(exporters_etw_test ${INSTALL_TEST_SRC_DIR}/test_exporters_etw.cc)
target_link_libraries(exporters_etw_test PRIVATE opentelemetry-cpp::etw_exporter
                                                 GTest::gtest GTest::gtest_main)

gtest_discover_tests(exporters_etw_test)
