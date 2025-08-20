# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.14)

project(opentelemetry-cpp-unsupported-components-install-test LANGUAGES CXX)

# request an unsupported/unknown component (without the REQUIRED arg)
find_package(opentelemetry-cpp CONFIG COMPONENTS api an_unknown_component)

if(OPENTELEMETRY_CPP_FOUND)
  message(
    FATAL_ERROR
      "calling find_package with an unsuported component should not set OPENTELEMETRY_CPP_FOUND"
  )
endif()

if(opentelemetry-cpp_FOUND)
  message(
    FATAL_ERROR
      "calling find_package with an unsupported component should not set opentelemetry-cpp_FOUND"
  )
endif()
