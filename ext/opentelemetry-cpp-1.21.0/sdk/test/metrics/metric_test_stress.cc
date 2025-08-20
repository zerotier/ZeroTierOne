// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <stdint.h>
#include <atomic>
#include <chrono>
#include <random>
#include <thread>
#include <utility>
#include <vector>

#include "common.h"
#include "opentelemetry/context/context.h"
#include "opentelemetry/metrics/meter.h"
#include "opentelemetry/metrics/sync_instruments.h"
#include "opentelemetry/nostd/function_ref.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/unique_ptr.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/sdk/common/exporter_utils.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/metrics/data/metric_data.h"
#include "opentelemetry/sdk/metrics/data/point_data.h"
#include "opentelemetry/sdk/metrics/export/metric_producer.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/metric_reader.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"

using namespace opentelemetry;
using namespace opentelemetry::sdk::instrumentationscope;
using namespace opentelemetry::sdk::metrics;

class MockMetricExporterForStress : public opentelemetry::sdk::metrics::PushMetricExporter
{
public:
  MockMetricExporterForStress() = default;

  opentelemetry::sdk::metrics::AggregationTemporality GetAggregationTemporality(
      opentelemetry::sdk::metrics::InstrumentType) const noexcept override
  {
    return AggregationTemporality::kDelta;
  }

  opentelemetry::sdk::common::ExportResult Export(
      const opentelemetry::sdk::metrics::ResourceMetrics &) noexcept override
  {
    return opentelemetry::sdk::common::ExportResult::kSuccess;
  }

  bool ForceFlush(std::chrono::microseconds) noexcept override { return true; }

  bool Shutdown(std::chrono::microseconds) noexcept override { return true; }
};

TEST(HistogramStress, UnsignedInt64)
{
  MeterProvider mp;
  auto m = mp.GetMeter("meter1", "version1", "schema1");

  std::unique_ptr<MockMetricExporterForStress> exporter(new MockMetricExporterForStress());
  std::shared_ptr<MetricReader> reader{new MockMetricReader(std::move(exporter))};
  mp.AddMetricReader(reader);

  auto h = m->CreateUInt64Histogram("histogram1", "histogram1_description", "histogram1_unit");

  //
  // Start a dedicated thread to collect the metrics
  //
  std::vector<HistogramPointData> actuals;
  auto stop_collecting = std::make_shared<std::atomic<bool>>(false);
  auto collect_thread  = std::thread([&reader, &actuals, stop_collecting]() {
    while (!*stop_collecting)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      reader->Collect([&](ResourceMetrics &rm) {
        for (const ScopeMetrics &smd : rm.scope_metric_data_)
        {
          for (const MetricData &md : smd.metric_data_)
          {
            for (const PointDataAttributes &dp : md.point_data_attr_)
            {
              actuals.push_back(opentelemetry::nostd::get<HistogramPointData>(dp.point_data));
            }
          }
        }
        return true;
      });
    }
  });

  //
  // Start logging threads
  //
  int record_thread_count = std::thread::hardware_concurrency() - 1;
  if (record_thread_count <= 0)
  {
    record_thread_count = 1;
  }

  std::vector<std::thread> threads(record_thread_count);
  constexpr int iterations_per_thread = 2000000;
  auto expected_sum                   = std::make_shared<std::atomic<uint64_t>>(0);

  for (int i = 0; i < record_thread_count; ++i)
  {
    threads[i] = std::thread([&] {
      std::random_device rd;
      std::mt19937 random_engine(rd());
      std::uniform_int_distribution<> gen_random(1, 20000);

      for (int j = 0; j < iterations_per_thread; ++j)
      {
        int64_t val = gen_random(random_engine);
        expected_sum->fetch_add(val, std::memory_order_relaxed);
        h->Record(val, {});
      }
    });
  }

  for (int i = 0; i < record_thread_count; ++i)
  {
    threads[i].join();
  }

  //
  // Stop the dedicated collection thread
  //
  *stop_collecting = true;
  collect_thread.join();

  //
  // run the the final collection
  //
  reader->Collect([&](ResourceMetrics &rm) {
    for (const ScopeMetrics &smd : rm.scope_metric_data_)
    {
      for (const MetricData &md : smd.metric_data_)
      {
        for (const PointDataAttributes &dp : md.point_data_attr_)
        {
          actuals.push_back(opentelemetry::nostd::get<HistogramPointData>(dp.point_data));
        }
      }
    }
    return true;
  });

  //
  // Aggregate the results
  //
  int64_t expected_count  = record_thread_count * iterations_per_thread;
  int64_t collected_count = 0;
  int64_t collected_sum   = 0;
  for (const auto &actual : actuals)
  {
    int64_t collected_bucket_sum = 0;
    for (const auto &count : actual.counts_)
    {
      collected_bucket_sum += count;
    }
    ASSERT_EQ(collected_bucket_sum, actual.count_);

    collected_sum += opentelemetry::nostd::get<int64_t>(actual.sum_);
    collected_count += actual.count_;
  }

  ASSERT_EQ(expected_count, collected_count);
  ASSERT_EQ(*expected_sum, collected_sum);
}
