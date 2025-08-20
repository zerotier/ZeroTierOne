// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <string>

#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/nostd/utility.h"
#include "opentelemetry/sdk/metrics/instruments.h"

#if OPENTELEMETRY_ABI_VERSION_NO >= 2
#  include <map>
#  include <memory>
#  include "common.h"

#  include "opentelemetry/common/key_value_iterable_view.h"
#  include "opentelemetry/nostd/shared_ptr.h"
#  include "opentelemetry/sdk/metrics/data/point_data.h"
#  include "opentelemetry/sdk/metrics/state/sync_metric_storage.h"
#  include "opentelemetry/sdk/metrics/view/attributes_processor.h"
#endif

using namespace opentelemetry::sdk::metrics;
using namespace opentelemetry::common;
namespace nostd = opentelemetry::nostd;

class WritableMetricStorageTestFixture : public ::testing::TestWithParam<AggregationTemporality>
{};

TEST_P(WritableMetricStorageTestFixture, LongGaugeLastValueAggregation)
{
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
  AggregationTemporality temporality = GetParam();
  auto sdk_start_ts                  = std::chrono::system_clock::now();
  InstrumentDescriptor instr_desc    = {"name", "desc", "1unit", InstrumentType::kGauge,
                                        InstrumentValueType::kLong};
  std::map<std::string, std::string> attributes_roomA = {{"Room.id", "Rack A"}};
  std::map<std::string, std::string> attributes_roomB = {{"Room.id", "Rack B"}};

  std::unique_ptr<DefaultAttributesProcessor> default_attributes_processor{
      new DefaultAttributesProcessor{}};
  opentelemetry::sdk::metrics::SyncMetricStorage storage(
      instr_desc, AggregationType::kLastValue, default_attributes_processor.get(),
#  ifdef ENABLE_METRICS_EXEMPLAR_PREVIEW
      ExemplarFilterType::kAlwaysOff, ExemplarReservoir::GetNoExemplarReservoir(),
#  endif
      nullptr);

  int64_t bg_noise_level_1_roomA = 10;
  int64_t bg_noise_level_1_roomB = 20;

  storage.RecordLong(bg_noise_level_1_roomA,
                     KeyValueIterableView<std::map<std::string, std::string>>(attributes_roomA),
                     opentelemetry::context::Context{});
  storage.RecordLong(bg_noise_level_1_roomB,
                     KeyValueIterableView<std::map<std::string, std::string>>(attributes_roomB),
                     opentelemetry::context::Context{});

  int64_t bg_noise_level_2_roomA = 43;
  int64_t bg_noise_level_2_roomB = 25;

  storage.RecordLong(bg_noise_level_2_roomA,
                     KeyValueIterableView<std::map<std::string, std::string>>(attributes_roomA),
                     opentelemetry::context::Context{});
  storage.RecordLong(bg_noise_level_2_roomB,
                     KeyValueIterableView<std::map<std::string, std::string>>(attributes_roomB),
                     opentelemetry::context::Context{});

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
          auto lastvalue_data = opentelemetry::nostd::get<LastValuePointData>(data_attr.point_data);
          if (opentelemetry::nostd::get<std::string>(
                  data_attr.attributes.find("Room.id")->second) == "Rack A")
          {
            if (temporality == AggregationTemporality::kCumulative)
            {
              EXPECT_EQ(opentelemetry::nostd::get<int64_t>(lastvalue_data.value_),
                        bg_noise_level_2_roomA);
            }
            count_attributes++;
          }
          else if (opentelemetry::nostd::get<std::string>(
                       data_attr.attributes.find("Room.id")->second) == "Rack B")
          {
            if (temporality == AggregationTemporality::kCumulative)
            {
              EXPECT_EQ(opentelemetry::nostd::get<int64_t>(lastvalue_data.value_),
                        bg_noise_level_2_roomB);
            }
            count_attributes++;
          }
        }
        return true;
      });
  EXPECT_EQ(count_attributes, 2);  // RackA and RackB
#else
  EXPECT_TRUE(true);
#endif
}

INSTANTIATE_TEST_SUITE_P(WritableMetricStorageTestLong,
                         WritableMetricStorageTestFixture,
                         ::testing::Values(AggregationTemporality::kCumulative));

TEST_P(WritableMetricStorageTestFixture, DoubleGaugeLastValueAggregation)
{
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
  AggregationTemporality temporality = GetParam();
  auto sdk_start_ts                  = std::chrono::system_clock::now();
  InstrumentDescriptor instr_desc    = {"name", "desc", "1unit", InstrumentType::kGauge,
                                        InstrumentValueType::kDouble};
  std::map<std::string, std::string> attributes_roomA = {{"Room.id", "Rack A"}};
  std::map<std::string, std::string> attributes_roomB = {{"Room.id", "Rack B"}};

  std::unique_ptr<DefaultAttributesProcessor> default_attributes_processor{
      new DefaultAttributesProcessor{}};
  opentelemetry::sdk::metrics::SyncMetricStorage storage(
      instr_desc, AggregationType::kLastValue, default_attributes_processor.get(),
#  ifdef ENABLE_METRICS_EXEMPLAR_PREVIEW
      ExemplarFilterType::kAlwaysOff, ExemplarReservoir::GetNoExemplarReservoir(),
#  endif
      nullptr);

  double bg_noise_level_1_roomA = 4.3;
  double bg_noise_level_1_roomB = 2.5;

  storage.RecordDouble(bg_noise_level_1_roomA,
                       KeyValueIterableView<std::map<std::string, std::string>>(attributes_roomA),
                       opentelemetry::context::Context{});
  storage.RecordDouble(bg_noise_level_1_roomB,
                       KeyValueIterableView<std::map<std::string, std::string>>(attributes_roomB),
                       opentelemetry::context::Context{});

  double bg_noise_level_2_roomA = 10.5;
  double bg_noise_level_2_roomB = 20.5;

  storage.RecordDouble(bg_noise_level_2_roomA,
                       KeyValueIterableView<std::map<std::string, std::string>>(attributes_roomA),
                       opentelemetry::context::Context{});
  storage.RecordDouble(bg_noise_level_2_roomB,
                       KeyValueIterableView<std::map<std::string, std::string>>(attributes_roomB),
                       opentelemetry::context::Context{});

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
          auto lastvalue_data = opentelemetry::nostd::get<LastValuePointData>(data_attr.point_data);
          if (opentelemetry::nostd::get<std::string>(
                  data_attr.attributes.find("Room.id")->second) == "Rack A")
          {
            if (temporality == AggregationTemporality::kCumulative)
            {
              EXPECT_DOUBLE_EQ(opentelemetry::nostd::get<double>(lastvalue_data.value_),
                               bg_noise_level_2_roomA);
            }
            count_attributes++;
          }
          else if (opentelemetry::nostd::get<std::string>(
                       data_attr.attributes.find("Room.id")->second) == "Rack B")
          {
            if (temporality == AggregationTemporality::kCumulative)
            {
              EXPECT_DOUBLE_EQ(opentelemetry::nostd::get<double>(lastvalue_data.value_),
                               bg_noise_level_2_roomB);
            }
            count_attributes++;
          }
        }
        return true;
      });
  EXPECT_EQ(count_attributes, 2);  // RackA and RackB
#else
  EXPECT_TRUE(true);
#endif
}

INSTANTIATE_TEST_SUITE_P(WritableMetricStorageTestDouble,
                         WritableMetricStorageTestFixture,
                         ::testing::Values(AggregationTemporality::kCumulative));
