# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

add_library(opentelemetry_resources resource.cc resource_detector.cc)

set_target_properties(opentelemetry_resources PROPERTIES EXPORT_NAME resources)
set_target_version(opentelemetry_resources)

target_link_libraries(opentelemetry_resources PUBLIC opentelemetry_common)

target_include_directories(
  opentelemetry_resources
  PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/sdk/include>")

if(OPENTELEMETRY_INSTALL)
  opentelemetry_add_pkgconfig(
    resources "OpenTelemetry SDK - Resources"
    "Components for resource detection in the OpenTelemetry SDK."
    "opentelemetry_common")
endif()
