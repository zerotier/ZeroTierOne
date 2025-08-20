# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

add_library(opentelemetry_sdk INTERFACE)
target_include_directories(
  opentelemetry_sdk
  INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
            "$<INSTALL_INTERFACE:include>")

set_target_properties(opentelemetry_sdk PROPERTIES EXPORT_NAME sdk)

target_link_libraries(opentelemetry_sdk INTERFACE opentelemetry_api)

set(OTEL_SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR})
add_subdirectory(src)

otel_add_component(
  COMPONENT
  sdk
  TARGETS
  opentelemetry_sdk
  opentelemetry_common
  opentelemetry_resources
  opentelemetry_version
  opentelemetry_logs
  opentelemetry_trace
  opentelemetry_metrics
  FILES_DIRECTORY
  "include/opentelemetry/"
  FILES_DESTINATION
  "include/opentelemetry"
  FILES_MATCHING
  PATTERN
  "*.h")

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
