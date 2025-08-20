# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

if(NOT TARGET prometheus-cpp::core)
  find_package(prometheus-cpp CONFIG REQUIRED)
endif()

add_library(
  opentelemetry_exporter_prometheus
  src/exporter.cc src/exporter_options.cc src/exporter_factory.cc
  src/collector.cc src/exporter_utils.cc)

set_target_properties(opentelemetry_exporter_prometheus
                      PROPERTIES EXPORT_NAME prometheus_exporter)
set_target_version(opentelemetry_exporter_prometheus)

target_include_directories(
  opentelemetry_exporter_prometheus
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
         "$<INSTALL_INTERFACE:include>")

set(PROMETHEUS_EXPORTER_TARGETS opentelemetry_exporter_prometheus)
if(TARGET pull)
  list(APPEND PROMETHEUS_EXPORTER_TARGETS pull)
endif()
if(TARGET core)
  list(APPEND PROMETHEUS_EXPORTER_TARGETS core)
endif()
if(TARGET util)
  list(APPEND PROMETHEUS_EXPORTER_TARGETS util)
endif()
set(PROMETHEUS_CPP_TARGETS prometheus-cpp::pull prometheus-cpp::core)
if(TARGET prometheus-cpp::util)
  list(APPEND PROMETHEUS_CPP_TARGETS prometheus-cpp::util)
endif()
target_link_libraries(opentelemetry_exporter_prometheus
                      PUBLIC opentelemetry_metrics ${PROMETHEUS_CPP_TARGETS})

otel_add_component(
  COMPONENT
  exporters_prometheus
  TARGETS
  opentelemetry_exporter_prometheus
  FILES_DIRECTORY
  "include/opentelemetry/exporters/prometheus"
  FILES_DESTINATION
  "include/opentelemetry/exporters"
  FILES_MATCHING
  PATTERN
  "*.h")

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
