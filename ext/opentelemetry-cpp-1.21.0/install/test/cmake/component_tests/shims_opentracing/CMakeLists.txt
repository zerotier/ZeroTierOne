# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.14)
project(opentelemetry-cpp-shims_opentracing-install-test LANGUAGES CXX)

find_package(opentelemetry-cpp REQUIRED COMPONENTS shims_opentracing)

if(NOT TARGET OpenTracing::opentracing)
  message(FATAL_ERROR "OpenTracing::opentracing target not found")
endif()

find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(shims_opentracing_test
               ${INSTALL_TEST_SRC_DIR}/test_shims_opentracing.cc)
target_link_libraries(
  shims_opentracing_test PRIVATE opentelemetry-cpp::opentracing_shim
                                 GTest::gtest GTest::gtest_main)

gtest_discover_tests(shims_opentracing_test)
