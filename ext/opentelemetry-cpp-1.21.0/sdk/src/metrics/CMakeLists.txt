# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

add_library(
  opentelemetry_metrics
  async_instruments.cc
  provider.cc
  meter_provider.cc
  meter_provider_factory.cc
  meter.cc
  meter_config.cc
  meter_context.cc
  meter_context_factory.cc
  metric_reader.cc
  instrument_metadata_validator.cc
  export/periodic_exporting_metric_reader.cc
  export/periodic_exporting_metric_reader_factory.cc
  state/filtered_ordered_attribute_map.cc
  state/metric_collector.cc
  state/observable_registry.cc
  state/sync_metric_storage.cc
  state/temporal_metric_storage.cc
  aggregation/base2_exponential_histogram_aggregation.cc
  aggregation/base2_exponential_histogram_indexer.cc
  aggregation/histogram_aggregation.cc
  aggregation/lastvalue_aggregation.cc
  aggregation/sum_aggregation.cc
  data/circular_buffer.cc
  exemplar/reservoir.cc
  view/instrument_selector_factory.cc
  view/meter_selector_factory.cc
  view/view_factory.cc
  view/view_registry_factory.cc
  sync_instruments.cc)

set_target_properties(opentelemetry_metrics PROPERTIES EXPORT_NAME metrics)
set_target_version(opentelemetry_metrics)

target_link_libraries(opentelemetry_metrics PUBLIC opentelemetry_common
                                                   opentelemetry_resources)

target_include_directories(
  opentelemetry_metrics
  PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/sdk/include>")

if(OPENTELEMETRY_INSTALL)
  opentelemetry_add_pkgconfig(
    metrics "OpenTelemetry SDK - Metrics"
    "Components for exporting metrics in the OpenTelemetry SDK."
    "opentelemetry_resources")
endif()
