# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.14)
project(opentelemetry-cpp-component-install-tests LANGUAGES CXX)

if(NOT INSTALL_TEST_CMAKE_OPTIONS)
  set(INSTALL_TEST_CMAKE_OPTIONS
      "-DCMAKE_CXX_STANDARD=14 -DCMAKE_BUILD_TYPE=Debug")
endif()

separate_arguments(INSTALL_TEST_CMAKE_OPTIONS)

message(
  STATUS "INSTALL_TEST_CMAKE_OPTIONS is set to ${INSTALL_TEST_CMAKE_OPTIONS}")

find_package(opentelemetry-cpp CONFIG REQUIRED)

message(
  STATUS
    "OPENTELEMETRY_CPP_COMPONENTS_INSTALLED = ${OPENTELEMETRY_CPP_COMPONENTS_INSTALLED}"
)

# Check that INSTALL_TEST_COMPONENTS is set and contains installed components
if(NOT INSTALL_TEST_COMPONENTS)
  message(
    STATUS
      "INSTALL_TEST_COMPONENTS is not set. Setting to OPENTELEMETRY_CPP_COMPONENTS_INSTALLED"
  )
  set(INSTALL_TEST_COMPONENTS ${OPENTELEMETRY_CPP_COMPONENTS_INSTALLED})
else()
  set(COMPONENTS_ARE_VALID TRUE)
  foreach(component ${INSTALL_TEST_COMPONENTS})
    if(NOT component IN_LIST OPENTELEMETRY_CPP_COMPONENTS_INSTALLED)
      message(
        ERROR
        " Component ${component} is not an installed opentelemetry-cpp component"
      )
      set(COMPONENTS_ARE_VALID FALSE)
    endif()
  endforeach()
  if(NOT COMPONENTS_ARE_VALID)
    message(FATAL_ERROR "INSTALL_TEST_COMPONENTS contains invalid components")
  endif()
endif()

message(STATUS "INSTALL_TEST_COMPONENTS = ${INSTALL_TEST_COMPONENTS}")

set(INSTALL_TEST_SRC_DIR "${CMAKE_SOURCE_DIR}/../src")

enable_testing()

# -----------------------------------------------------------
# CMake Usage tests for find_package(opentelemetry-cpp ...)
#
# 1. Test find_package with no components specified
# 2. Test find_package with components specified but not sorted in dependency
#    order
# 3. Test find_package with components specified but missing dependent components
# 4. Test find_package with components specified but including
#    unsupported/unknown components

add_test(
  NAME cmake-usage-no-components-test
  COMMAND
    ${CMAKE_COMMAND} --log-level=DEBUG -S
    ${CMAKE_SOURCE_DIR}/usage_tests/no_components -B
    build-cmake-usage-no-components-test
    "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}" ${INSTALL_TEST_CMAKE_OPTIONS})

add_test(
  NAME cmake-usage-unsorted-components-test
  COMMAND
    ${CMAKE_COMMAND} --log-level=DEBUG -S
    ${CMAKE_SOURCE_DIR}/usage_tests/unsorted_components -B
    build-cmake-usage-unsorted-components-test
    "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}" ${INSTALL_TEST_CMAKE_OPTIONS})

add_test(
  NAME cmake-usage-missing-components-test
  COMMAND
    ${CMAKE_COMMAND} --log-level=DEBUG -S
    ${CMAKE_SOURCE_DIR}/usage_tests/missing_components -B
    build-cmake-usage-missing-components-test
    "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}" ${INSTALL_TEST_CMAKE_OPTIONS})

add_test(
  NAME cmake-usage-unsupported-components-test
  COMMAND
    ${CMAKE_COMMAND} --log-level=DEBUG -S
    ${CMAKE_SOURCE_DIR}/usage_tests/unsupported_components -B
    build-cmake-usage-unsupported-components-test
    "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}" ${INSTALL_TEST_CMAKE_OPTIONS})

# -----------------------------------------------------------
# Test the full package install using legacy cmake build instructions
# find_package(opentelemetry-cpp CONFIG REQUIRED)

# Test cmake configuration
add_test(
  NAME full-package-cmake-config-test
  COMMAND
    ${CMAKE_COMMAND} --log-level=DEBUG -S ${CMAKE_SOURCE_DIR}/package_test -B
    build-full-package-test "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}"
    ${INSTALL_TEST_CMAKE_OPTIONS}
    "-DINSTALL_TEST_COMPONENTS=${INSTALL_TEST_COMPONENTS}"
    "-DINSTALL_TEST_SRC_DIR=${INSTALL_TEST_SRC_DIR}")

# Build the full package test executable
add_test(NAME full-package-build-test
         COMMAND ${CMAKE_COMMAND} --build
                 ${CMAKE_BINARY_DIR}/build-full-package-test --parallel)

# Run the full package test executable
add_test(NAME full-package-run-test
         COMMAND ${CMAKE_BINARY_DIR}/build-full-package-test/full_test)
# -----------------------------------------------------------

# -----------------------------------------------------------
# Loop over all the components to test
foreach(component ${INSTALL_TEST_COMPONENTS})
  # Test cmake configuration
  add_test(
    NAME component-${component}-cmake-config-test
    COMMAND
      ${CMAKE_COMMAND} --log-level=DEBUG -S
      ${CMAKE_SOURCE_DIR}/component_tests/${component} -B
      build-${component}-test "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}"
      ${INSTALL_TEST_CMAKE_OPTIONS}
      "-DINSTALL_TEST_SRC_DIR=${INSTALL_TEST_SRC_DIR}")

  # Build the component test executable
  add_test(NAME component-${component}-build-test
           COMMAND ${CMAKE_COMMAND} --build
                   ${CMAKE_BINARY_DIR}/build-${component}-test --parallel)

  # Run the component test executable
  add_test(
    NAME component-${component}-run-test
    COMMAND ${CMAKE_BINARY_DIR}/build-${component}-test/${component}_test)
endforeach()
