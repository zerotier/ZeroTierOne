# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.14)
project(opentelemetry-cpp-ext_http_curl-install-test LANGUAGES CXX)

find_package(opentelemetry-cpp REQUIRED COMPONENTS ext_http_curl)

if(NOT TARGET CURL::libcurl)
  message(FATAL_ERROR "CURL::libcurl target not found")
endif()

find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(ext_http_curl_test ${INSTALL_TEST_SRC_DIR}/test_ext_http_curl.cc)
target_link_libraries(
  ext_http_curl_test PRIVATE opentelemetry-cpp::http_client_curl GTest::gtest
                             GTest::gtest_main)

gtest_discover_tests(ext_http_curl_test)
