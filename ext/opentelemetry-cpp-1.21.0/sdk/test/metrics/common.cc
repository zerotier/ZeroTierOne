// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "common.h"
#include <utility>

#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"

using namespace opentelemetry::sdk::instrumentationscope;
using namespace opentelemetry::sdk::metrics;
using namespace opentelemetry::sdk::common;

// MockMetricExporter

ExportResult MockMetricExporter::Export(const ResourceMetrics & /*resource_metrics*/) noexcept
{
  return ExportResult::kSuccess;
}

AggregationTemporality MockMetricExporter::GetAggregationTemporality(
    InstrumentType /*instrument_type*/) const noexcept
{
  return AggregationTemporality::kCumulative;
}

bool MockMetricExporter::ForceFlush(std::chrono::microseconds /* timeout */) noexcept
{
  return true;
}

bool MockMetricExporter::Shutdown(std::chrono::microseconds /* timeout */) noexcept
{
  return true;
}

// MockMetricReader
MockMetricReader::MockMetricReader(std::unique_ptr<PushMetricExporter> exporter)
    : exporter_(std::move(exporter))
{}

MockMetricReader::MockMetricReader() : exporter_{new MockMetricExporter()} {}

AggregationTemporality MockMetricReader::GetAggregationTemporality(
    InstrumentType instrument_type) const noexcept

{
  return exporter_->GetAggregationTemporality(instrument_type);
}

bool MockMetricReader::OnForceFlush(std::chrono::microseconds /* timeout */) noexcept
{
  return true;
}

bool MockMetricReader::OnShutDown(std::chrono::microseconds /* timeout */) noexcept
{
  return true;
}

void MockMetricReader::OnInitialized() noexcept {}

// MockCollectorHandle

MockCollectorHandle::MockCollectorHandle(AggregationTemporality temp) : temporality_(temp) {}

AggregationTemporality MockCollectorHandle::GetAggregationTemporality(
    InstrumentType /* instrument_type */) noexcept
{
  return temporality_;
}
