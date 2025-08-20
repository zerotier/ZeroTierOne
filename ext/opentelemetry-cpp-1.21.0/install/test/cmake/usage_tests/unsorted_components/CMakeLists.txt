# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.14)

project(opentelemetry-cpp-unsorted-components-install-test LANGUAGES CXX)

# components not provided in order of dependency must be imported in the correct
# order without error
find_package(opentelemetry-cpp CONFIG REQUIRED COMPONENTS exporters_ostream api
                                                          sdk)

if(NOT OPENTELEMETRY_CPP_FOUND)
  message(
    FATAL_ERROR
      "calling find_package with out of order components must import the components and set OPENTELEMETRY_CPP_FOUND"
  )
endif()

if(NOT opentelemetry-cpp_FOUND)
  message(
    FATAL_ERROR
      "calling find_package with out of order components must import the components and set opentelemetry-cpp_FOUND"
  )
endif()

if(NOT opentelemetry-cpp_exporters_ostream_FOUND)
  message(
    FATAL_ERROR
      "calling find_package with out of order components must import the components and set opentelemetry-cpp_exporters_ostream_FOUND"
  )
endif()

if(NOT TARGET opentelemetry-cpp::api)
  message(FATAL_ERROR "opentelemetry-cpp::api target not found")
endif()

if(NOT TARGET opentelemetry-cpp::sdk)
  message(FATAL_ERROR "opentelemetry-cpp::sdk target not found")
endif()

if(NOT TARGET opentelemetry-cpp::ostream_log_record_exporter)
  message(
    FATAL_ERROR
      "opentelemetry-cpp::ostream_log_record_exporter target not found")
endif()
