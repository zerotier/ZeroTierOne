# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.14)
project(opentelemetry-cpp-exporters-prometheus-install-test LANGUAGES CXX)

find_package(opentelemetry-cpp REQUIRED COMPONENTS exporters_prometheus)

if(NOT TARGET prometheus-cpp::core)
  message(FATAL_ERROR "prometheus-cpp::core target not found")
endif()

find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(exporters_prometheus_test
               ${INSTALL_TEST_SRC_DIR}/test_exporters_prometheus.cc)
target_link_libraries(
  exporters_prometheus_test PRIVATE opentelemetry-cpp::prometheus_exporter
                                    GTest::gtest GTest::gtest_main)

gtest_discover_tests(exporters_prometheus_test)
