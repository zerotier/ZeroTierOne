# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

add_library(
  opentelemetry_logs
  provider.cc
  logger_provider.cc
  logger_provider_factory.cc
  logger.cc
  exporter.cc
  event_logger_provider.cc
  event_logger_provider_factory.cc
  event_logger.cc
  simple_log_record_processor.cc
  simple_log_record_processor_factory.cc
  batch_log_record_processor.cc
  batch_log_record_processor_factory.cc
  logger_config.cc
  logger_context.cc
  logger_context_factory.cc
  multi_log_record_processor.cc
  multi_log_record_processor_factory.cc
  multi_recordable.cc
  read_write_log_record.cc
  readable_log_record.cc)

set_target_properties(opentelemetry_logs PROPERTIES EXPORT_NAME logs)

target_link_libraries(opentelemetry_logs PUBLIC opentelemetry_resources
                                                opentelemetry_common)
set_target_version(opentelemetry_logs)

target_include_directories(
  opentelemetry_logs
  PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/sdk/include>")

if(OPENTELEMETRY_INSTALL)
  opentelemetry_add_pkgconfig(
    logs "OpenTelemetry SDK - Logs"
    "Components for exporting logs in the OpenTelemetry SDK."
    "opentelemetry_resources")
endif()
