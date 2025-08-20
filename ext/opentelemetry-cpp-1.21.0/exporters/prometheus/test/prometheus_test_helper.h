// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/version.h"

namespace metric_sdk      = opentelemetry::sdk::metrics;
namespace nostd           = opentelemetry::nostd;
namespace exportermetrics = opentelemetry::exporter::metrics;

namespace
{

using opentelemetry::sdk::instrumentationscope::InstrumentationScope;
using opentelemetry::sdk::resource::Resource;
using opentelemetry::sdk::resource::ResourceAttributes;

struct TestDataPoints
{
  Resource resource = Resource::Create(ResourceAttributes{});
  nostd::unique_ptr<InstrumentationScope> instrumentation_scope =
      InstrumentationScope::Create("library_name", "1.2.0");

  /**
   * Helper function to create ResourceMetrics
   */
  inline metric_sdk::ResourceMetrics CreateSumPointData()
  {
    metric_sdk::SumPointData sum_point_data{};
    sum_point_data.value_ = 10.0;
    metric_sdk::SumPointData sum_point_data2{};
    sum_point_data2.value_ = 20.0;
    metric_sdk::ResourceMetrics data;
    data.resource_ = &resource;
    metric_sdk::MetricData metric_data{
        metric_sdk::InstrumentDescriptor{"library_name", "description", "unit",
                                         metric_sdk::InstrumentType::kCounter,
                                         metric_sdk::InstrumentValueType::kDouble},
        metric_sdk::AggregationTemporality::kDelta, opentelemetry::common::SystemTimestamp{},
        opentelemetry::common::SystemTimestamp{},
        std::vector<metric_sdk::PointDataAttributes>{
            {metric_sdk::PointAttributes{{"a1", "b1"}}, sum_point_data},
            {metric_sdk::PointAttributes{{"a2", "b2"}}, sum_point_data2}}};
    data.scope_metric_data_ = std::vector<metric_sdk::ScopeMetrics>{
        {instrumentation_scope.get(), std::vector<metric_sdk::MetricData>{metric_data}}};
    return data;
  }

  inline metric_sdk::ResourceMetrics CreateHistogramPointData()
  {
    metric_sdk::HistogramPointData histogram_point_data{};
    histogram_point_data.boundaries_ = {10.1, 20.2, 30.2};
    histogram_point_data.count_      = 3;
    histogram_point_data.counts_     = {200, 300, 400, 500};
    histogram_point_data.sum_        = 900.5;
    metric_sdk::HistogramPointData histogram_point_data2{};
    histogram_point_data2.boundaries_ = {10.0, 20.0, 30.0};
    histogram_point_data2.count_      = 3;
    histogram_point_data2.counts_     = {200, 300, 400, 500};
    histogram_point_data2.sum_        = static_cast<int64_t>(900);
    metric_sdk::ResourceMetrics data;
    data.resource_ = &resource;
    metric_sdk::MetricData metric_data{
        metric_sdk::InstrumentDescriptor{"library_name", "description", "unit",
                                         metric_sdk::InstrumentType::kHistogram,
                                         metric_sdk::InstrumentValueType::kDouble},
        metric_sdk::AggregationTemporality::kDelta, opentelemetry::common::SystemTimestamp{},
        opentelemetry::common::SystemTimestamp{},
        std::vector<metric_sdk::PointDataAttributes>{
            {metric_sdk::PointAttributes{{"a1", "b1"}}, histogram_point_data},
            {metric_sdk::PointAttributes{{"a2", "b2"}}, histogram_point_data2}}};
    data.scope_metric_data_ = std::vector<metric_sdk::ScopeMetrics>{
        {instrumentation_scope.get(), std::vector<metric_sdk::MetricData>{metric_data}}};
    return data;
  }

  inline metric_sdk::ResourceMetrics CreateLastValuePointData()
  {
    metric_sdk::ResourceMetrics data;
    data.resource_ = &resource;
    metric_sdk::LastValuePointData last_value_point_data{};
    last_value_point_data.value_              = 10.0;
    last_value_point_data.is_lastvalue_valid_ = true;
    last_value_point_data.sample_ts_          = opentelemetry::common::SystemTimestamp{};
    metric_sdk::LastValuePointData last_value_point_data2{};
    last_value_point_data2.value_              = static_cast<int64_t>(20);
    last_value_point_data2.is_lastvalue_valid_ = true;
    last_value_point_data2.sample_ts_          = opentelemetry::common::SystemTimestamp{};
    metric_sdk::MetricData metric_data{
        metric_sdk::InstrumentDescriptor{"library_name", "description", "unit",
                                         metric_sdk::InstrumentType::kCounter,
                                         metric_sdk::InstrumentValueType::kDouble},
        metric_sdk::AggregationTemporality::kDelta, opentelemetry::common::SystemTimestamp{},
        opentelemetry::common::SystemTimestamp{},
        std::vector<metric_sdk::PointDataAttributes>{
            {metric_sdk::PointAttributes{{"a1", "b1"}}, last_value_point_data},
            {metric_sdk::PointAttributes{{"a2", "b2"}}, last_value_point_data2}}};
    data.scope_metric_data_ = std::vector<metric_sdk::ScopeMetrics>{
        {instrumentation_scope.get(), std::vector<metric_sdk::MetricData>{metric_data}}};
    return data;
  }

  inline metric_sdk::ResourceMetrics CreateDropPointData()
  {
    metric_sdk::ResourceMetrics data;
    data.resource_ = &resource;
    metric_sdk::DropPointData drop_point_data{};
    metric_sdk::DropPointData drop_point_data2{};
    metric_sdk::MetricData metric_data{
        metric_sdk::InstrumentDescriptor{"library_name", "description", "unit",
                                         metric_sdk::InstrumentType::kCounter,
                                         metric_sdk::InstrumentValueType::kDouble},
        metric_sdk::AggregationTemporality::kDelta, opentelemetry::common::SystemTimestamp{},
        opentelemetry::common::SystemTimestamp{},
        std::vector<metric_sdk::PointDataAttributes>{
            {metric_sdk::PointAttributes{{"a1", "b1"}}, drop_point_data},
            {metric_sdk::PointAttributes{{"a2", "b2"}}, drop_point_data2}}};
    data.scope_metric_data_ = std::vector<metric_sdk::ScopeMetrics>{
        {instrumentation_scope.get(), std::vector<metric_sdk::MetricData>{metric_data}}};
    return data;
  }
};
}  // namespace

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace metrics
{
inline bool operator==(const metric_sdk::MetricData &lhs, const metric_sdk::MetricData &rhs)
{
  if (lhs.start_ts != rhs.start_ts)
  {
    return false;
  }
  if (lhs.end_ts != rhs.end_ts)
  {
    return false;
  }

  if (lhs.instrument_descriptor.description_ != rhs.instrument_descriptor.description_)
  {
    return false;
  }
  if (lhs.instrument_descriptor.name_ != rhs.instrument_descriptor.name_)
  {
    return false;
  }
  if (lhs.instrument_descriptor.type_ != rhs.instrument_descriptor.type_)
  {
    return false;
  }
  if (lhs.instrument_descriptor.unit_ != rhs.instrument_descriptor.unit_)
  {
    return false;
  }
  if (lhs.instrument_descriptor.value_type_ != rhs.instrument_descriptor.value_type_)
  {
    return false;
  }

  if (lhs.point_data_attr_.size() != rhs.point_data_attr_.size())
  {
    return false;
  }
  for (uint32_t idx = 0; idx < lhs.point_data_attr_.size(); ++idx)
  {
    if (lhs.point_data_attr_.at(idx).attributes != rhs.point_data_attr_.at(idx).attributes)
    {
      return false;
    }
    auto &lhs_point_data = lhs.point_data_attr_.at(idx).point_data;
    auto &rhs_point_data = rhs.point_data_attr_.at(idx).point_data;
    if (nostd::holds_alternative<metric_sdk::SumPointData>(lhs_point_data))
    {
      if (nostd::get<metric_sdk::SumPointData>(lhs_point_data).value_ !=
          nostd::get<metric_sdk::SumPointData>(rhs_point_data).value_)
      {
        return false;
      }
    }
    else if (nostd::holds_alternative<metric_sdk::DropPointData>(lhs_point_data))
    {
      if (!nostd::holds_alternative<metric_sdk::DropPointData>(rhs_point_data))
      {
        return false;
      }
    }
    else if (nostd::holds_alternative<metric_sdk::HistogramPointData>(lhs_point_data))
    {
      auto &lhs_histogram_d = nostd::get<metric_sdk::HistogramPointData>(lhs_point_data);
      auto &rhs_histogram_d = nostd::get<metric_sdk::HistogramPointData>(rhs_point_data);
      if (lhs_histogram_d.count_ != rhs_histogram_d.count_)
      {
        return false;
      }
      if (lhs_histogram_d.counts_ != rhs_histogram_d.counts_)
      {
        return false;
      }
      if (lhs_histogram_d.boundaries_ != rhs_histogram_d.boundaries_)
      {
        return false;
      }
      if (lhs_histogram_d.sum_ != rhs_histogram_d.sum_)
      {
        return false;
      }
    }
    else if (nostd::holds_alternative<metric_sdk::LastValuePointData>(lhs_point_data))
    {
      auto &lhs_last_value_d = nostd::get<metric_sdk::LastValuePointData>(lhs_point_data);
      auto &rhs_last_value_d = nostd::get<metric_sdk::LastValuePointData>(rhs_point_data);
      if (lhs_last_value_d.is_lastvalue_valid_ != rhs_last_value_d.is_lastvalue_valid_)
      {
        return false;
      }
      if (lhs_last_value_d.sample_ts_ != rhs_last_value_d.sample_ts_)
      {
        return false;
      }
      if (lhs_last_value_d.value_ != rhs_last_value_d.value_)
      {
        return false;
      }
    }
  }
  return true;
}
}  // namespace metrics
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
