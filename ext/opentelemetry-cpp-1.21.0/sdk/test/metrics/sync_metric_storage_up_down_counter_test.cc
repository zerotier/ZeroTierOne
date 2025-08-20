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
#include "opentelemetry/sdk/metrics/data/metric_data.h"
#include "opentelemetry/sdk/metrics/data/point_data.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/sdk/metrics/state/metric_collector.h"
#include "opentelemetry/sdk/metrics/state/sync_metric_storage.h"
#include "opentelemetry/sdk/metrics/view/attributes_processor.h"

#ifdef ENABLE_METRICS_EXEMPLAR_PREVIEW
#  include "opentelemetry/sdk/metrics/exemplar/filter_type.h"
#endif

using namespace opentelemetry::sdk::metrics;
using namespace opentelemetry::common;
using M         = std::map<std::string, std::string>;
namespace nostd = opentelemetry::nostd;

class WritableMetricStorageTestFixture : public ::testing::TestWithParam<AggregationTemporality>
{};

TEST_P(WritableMetricStorageTestFixture, LongUpDownCounterSumAggregation)
{
  AggregationTemporality temporality         = GetParam();
  auto sdk_start_ts                          = std::chrono::system_clock::now();
  int64_t expected_total_active_get_requests = 0;
  int64_t expected_total_active_put_requests = 0;
  InstrumentDescriptor instr_desc = {"name", "desc", "1unit", InstrumentType::kUpDownCounter,
                                     InstrumentValueType::kLong};
  std::map<std::string, std::string> attributes_get = {{"RequestType", "GET"}};
  std::map<std::string, std::string> attributes_put = {{"RequestType", "PUT"}};

  std::unique_ptr<DefaultAttributesProcessor> default_attributes_processor{
      new DefaultAttributesProcessor{}};
  opentelemetry::sdk::metrics::SyncMetricStorage storage(
      instr_desc, AggregationType::kSum, default_attributes_processor.get(),
#ifdef ENABLE_METRICS_EXEMPLAR_PREVIEW
      ExemplarFilterType::kAlwaysOff, ExemplarReservoir::GetNoExemplarReservoir(),
#endif
      nullptr);

  int64_t val1 = 10, val2 = 30, val3 = -5, val4 = -10;
  storage.RecordLong(val1, KeyValueIterableView<std::map<std::string, std::string>>(attributes_get),
                     opentelemetry::context::Context{});
  expected_total_active_get_requests += val1;

  storage.RecordLong(val2, KeyValueIterableView<std::map<std::string, std::string>>(attributes_put),
                     opentelemetry::context::Context{});
  expected_total_active_put_requests += val2;

  storage.RecordLong(val3, KeyValueIterableView<std::map<std::string, std::string>>(attributes_get),
                     opentelemetry::context::Context{});
  expected_total_active_get_requests += val3;

  storage.RecordLong(val4, KeyValueIterableView<std::map<std::string, std::string>>(attributes_put),
                     opentelemetry::context::Context{});
  expected_total_active_put_requests += val4;

  std::shared_ptr<CollectorHandle> collector(new MockCollectorHandle(temporality));
  std::vector<std::shared_ptr<CollectorHandle>> collectors;
  collectors.push_back(collector);

  // Some computation here
  auto collection_ts      = std::chrono::system_clock::now();
  size_t count_attributes = 0;
  storage.Collect(collector.get(), collectors, sdk_start_ts, collection_ts,
                  [&](const MetricData &data) {
                    for (auto data_attr : data.point_data_attr_)
                    {
                      auto sum_data = opentelemetry::nostd::get<SumPointData>(data_attr.point_data);
                      if (opentelemetry::nostd::get<std::string>(
                              data_attr.attributes.find("RequestType")->second) == "GET")
                      {
                        EXPECT_EQ(opentelemetry::nostd::get<int64_t>(sum_data.value_),
                                  expected_total_active_get_requests);
                        count_attributes++;
                      }
                      else if (opentelemetry::nostd::get<std::string>(
                                   data_attr.attributes.find("RequestType")->second) == "PUT")
                      {
                        EXPECT_EQ(opentelemetry::nostd::get<int64_t>(sum_data.value_),
                                  expected_total_active_put_requests);
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
    expected_total_active_get_requests = 0;
    expected_total_active_put_requests = 0;
  }

  // collect one more time.
  collection_ts    = std::chrono::system_clock::now();
  count_attributes = 0;
  storage.Collect(collector.get(), collectors, sdk_start_ts, collection_ts,
                  [&](const MetricData &data) {
                    if (temporality == AggregationTemporality::kCumulative)
                    {
                      EXPECT_EQ(data.start_ts, sdk_start_ts);
                    }
                    for (auto data_attr : data.point_data_attr_)
                    {
                      auto sum_data = opentelemetry::nostd::get<SumPointData>(data_attr.point_data);
                      if (opentelemetry::nostd::get<std::string>(
                              data_attr.attributes.find("RequestType")->second) == "GET")
                      {
                        count_attributes++;
                        EXPECT_EQ(opentelemetry::nostd::get<int64_t>(sum_data.value_),
                                  expected_total_active_get_requests);
                      }
                      else if (opentelemetry::nostd::get<std::string>(
                                   data_attr.attributes.find("RequestType")->second) == "PUT")
                      {
                        count_attributes++;
                        EXPECT_EQ(opentelemetry::nostd::get<int64_t>(sum_data.value_),
                                  expected_total_active_put_requests);
                      }
                    }
                    return true;
                  });
  if (temporality == AggregationTemporality::kCumulative)
  {
    EXPECT_EQ(count_attributes, 2);  // GET AND PUT
  }

  val1 = 50;
  val2 = 40;
  storage.RecordLong(val1, KeyValueIterableView<std::map<std::string, std::string>>(attributes_get),
                     opentelemetry::context::Context{});
  expected_total_active_get_requests += val1;
  storage.RecordLong(val2, KeyValueIterableView<std::map<std::string, std::string>>(attributes_put),
                     opentelemetry::context::Context{});
  expected_total_active_put_requests += val2;

  collection_ts    = std::chrono::system_clock::now();
  count_attributes = 0;
  storage.Collect(collector.get(), collectors, sdk_start_ts, collection_ts,
                  [&](const MetricData &data) {
                    for (auto data_attr : data.point_data_attr_)
                    {
                      auto sum_data = opentelemetry::nostd::get<SumPointData>(data_attr.point_data);
                      if (opentelemetry::nostd::get<std::string>(
                              data_attr.attributes.find("RequestType")->second) == "GET")
                      {
                        EXPECT_EQ(opentelemetry::nostd::get<int64_t>(sum_data.value_),
                                  expected_total_active_get_requests);
                        count_attributes++;
                      }
                      else if (opentelemetry::nostd::get<std::string>(
                                   data_attr.attributes.find("RequestType")->second) == "PUT")
                      {
                        EXPECT_EQ(opentelemetry::nostd::get<int64_t>(sum_data.value_),
                                  expected_total_active_put_requests);
                        count_attributes++;
                      }
                    }
                    return true;
                  });
  EXPECT_EQ(count_attributes, 2);  // GET and PUT
}

INSTANTIATE_TEST_SUITE_P(WritableMetricStorageTestLong,
                         WritableMetricStorageTestFixture,
                         ::testing::Values(AggregationTemporality::kCumulative,
                                           AggregationTemporality::kDelta));

TEST_P(WritableMetricStorageTestFixture, DoubleUpDownCounterSumAggregation)
{
  AggregationTemporality temporality        = GetParam();
  auto sdk_start_ts                         = std::chrono::system_clock::now();
  double expected_total_active_get_requests = 0;
  double expected_total_active_put_requests = 0;
  InstrumentDescriptor instr_desc = {"name", "desc", "1unit", InstrumentType::kUpDownCounter,
                                     InstrumentValueType::kDouble};
  std::map<std::string, std::string> attributes_get = {{"RequestType", "GET"}};
  std::map<std::string, std::string> attributes_put = {{"RequestType", "PUT"}};

  std::unique_ptr<DefaultAttributesProcessor> default_attributes_processor{
      new DefaultAttributesProcessor{}};
  opentelemetry::sdk::metrics::SyncMetricStorage storage(
      instr_desc, AggregationType::kSum, default_attributes_processor.get(),
#ifdef ENABLE_METRICS_EXEMPLAR_PREVIEW
      ExemplarFilterType::kAlwaysOff, ExemplarReservoir::GetNoExemplarReservoir(),
#endif
      nullptr);

  storage.RecordDouble(10.0,
                       KeyValueIterableView<std::map<std::string, std::string>>(attributes_get),
                       opentelemetry::context::Context{});
  expected_total_active_get_requests += 10;

  storage.RecordDouble(30.0,
                       KeyValueIterableView<std::map<std::string, std::string>>(attributes_put),
                       opentelemetry::context::Context{});
  expected_total_active_put_requests += 30;

  storage.RecordDouble(-5.0,
                       KeyValueIterableView<std::map<std::string, std::string>>(attributes_get),
                       opentelemetry::context::Context{});
  expected_total_active_get_requests -= 5;

  storage.RecordDouble(-10.0,
                       KeyValueIterableView<std::map<std::string, std::string>>(attributes_put),
                       opentelemetry::context::Context{});
  expected_total_active_put_requests -= 10;

  std::shared_ptr<CollectorHandle> collector(new MockCollectorHandle(temporality));
  std::vector<std::shared_ptr<CollectorHandle>> collectors;
  collectors.push_back(collector);

  // Some computation here
  auto collection_ts      = std::chrono::system_clock::now();
  size_t count_attributes = 0;
  storage.Collect(collector.get(), collectors, sdk_start_ts, collection_ts,
                  [&](const MetricData &data) {
                    for (auto data_attr : data.point_data_attr_)
                    {
                      auto sum_data = opentelemetry::nostd::get<SumPointData>(data_attr.point_data);
                      if (opentelemetry::nostd::get<std::string>(
                              data_attr.attributes.find("RequestType")->second) == "GET")
                      {
                        EXPECT_EQ(opentelemetry::nostd::get<double>(sum_data.value_),
                                  expected_total_active_get_requests);
                        count_attributes++;
                      }
                      else if (opentelemetry::nostd::get<std::string>(
                                   data_attr.attributes.find("RequestType")->second) == "PUT")
                      {
                        EXPECT_EQ(opentelemetry::nostd::get<double>(sum_data.value_),
                                  expected_total_active_put_requests);
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
    expected_total_active_get_requests = 0;
    expected_total_active_put_requests = 0;
  }

  // collect one more time.
  collection_ts    = std::chrono::system_clock::now();
  count_attributes = 0;
  storage.Collect(collector.get(), collectors, sdk_start_ts, collection_ts,
                  [&](const MetricData &data) {
                    if (temporality == AggregationTemporality::kCumulative)
                    {
                      EXPECT_EQ(data.start_ts, sdk_start_ts);
                    }
                    for (auto data_attr : data.point_data_attr_)
                    {
                      auto sum_data = opentelemetry::nostd::get<SumPointData>(data_attr.point_data);
                      if (opentelemetry::nostd::get<std::string>(
                              data_attr.attributes.find("RequestType")->second) == "GET")
                      {
                        count_attributes++;
                        EXPECT_EQ(opentelemetry::nostd::get<double>(sum_data.value_),
                                  expected_total_active_get_requests);
                      }
                      else if (opentelemetry::nostd::get<std::string>(
                                   data_attr.attributes.find("RequestType")->second) == "PUT")
                      {
                        count_attributes++;
                        EXPECT_EQ(opentelemetry::nostd::get<double>(sum_data.value_),
                                  expected_total_active_put_requests);
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
  expected_total_active_get_requests += 50;
  storage.RecordDouble(40.0,
                       KeyValueIterableView<std::map<std::string, std::string>>(attributes_put),
                       opentelemetry::context::Context{});
  expected_total_active_put_requests += 40;

  collection_ts    = std::chrono::system_clock::now();
  count_attributes = 0;
  storage.Collect(collector.get(), collectors, sdk_start_ts, collection_ts,
                  [&](const MetricData &data) {
                    for (auto data_attr : data.point_data_attr_)
                    {
                      auto sum_data = opentelemetry::nostd::get<SumPointData>(data_attr.point_data);
                      if (opentelemetry::nostd::get<std::string>(
                              data_attr.attributes.find("RequestType")->second) == "GET")
                      {
                        EXPECT_EQ(opentelemetry::nostd::get<double>(sum_data.value_),
                                  expected_total_active_get_requests);
                        count_attributes++;
                      }
                      else if (opentelemetry::nostd::get<std::string>(
                                   data_attr.attributes.find("RequestType")->second) == "PUT")
                      {
                        EXPECT_EQ(opentelemetry::nostd::get<double>(sum_data.value_),
                                  expected_total_active_put_requests);
                        count_attributes++;
                      }
                    }
                    return true;
                  });
  EXPECT_EQ(count_attributes, 2);  // GET and PUT
}
INSTANTIATE_TEST_SUITE_P(WritableMetricStorageTestDouble,
                         WritableMetricStorageTestFixture,
                         ::testing::Values(AggregationTemporality::kCumulative,
                                           AggregationTemporality::kDelta));
