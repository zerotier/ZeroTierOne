// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#ifdef ENABLE_METRICS_EXEMPLAR_PREVIEW

#  include "opentelemetry/sdk/metrics/exemplar/reservoir_cell.h"
#  include <gtest/gtest.h>

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace metrics
{
class ReservoirCellTestPeer : public ::testing::Test
{
public:
  int64_t GetLongVal(const opentelemetry::sdk::metrics::ReservoirCell &reservoir_cell)
  {
    return nostd::get<int64_t>(reservoir_cell.value_);
  }

  double GetDoubleVal(const opentelemetry::sdk::metrics::ReservoirCell &reservoir_cell)
  {
    return nostd::get<double>(reservoir_cell.value_);
  }

  opentelemetry::common::SystemTimestamp GetRecordTime(
      const opentelemetry::sdk::metrics::ReservoirCell &reservoir_cell)
  {
    return reservoir_cell.record_time_;
  }

  void FilteredTest()
  {
    MetricAttributes original{{"k1", "v1"}, {"k2", "v2"}, {"k3", "v3"}};
    MetricAttributes metric_point{{"k2", "v2"}, {"k4", "v4"}};
    MetricAttributes expected_result{{"k1", "v1"}, {"k3", "v3"}};
    auto result = opentelemetry::sdk::metrics::ReservoirCell::filtered(original, metric_point);
    ASSERT_TRUE(result == expected_result);
  }
};

TEST_F(ReservoirCellTestPeer, recordMeasurement)
{
  opentelemetry::sdk::metrics::ReservoirCell reservoir_cell;
  reservoir_cell.RecordLongMeasurement(static_cast<int64_t>(1), MetricAttributes{},
                                       opentelemetry::context::Context{});
  ASSERT_TRUE(GetLongVal(reservoir_cell) == 1);

  reservoir_cell.RecordDoubleMeasurement(1.5, MetricAttributes{},
                                         opentelemetry::context::Context{});
  ASSERT_TRUE(GetDoubleVal(reservoir_cell) == 1.5);
}

TEST_F(ReservoirCellTestPeer, GetAndReset)
{
  opentelemetry::sdk::metrics::ReservoirCell reservoir_cell;
  auto double_data = reservoir_cell.GetAndResetDouble(MetricAttributes{});
  ASSERT_TRUE(GetRecordTime(reservoir_cell) == opentelemetry::common::SystemTimestamp{});
  ASSERT_TRUE(double_data == nullptr);

  auto long_data = reservoir_cell.GetAndResetLong(MetricAttributes{});
  ASSERT_TRUE(GetRecordTime(reservoir_cell) == opentelemetry::common::SystemTimestamp{});
  ASSERT_TRUE(long_data == nullptr);
}

TEST_F(ReservoirCellTestPeer, Filtered)
{
  FilteredTest();
}

}  // namespace metrics
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE

#endif  // ENABLE_METRICS_EXEMPLAR_PREVIEW
