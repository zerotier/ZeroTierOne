# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.14)

project(opentelemetry-cpp-full-package-install-test LANGUAGES CXX)

if(NOT DEFINED INSTALL_TEST_SRC_DIR)
  message(
    FATAL_ERROR
      "INSTALL_TEST_SRC_DIR must be defined when running cmake on this test project"
  )
endif()

find_package(opentelemetry-cpp CONFIG REQUIRED)

# Test that the required OPENTELEMETRY_CPP_* variables are set

if(NOT OPENTELEMETRY_CPP_INCLUDE_DIRS)
  message(FATAL_ERROR "OPENTELEMETRY_CPP_INCLUDE_DIRS is empty")
endif()

if(NOT OPENTELEMETRY_CPP_LIBRARY_DIRS)
  message(FATAL_ERROR "OPENTELEMETRY_CPP_LIBRARY_DIRS is empty")
endif()

if(NOT OPENTELEMETRY_CPP_LIBRARIES)
  message(FATAL_ERROR "OPENTELEMETRY_CPP_LIBRARIES is empty")
endif()

if(NOT OPENTELEMETRY_CPP_FOUND)
  message(FATAL_ERROR "OPENTELEMETRY_CPP_FOUND is not set")
endif()

if(NOT OPENTELEMETRY_ABI_VERSION_NO)
  message(FATAL_ERROR "OPENTELEMETRY_ABI_VERSION_NO is empty")
endif()

if(NOT OPENTELEMETRY_VERSION)
  message(FATAL_ERROR "OPENTELEMETRY_VERSION is empty")
endif()

if(NOT opentelemetry-cpp_FOUND)
  message(
    FATAL_ERROR
      "calling find_package must import the components and set opentelemetry-cpp_FOUND"
  )
endif()

if(NOT OPENTELEMETRY_CPP_COMPONENTS_INSTALLED)
  message(FATAL_ERROR "OPENTELEMETRY_CPP_COMPONENTS_INSTALLED is empty")
endif()

if(NOT INSTALL_TEST_COMPONENTS)
  message(FATAL_ERROR "INSTALL_TEST_COMPONENTS is empty")
endif()

message(
  STATUS
    "Testing the full package install on components = ${INSTALL_TEST_COMPONENTS}"
)

find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(full_test)

foreach(component IN LISTS INSTALL_TEST_COMPONENTS)
  message(STATUS "Adding test source for component ${component}")
  target_sources(full_test
                 PRIVATE "${INSTALL_TEST_SRC_DIR}/test_${component}.cc")
endforeach()

target_include_directories(full_test PRIVATE ${OPENTELEMETRY_CPP_INCLUDE_DIRS})
target_link_libraries(full_test PRIVATE ${OPENTELEMETRY_CPP_LIBRARIES}
                                        GTest::gtest GTest::gtest_main)

gtest_discover_tests(full_test)
