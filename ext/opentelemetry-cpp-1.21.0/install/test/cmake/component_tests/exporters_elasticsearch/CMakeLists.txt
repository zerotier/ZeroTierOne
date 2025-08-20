# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.14)
project(opentelemetry-cpp-exporters-elasticsearch-install-test LANGUAGES CXX)

find_package(opentelemetry-cpp REQUIRED COMPONENTS exporters_elasticsearch)

if(NOT TARGET nlohmann_json::nlohmann_json)
  message(FATAL_ERROR "nlohmann_json::nlohmann_json target not found")
endif()

find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(exporters_elasticsearch_test
               ${INSTALL_TEST_SRC_DIR}/test_exporters_elasticsearch.cc)
target_link_libraries(
  exporters_elasticsearch_test
  PRIVATE opentelemetry-cpp::elasticsearch_log_record_exporter GTest::gtest
          GTest::gtest_main)

gtest_discover_tests(exporters_elasticsearch_test)
