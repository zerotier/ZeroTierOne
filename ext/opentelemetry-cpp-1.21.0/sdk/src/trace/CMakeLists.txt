# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

add_library(
  opentelemetry_trace
  tracer_context.cc
  tracer_context_factory.cc
  tracer_provider.cc
  tracer_provider_factory.cc
  tracer.cc
  span.cc
  exporter.cc
  provider.cc
  batch_span_processor.cc
  batch_span_processor_factory.cc
  simple_processor_factory.cc
  samplers/always_on_factory.cc
  samplers/always_off_factory.cc
  samplers/parent.cc
  samplers/parent_factory.cc
  samplers/trace_id_ratio.cc
  samplers/trace_id_ratio_factory.cc
  random_id_generator.cc
  random_id_generator_factory.cc
  tracer_config.cc)

set_target_properties(opentelemetry_trace PROPERTIES EXPORT_NAME trace)
set_target_version(opentelemetry_trace)

target_link_libraries(opentelemetry_trace PUBLIC opentelemetry_common
                                                 opentelemetry_resources)

target_include_directories(
  opentelemetry_trace
  PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/sdk/include>")

if(OPENTELEMETRY_INSTALL)
  opentelemetry_add_pkgconfig(
    trace "OpenTelemetry SDK - Trace"
    "Components for exporting traces in the OpenTelemetry SDK."
    "opentelemetry_resources")
endif()
