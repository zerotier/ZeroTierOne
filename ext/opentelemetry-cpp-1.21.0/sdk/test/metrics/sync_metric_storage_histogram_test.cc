// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "common.h"

#include "opentelemetry/common/key_value_iterable_view.h"
#include "opentelemetry/context/context.h"
#include "opentelemetry/nostd/function_ref.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/sdk/metrics/data/circular_buffer.h"
#include "opentelemetry/sdk/metrics/data/metric_data.h"
#include "opentelemetry/sdk/metrics/data/point_data.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/sdk/metrics/state/metric_collector.h"
#include "opentelemetry/sdk/metrics/state/sync_metric_storage.h"
#include "opentelemetry/sdk/metrics/view/attributes_processor.h"

#ifdef ENABLE_METRICS_EXEMPLAR_PREVIEW
#  include "opentelemetry/sdk/metrics/exemplar/filter_type.h"
#  include "opentelemetry/sdk/metrics/exemplar/reservoir.h"
#endif

using namespace opentelemetry::sdk::metrics;
using namespace opentelemetry::common;
using M         = std::map<std::string, std::string>;
namespace nostd = opentelemetry::nostd;

class WritableMetricStorageHistogramTestFixture
    : public ::testing::TestWithParam<AggregationTemporality>
{};

TEST_P(WritableMetricStorageHistogramTestFixture, LongHistogram)
{
  AggregationTemporality temporality  = GetParam();
  auto sdk_start_ts                   = std::chrono::system_clock::now();
  int64_t expected_total_get_requests = 0;
  int64_t expected_total_put_requests = 0;
  InstrumentDescriptor instr_desc     = {"name", "desc", "1unit", InstrumentType::kHistogram,
                                         InstrumentValueType::kLong};
  std::map<std::string, std::string> attributes_get = {{"RequestType", "GET"}};
  std::map<std::string, std::string> attributes_put = {{"RequestType", "PUT"}};

  std::unique_ptr<DefaultAttributesProcessor> default_attributes_processor{
      new DefaultAttributesProcessor{}};
  opentelemetry::sdk::metrics::SyncMetricStorage storage(
      instr_desc, AggregationType::kHistogram, default_attributes_processor.get(),
#ifdef ENABLE_METRICS_EXEMPLAR_PREVIEW
      ExemplarFilterType::kAlwaysOff, ExemplarReservoir::GetNoExemplarReservoir(),
#endif
      nullptr);

  storage.RecordLong(10, KeyValueIterableView<std::map<std::string, std::string>>(attributes_get),
                     opentelemetry::context::Context{});
  expected_total_get_requests += 10;

  storage.RecordLong(30, KeyValueIterableView<std::map<std::string, std::string>>(attributes_put),
                     opentelemetry::context::Context{});
  expected_total_put_requests += 30;

  storage.RecordLong(20, KeyValueIterableView<std::map<std::string, std::string>>(attributes_get),
                     opentelemetry::context::Context{});
  expected_total_get_requests += 20;

  storage.RecordLong(40, KeyValueIterableView<std::map<std::string, std::string>>(attributes_put),
                     opentelemetry::context::Context{});
  expected_total_put_requests += 40;

  std::shared_ptr<CollectorHandle> collector(new MockCollectorHandle(temporality));
  std::vector<std::shared_ptr<CollectorHandle>> collectors;
  collectors.push_back(collector);

  // Some computation here
  auto collection_ts      = std::chrono::system_clock::now();
  size_t count_attributes = 0;
  storage.Collect(
      collector.get(), collectors, sdk_start_ts, collection_ts, [&](const MetricData &metric_data) {
        for (const auto &data_attr : metric_data.point_data_attr_)
        {
          const auto &data = opentelemetry::nostd::get<HistogramPointData>(data_attr.point_data);
          if (opentelemetry::nostd::get<std::string>(
                  data_attr.attributes.find("RequestType")->second) == "GET")
          {
            EXPECT_EQ(opentelemetry::nostd::get<int64_t>(data.sum_), expected_total_get_requests);
            count_attributes++;
          }
          else if (opentelemetry::nostd::get<std::string>(
                       data_attr.attributes.find("RequestType")->second) == "PUT")
          {
            EXPECT_EQ(opentelemetry::nostd::get<int64_t>(data.sum_), expected_total_put_requests);
            count_attributes++;
          }
        }
        return true;
      });
  EXPECT_EQ(count_attributes, 2);  // GET and PUT
  // In case of delta temporarily, subsequent collection would contain new data points, so resetting
  // the counts
  if (temporality == AggregationTemporality::kDelta)
  {
    expected_total_get_requests = 0;
    expected_total_put_requests = 0;
  }

  // collect one more time.
  collection_ts    = std::chrono::system_clock::now();
  count_attributes = 0;
  storage.Collect(
      collector.get(), collectors, sdk_start_ts, collection_ts, [&](const MetricData &metric_data) {
        if (temporality == AggregationTemporality::kCumulative)
        {
          EXPECT_EQ(metric_data.start_ts, sdk_start_ts);
        }
        for (const auto &data_attr : metric_data.point_data_attr_)
        {
          const auto &data = opentelemetry::nostd::get<HistogramPointData>(data_attr.point_data);
          if (opentelemetry::nostd::get<std::string>(
                  data_attr.attributes.find("RequestType")->second) == "GET")
          {
            count_attributes++;
            EXPECT_EQ(opentelemetry::nostd::get<int64_t>(data.sum_), expected_total_get_requests);
          }
          else if (opentelemetry::nostd::get<std::string>(
                       data_attr.attributes.find("RequestType")->second) == "PUT")
          {
            count_attributes++;
            EXPECT_EQ(opentelemetry::nostd::get<int64_t>(data.sum_), expected_total_put_requests);
          }
        }
        return true;
      });
  if (temporality == AggregationTemporality::kCumulative)
  {
    EXPECT_EQ(count_attributes, 2);  // GET AND PUT
  }

  storage.RecordLong(50, KeyValueIterableView<std::map<std::string, std::string>>(attributes_get),
                     opentelemetry::context::Context{});
  expected_total_get_requests += 50;
  storage.RecordLong(40, KeyValueIterableView<std::map<std::string, std::string>>(attributes_put),
                     opentelemetry::context::Context{});
  expected_total_put_requests += 40;

  collection_ts    = std::chrono::system_clock::now();
  count_attributes = 0;
  storage.Collect(
      collector.get(), collectors, sdk_start_ts, collection_ts, [&](const MetricData &metric_data) {
        for (const auto &data_attr : metric_data.point_data_attr_)
        {
          const auto &data = opentelemetry::nostd::get<HistogramPointData>(data_attr.point_data);
          if (opentelemetry::nostd::get<std::string>(
                  data_attr.attributes.find("RequestType")->second) == "GET")
          {
            EXPECT_EQ(opentelemetry::nostd::get<int64_t>(data.sum_), expected_total_get_requests);
            count_attributes++;
          }
          else if (opentelemetry::nostd::get<std::string>(
                       data_attr.attributes.find("RequestType")->second) == "PUT")
          {
            EXPECT_EQ(opentelemetry::nostd::get<int64_t>(data.sum_), expected_total_put_requests);
            count_attributes++;
          }
        }
        return true;
      });
  EXPECT_EQ(count_attributes, 2);  // GET and PUT
}

INSTANTIATE_TEST_SUITE_P(WritableMetricStorageHistogramTestLong,
                         WritableMetricStorageHistogramTestFixture,
                         ::testing::Values(AggregationTemporality::kCumulative,
                                           AggregationTemporality::kDelta));

TEST_P(WritableMetricStorageHistogramTestFixture, DoubleHistogram)
{
  AggregationTemporality temporality = GetParam();
  auto sdk_start_ts                  = std::chrono::system_clock::now();
  double expected_total_get_requests = 0;
  double expected_total_put_requests = 0;
  InstrumentDescriptor instr_desc    = {"name", "desc", "1unit", InstrumentType::kHistogram,
                                        InstrumentValueType::kDouble};
  std::map<std::string, std::string> attributes_get = {{"RequestType", "GET"}};
  std::map<std::string, std::string> attributes_put = {{"RequestType", "PUT"}};

  std::unique_ptr<DefaultAttributesProcessor> default_attributes_processor{
      new DefaultAttributesProcessor{}};
  opentelemetry::sdk::metrics::SyncMetricStorage storage(
      instr_desc, AggregationType::kHistogram, default_attributes_processor.get(),
#ifdef ENABLE_METRICS_EXEMPLAR_PREVIEW
      ExemplarFilterType::kAlwaysOff, ExemplarReservoir::GetNoExemplarReservoir(),
#endif
      nullptr);

  storage.RecordDouble(10.0,
                       KeyValueIterableView<std::map<std::string, std::string>>(attributes_get),
                       opentelemetry::context::Context{});
  expected_total_get_requests += 10;

  storage.RecordDouble(30.0,
                       KeyValueIterableView<std::map<std::string, std::string>>(attributes_put),
                       opentelemetry::context::Context{});
  expected_total_put_requests += 30;

  storage.RecordDouble(20.0,
                       KeyValueIterableView<std::map<std::string, std::string>>(attributes_get),
                       opentelemetry::context::Context{});
  expected_total_get_requests += 20;

  storage.RecordDouble(40.0,
                       KeyValueIterableView<std::map<std::string, std::string>>(attributes_put),
                       opentelemetry::context::Context{});
  expected_total_put_requests += 40;

  std::shared_ptr<CollectorHandle> collector(new MockCollectorHandle(temporality));
  std::vector<std::shared_ptr<CollectorHandle>> collectors;
  collectors.push_back(collector);

  // Some computation here
  auto collection_ts      = std::chrono::system_clock::now();
  size_t count_attributes = 0;
  storage.Collect(
      collector.get(), collectors, sdk_start_ts, collection_ts, [&](const MetricData &metric_data) {
        for (const auto &data_attr : metric_data.point_data_attr_)
        {
          const auto &data = opentelemetry::nostd::get<HistogramPointData>(data_attr.point_data);
          if (opentelemetry::nostd::get<std::string>(
                  data_attr.attributes.find("RequestType")->second) == "GET")
          {
            EXPECT_EQ(opentelemetry::nostd::get<double>(data.sum_), expected_total_get_requests);
            count_attributes++;
          }
          else if (opentelemetry::nostd::get<std::string>(
                       data_attr.attributes.find("RequestType")->second) == "PUT")
          {
            EXPECT_EQ(opentelemetry::nostd::get<double>(data.sum_), expected_total_put_requests);
            count_attributes++;
          }
        }
        return true;
      });
  EXPECT_EQ(count_attributes, 2);  // GET and PUT

  // In case of delta temporarily, subsequent collection would contain new data points, so resetting
  // the counts
  if (temporality == AggregationTemporality::kDelta)
  {
    expected_total_get_requests = 0;
    expected_total_put_requests = 0;
  }

  // collect one more time.
  collection_ts    = std::chrono::system_clock::now();
  count_attributes = 0;
  storage.Collect(
      collector.get(), collectors, sdk_start_ts, collection_ts, [&](const MetricData &metric_data) {
        if (temporality == AggregationTemporality::kCumulative)
        {
          EXPECT_EQ(metric_data.start_ts, sdk_start_ts);
        }
        for (const auto &data_attr : metric_data.point_data_attr_)
        {
          const auto &data = opentelemetry::nostd::get<HistogramPointData>(data_attr.point_data);
          if (opentelemetry::nostd::get<std::string>(
                  data_attr.attributes.find("RequestType")->second) == "GET")
          {
            count_attributes++;
            EXPECT_EQ(opentelemetry::nostd::get<double>(data.sum_), expected_total_get_requests);
          }
          else if (opentelemetry::nostd::get<std::string>(
                       data_attr.attributes.find("RequestType")->second) == "PUT")
          {
            count_attributes++;
            EXPECT_EQ(opentelemetry::nostd::get<double>(data.sum_), expected_total_put_requests);
          }
        }
        return true;
      });
  if (temporality == AggregationTemporality::kCumulative)
  {
    EXPECT_EQ(count_attributes, 2);  // GET AND PUT
  }

  storage.RecordDouble(50.0,
                       KeyValueIterableView<std::map<std::string, std::string>>(attributes_get),
                       opentelemetry::context::Context{});
  expected_total_get_requests += 50;
  storage.RecordDouble(40.0,
                       KeyValueIterableView<std::map<std::string, std::string>>(attributes_put),
                       opentelemetry::context::Context{});
  expected_total_put_requests += 40;

  collection_ts    = std::chrono::system_clock::now();
  count_attributes = 0;
  storage.Collect(
      collector.get(), collectors, sdk_start_ts, collection_ts, [&](const MetricData &metric_data) {
        for (const auto &data_attr : metric_data.point_data_attr_)
        {
          const auto &data = opentelemetry::nostd::get<HistogramPointData>(data_attr.point_data);
          if (opentelemetry::nostd::get<std::string>(
                  data_attr.attributes.find("RequestType")->second) == "GET")
          {
            EXPECT_EQ(opentelemetry::nostd::get<double>(data.sum_), expected_total_get_requests);
            count_attributes++;
          }
          else if (opentelemetry::nostd::get<std::string>(
                       data_attr.attributes.find("RequestType")->second) == "PUT")
          {
            EXPECT_EQ(opentelemetry::nostd::get<double>(data.sum_), expected_total_put_requests);
            count_attributes++;
          }
        }
        return true;
      });
  EXPECT_EQ(count_attributes, 2);  // GET and PUT
}

INSTANTIATE_TEST_SUITE_P(WritableMetricStorageHistogramTestDouble,
                         WritableMetricStorageHistogramTestFixture,
                         ::testing::Values(AggregationTemporality::kCumulative,
                                           AggregationTemporality::kDelta));

TEST_P(WritableMetricStorageHistogramTestFixture, Base2ExponentialDoubleHistogram)
{
  AggregationTemporality temporality = GetParam();
  auto sdk_start_ts                  = std::chrono::system_clock::now();
  double expected_total_get_requests = 0;
  double expected_total_put_requests = 0;
  InstrumentDescriptor instr_desc    = {"name", "desc", "1unit", InstrumentType::kHistogram,
                                        InstrumentValueType::kDouble};
  std::map<std::string, std::string> attributes_get = {{"RequestType", "GET"}};
  std::map<std::string, std::string> attributes_put = {{"RequestType", "PUT"}};

  std::unique_ptr<DefaultAttributesProcessor> default_attributes_processor{
      new DefaultAttributesProcessor{}};
  opentelemetry::sdk::metrics::SyncMetricStorage storage(
      instr_desc, AggregationType::kBase2ExponentialHistogram, default_attributes_processor.get(),
#ifdef ENABLE_METRICS_EXEMPLAR_PREVIEW
      ExemplarFilterType::kAlwaysOff, ExemplarReservoir::GetNoExemplarReservoir(),
#endif
      nullptr);

  storage.RecordDouble(10.0,
                       KeyValueIterableView<std::map<std::string, std::string>>(attributes_get),
                       opentelemetry::context::Context{});
  expected_total_get_requests += 10;

  storage.RecordDouble(30.0,
                       KeyValueIterableView<std::map<std::string, std::string>>(attributes_put),
                       opentelemetry::context::Context{});
  expected_total_put_requests += 30;

  storage.RecordDouble(20.0,
                       KeyValueIterableView<std::map<std::string, std::string>>(attributes_get),
                       opentelemetry::context::Context{});
  expected_total_get_requests += 20;

  storage.RecordDouble(40.0,
                       KeyValueIterableView<std::map<std::string, std::string>>(attributes_put),
                       opentelemetry::context::Context{});
  expected_total_put_requests += 40;

  std::shared_ptr<CollectorHandle> collector(new MockCollectorHandle(temporality));
  std::vector<std::shared_ptr<CollectorHandle>> collectors;
  collectors.push_back(collector);

  // Some computation here
  auto collection_ts      = std::chrono::system_clock::now();
  size_t count_attributes = 0;
  storage.Collect(
      collector.get(), collectors, sdk_start_ts, collection_ts, [&](const MetricData &metric_data) {
        for (const auto &data_attr : metric_data.point_data_attr_)
        {
          const auto &data =
              opentelemetry::nostd::get<Base2ExponentialHistogramPointData>(data_attr.point_data);
          if (opentelemetry::nostd::get<std::string>(
                  data_attr.attributes.find("RequestType")->second) == "GET")
          {
            EXPECT_EQ(data.sum_, expected_total_get_requests);
            EXPECT_EQ(data.count_, 2);
            EXPECT_EQ(data.min_, 10);
            EXPECT_EQ(data.max_, 20);
            EXPECT_EQ(data.negative_buckets_->Empty(), true);
            auto start_index = data.positive_buckets_->StartIndex();
            auto end_index   = data.positive_buckets_->EndIndex();
            EXPECT_EQ(data.positive_buckets_->Get(start_index), 1);
            EXPECT_EQ(data.positive_buckets_->Get(end_index), 1);
            count_attributes++;
          }
          else if (opentelemetry::nostd::get<std::string>(
                       data_attr.attributes.find("RequestType")->second) == "PUT")
          {
            EXPECT_EQ(data.sum_, expected_total_put_requests);
            EXPECT_EQ(data.count_, 2);
            EXPECT_EQ(data.min_, 30);
            EXPECT_EQ(data.max_, 40);
            auto start_index = data.positive_buckets_->StartIndex();
            auto end_index   = data.positive_buckets_->EndIndex();
            EXPECT_EQ(data.positive_buckets_->Get(start_index), 1);
            EXPECT_EQ(data.positive_buckets_->Get(end_index), 1);
            EXPECT_EQ(data.negative_buckets_->Empty(), true);
            count_attributes++;
          }
        }
        return true;
      });
  EXPECT_EQ(count_attributes, 2);  // GET and PUT

  // In case of delta temporarily, subsequent collection would contain new data points, so resetting
  // the counts
  if (temporality == AggregationTemporality::kDelta)
  {
    expected_total_get_requests = 0;
    expected_total_put_requests = 0;
  }

  // collect one more time.
  collection_ts    = std::chrono::system_clock::now();
  count_attributes = 0;
  storage.Collect(
      collector.get(), collectors, sdk_start_ts, collection_ts, [&](const MetricData &metric_data) {
        if (temporality == AggregationTemporality::kCumulative)
        {
          EXPECT_EQ(metric_data.start_ts, sdk_start_ts);
        }
        for (const auto &data_attr : metric_data.point_data_attr_)
        {
          const auto &data =
              opentelemetry::nostd::get<Base2ExponentialHistogramPointData>(data_attr.point_data);
          if (opentelemetry::nostd::get<std::string>(
                  data_attr.attributes.find("RequestType")->second) == "GET")
          {
            count_attributes++;
            EXPECT_EQ(data.sum_, expected_total_get_requests);
          }
          else if (opentelemetry::nostd::get<std::string>(
                       data_attr.attributes.find("RequestType")->second) == "PUT")
          {
            count_attributes++;
            EXPECT_EQ(data.sum_, expected_total_put_requests);
          }
        }
        return true;
      });
  if (temporality == AggregationTemporality::kCumulative)
  {
    EXPECT_EQ(count_attributes, 2);  // GET AND PUT
  }

  storage.RecordDouble(50.0,
                       KeyValueIterableView<std::map<std::string, std::string>>(attributes_get),
                       opentelemetry::context::Context{});
  expected_total_get_requests += 50;
  storage.RecordDouble(40.0,
                       KeyValueIterableView<std::map<std::string, std::string>>(attributes_put),
                       opentelemetry::context::Context{});
  expected_total_put_requests += 40;

  collection_ts    = std::chrono::system_clock::now();
  count_attributes = 0;
  storage.Collect(
      collector.get(), collectors, sdk_start_ts, collection_ts, [&](const MetricData &metric_data) {
        for (const auto &data_attr : metric_data.point_data_attr_)
        {
          auto &data =
              opentelemetry::nostd::get<Base2ExponentialHistogramPointData>(data_attr.point_data);
          if (opentelemetry::nostd::get<std::string>(
                  data_attr.attributes.find("RequestType")->second) == "GET")
          {
            EXPECT_EQ(data.sum_, expected_total_get_requests);
            count_attributes++;
            auto start_index = data.positive_buckets_->StartIndex();
            auto end_index   = data.positive_buckets_->EndIndex();
            if (temporality == AggregationTemporality::kCumulative)
            {
              EXPECT_EQ(data.count_, 3);
              EXPECT_EQ(data.min_, 10);
              EXPECT_EQ(data.max_, 50);
              uint64_t count = 0;
              for (auto i = start_index; i <= end_index; ++i)
              {
                count += data.positive_buckets_->Get(i);
              }
              EXPECT_EQ(count, 3);
            }
            if (temporality == AggregationTemporality::kDelta)
            {
              EXPECT_EQ(data.count_, 1);
              EXPECT_EQ(data.min_, 50);
              EXPECT_EQ(data.max_, 50);
              EXPECT_EQ(data.positive_buckets_->Get(start_index), 1);
              EXPECT_EQ(end_index, start_index);
            }
          }
          else if (opentelemetry::nostd::get<std::string>(
                       data_attr.attributes.find("RequestType")->second) == "PUT")
          {
            EXPECT_EQ(data.sum_, expected_total_put_requests);
            count_attributes++;
            auto start_index = data.positive_buckets_->StartIndex();
            auto end_index   = data.positive_buckets_->EndIndex();
            if (temporality == AggregationTemporality::kCumulative)
            {
              EXPECT_EQ(data.count_, 3);
              EXPECT_EQ(data.min_, 30);
              EXPECT_EQ(data.max_, 40);
              uint64_t count = 0;
              for (auto i = start_index; i <= end_index; ++i)
              {
                count += data.positive_buckets_->Get(i);
              }
              EXPECT_EQ(count, 3);
            }
            if (temporality == AggregationTemporality::kDelta)
            {
              EXPECT_EQ(data.count_, 1);
              EXPECT_EQ(data.min_, 40);
              EXPECT_EQ(data.max_, 40);
              EXPECT_EQ(data.positive_buckets_->Get(start_index), 1);
              EXPECT_EQ(end_index, start_index);
            }
          }
        }
        return true;
      });

  EXPECT_EQ(count_attributes, 2);  // GET and PUT
}

INSTANTIATE_TEST_SUITE_P(WritableMetricStorageHistogramTestBase2ExponentialDouble,
                         WritableMetricStorageHistogramTestFixture,
                         ::testing::Values(AggregationTemporality::kCumulative,
                                           AggregationTemporality::kDelta));
