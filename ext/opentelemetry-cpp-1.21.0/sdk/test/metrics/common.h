// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chrono>
#include <memory>

#include "opentelemetry/sdk/common/exporter_utils.h"
#include "opentelemetry/sdk/metrics/export/metric_producer.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/sdk/metrics/metric_reader.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"
#include "opentelemetry/sdk/metrics/state/metric_collector.h"

class MockMetricExporter : public opentelemetry::sdk::metrics::PushMetricExporter
{
public:
  MockMetricExporter() = default;
  opentelemetry::sdk::common::ExportResult Export(
      const opentelemetry::sdk::metrics::ResourceMetrics &) noexcept override;

  opentelemetry::sdk::metrics::AggregationTemporality GetAggregationTemporality(
      opentelemetry::sdk::metrics::InstrumentType) const noexcept override;

  bool ForceFlush(std::chrono::microseconds) noexcept override;

  bool Shutdown(std::chrono::microseconds) noexcept override;
};

class MockMetricReader : public opentelemetry::sdk::metrics::MetricReader
{
public:
  MockMetricReader(std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>);

  MockMetricReader();

  opentelemetry::sdk::metrics::AggregationTemporality GetAggregationTemporality(
      opentelemetry::sdk::metrics::InstrumentType) const noexcept override;

  bool OnForceFlush(std::chrono::microseconds) noexcept override;

  bool OnShutDown(std::chrono::microseconds) noexcept override;

  void OnInitialized() noexcept override;

private:
  std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter_;
};

class MockCollectorHandle : public opentelemetry::sdk::metrics::CollectorHandle
{
public:
  MockCollectorHandle(opentelemetry::sdk::metrics::AggregationTemporality);

  ~MockCollectorHandle() override = default;

  opentelemetry::sdk::metrics::AggregationTemporality GetAggregationTemporality(
      opentelemetry::sdk::metrics::InstrumentType) noexcept override;

private:
  opentelemetry::sdk::metrics::AggregationTemporality temporality_;
};
