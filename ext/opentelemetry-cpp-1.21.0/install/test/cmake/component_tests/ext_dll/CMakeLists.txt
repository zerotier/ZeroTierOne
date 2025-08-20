# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.14)
project(opentelemetry-cpp-ext_dll-install-test LANGUAGES CXX)

find_package(opentelemetry-cpp REQUIRED COMPONENTS ext_dll exporters_ostream)

find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(ext_dll_test ${INSTALL_TEST_SRC_DIR}/test_ext_dll.cc)

target_compile_definitions(ext_dll_test PRIVATE OPENTELEMETRY_BUILD_IMPORT_DLL)

target_link_libraries(ext_dll_test PRIVATE opentelemetry-cpp::opentelemetry_cpp
                                           GTest::gtest GTest::gtest_main)

gtest_discover_tests(ext_dll_test)
