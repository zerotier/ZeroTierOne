// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <prometheus/metric_family.h>
#include <stddef.h>
#include <chrono>
#include <thread>
#include <vector>

#include "opentelemetry/exporters/prometheus/collector.h"
#include "opentelemetry/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/export/metric_producer.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/sdk/metrics/metric_reader.h"
#include "prometheus_test_helper.h"

using opentelemetry::exporter::metrics::PrometheusCollector;
using opentelemetry::sdk::metrics::MetricProducer;
using opentelemetry::sdk::metrics::ResourceMetrics;
namespace metric_api      = opentelemetry::metrics;
namespace metric_sdk      = opentelemetry::sdk::metrics;
namespace metric_exporter = opentelemetry::exporter::metrics;

class MockMetricProducer : public MetricProducer
{
  TestDataPoints test_data_points_;

public:
  MockMetricProducer(std::chrono::microseconds sleep_ms = std::chrono::microseconds::zero())
      : sleep_ms_{sleep_ms}
  {}

  MetricProducer::Result Produce() noexcept override
  {
    std::this_thread::sleep_for(sleep_ms_);
    data_sent_size_++;
    ResourceMetrics data = test_data_points_.CreateSumPointData();
    return {data, MetricProducer::Status::kSuccess};
  }

  size_t GetDataCount() { return data_sent_size_; }

private:
  std::chrono::microseconds sleep_ms_;
  size_t data_sent_size_{0};
};

class MockMetricReader : public opentelemetry::sdk::metrics::MetricReader
{
public:
  opentelemetry::sdk::metrics::AggregationTemporality GetAggregationTemporality(
      opentelemetry::sdk::metrics::InstrumentType /* instrument_type */) const noexcept override
  {
    // Prometheus exporter only support Cumulative
    return opentelemetry::sdk::metrics::AggregationTemporality::kCumulative;
  }

private:
  bool OnForceFlush(std::chrono::microseconds /* timeout */) noexcept override { return true; }

  bool OnShutDown(std::chrono::microseconds /* timeout */) noexcept override { return true; }

  void OnInitialized() noexcept override {}
};

// ==================== Test for addMetricsData() function ======================

/**
 * AddMetricData() should be able to successfully add a collection
 * of SumPointData. It checks that the cumulative
 * sum of updates to the aggregator of a record before and after AddMetricData()
 * is called are equal.
 */
TEST(PrometheusCollector, BasicTests)
{
  MockMetricReader reader;
  MockMetricProducer producer;
  reader.SetMetricProducer(&producer);
  PrometheusCollector collector(&reader, true, false);
  auto data = collector.Collect();

  // Collection size should be the same as the size
  // of the records collection produced by MetricProducer.
  ASSERT_EQ(data.size(), 2);
}
