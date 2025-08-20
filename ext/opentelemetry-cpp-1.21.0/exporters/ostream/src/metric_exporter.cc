// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/exporters/ostream/common_utils.h"
#include "opentelemetry/exporters/ostream/metric_exporter.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/sdk/common/attribute_utils.h"
#include "opentelemetry/sdk/common/exporter_utils.h"
#include "opentelemetry/sdk/common/global_log_handler.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/metrics/data/circular_buffer.h"
#include "opentelemetry/sdk/metrics/data/metric_data.h"
#include "opentelemetry/sdk/metrics/data/point_data.h"
#include "opentelemetry/sdk/metrics/export/metric_producer.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/version.h"

namespace
{
std::string timeToString(opentelemetry::common::SystemTimestamp time_stamp)
{
  std::time_t epoch_time = std::chrono::system_clock::to_time_t(time_stamp);

  struct tm *tm_ptr = nullptr;
#if defined(_MSC_VER)
  struct tm buf_tm;
  if (!gmtime_s(&buf_tm, &epoch_time))
  {
    tm_ptr = &buf_tm;
  }
#else
  tm_ptr = std::gmtime(&epoch_time);
#endif

  char buf[100];
  char *date_str = nullptr;
  if (tm_ptr == nullptr)
  {
    OTEL_INTERNAL_LOG_ERROR("[OStream Metric] gmtime failed for " << epoch_time);
  }
  else if (std::strftime(buf, sizeof(buf), "%c", tm_ptr) > 0)
  {
    date_str = buf;
  }
  else
  {
    OTEL_INTERNAL_LOG_ERROR("[OStream Metric] strftime failed for " << epoch_time);
  }

  return std::string{date_str};
}
}  // namespace

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace metrics
{

template <typename Container>
inline void printVec(std::ostream &os, Container &vec)
{
  using T = typename std::decay<decltype(*vec.begin())>::type;
  os << '[';
  if (vec.size() > 1)
  {
    std::copy(vec.begin(), vec.end(), std::ostream_iterator<T>(os, ", "));
  }
  os << ']';
}

OStreamMetricExporter::OStreamMetricExporter(
    std::ostream &sout,
    sdk::metrics::AggregationTemporality aggregation_temporality) noexcept
    : sout_(sout), aggregation_temporality_(aggregation_temporality)
{}

sdk::metrics::AggregationTemporality OStreamMetricExporter::GetAggregationTemporality(
    sdk::metrics::InstrumentType /* instrument_type */) const noexcept
{
  return aggregation_temporality_;
}

sdk::common::ExportResult OStreamMetricExporter::Export(
    const sdk::metrics::ResourceMetrics &data) noexcept
{
  if (isShutdown())
  {
    OTEL_INTERNAL_LOG_ERROR("[OStream Metric] Exporting "
                            << data.scope_metric_data_.size()
                            << " records(s) failed, exporter is shutdown");
    return sdk::common::ExportResult::kFailure;
  }

  for (auto &record : data.scope_metric_data_)
  {
    printInstrumentationInfoMetricData(record, data);
  }
  return sdk::common::ExportResult::kSuccess;
}

void OStreamMetricExporter::printAttributes(
    const std::map<std::string, sdk::common::OwnedAttributeValue> &map,
    const std::string &prefix)
{
  for (const auto &kv : map)
  {
    sout_ << prefix << kv.first << ": ";
    opentelemetry::exporter::ostream_common::print_value(kv.second, sout_);
  }
}

void OStreamMetricExporter::printResources(const opentelemetry::sdk::resource::Resource &resources)
{
  auto attributes = resources.GetAttributes();
  if (attributes.size())
  {
    // Convert unordered_map to map for printing so that iteration
    // order is guaranteed.
    std::map<std::string, sdk::common::OwnedAttributeValue> attr_map;
    for (auto &kv : attributes)
      attr_map[kv.first] = std::move(kv.second);
    printAttributes(attr_map, "\n\t");
  }
}

void OStreamMetricExporter::printInstrumentationInfoMetricData(
    const sdk::metrics::ScopeMetrics &info_metric,
    const sdk::metrics::ResourceMetrics &data)
{
  // sout_ is shared
  const std::lock_guard<std::mutex> serialize(serialize_lock_);
  sout_ << "{";
  sout_ << "\n  scope name\t: " << info_metric.scope_->GetName()
        << "\n  schema url\t: " << info_metric.scope_->GetSchemaURL()
        << "\n  version\t: " << info_metric.scope_->GetVersion();
  for (const auto &record : info_metric.metric_data_)
  {
    sout_ << "\n  start time\t: " << timeToString(record.start_ts)
          << "\n  end time\t: " << timeToString(record.end_ts)
          << "\n  instrument name\t: " << record.instrument_descriptor.name_
          << "\n  description\t: " << record.instrument_descriptor.description_
          << "\n  unit\t\t: " << record.instrument_descriptor.unit_;

    for (const auto &pd : record.point_data_attr_)
    {
      if (!nostd::holds_alternative<sdk::metrics::DropPointData>(pd.point_data))
      {
        printPointData(pd.point_data);
        printPointAttributes(pd.attributes);
      }
    }

    sout_ << "\n  resources\t:";
    printResources(*data.resource_);
  }
  sout_ << "\n}\n";
}

void OStreamMetricExporter::printPointData(const opentelemetry::sdk::metrics::PointType &point_data)
{
  if (nostd::holds_alternative<sdk::metrics::SumPointData>(point_data))
  {
    auto sum_point_data = nostd::get<sdk::metrics::SumPointData>(point_data);
    sout_ << "\n  type\t\t: SumPointData";
    sout_ << "\n  value\t\t: ";
    if (nostd::holds_alternative<double>(sum_point_data.value_))
    {
      sout_ << nostd::get<double>(sum_point_data.value_);
    }
    else if (nostd::holds_alternative<int64_t>(sum_point_data.value_))
    {
      sout_ << nostd::get<int64_t>(sum_point_data.value_);
    }
  }
  else if (nostd::holds_alternative<sdk::metrics::HistogramPointData>(point_data))
  {
    auto histogram_point_data = nostd::get<sdk::metrics::HistogramPointData>(point_data);
    sout_ << "\n  type     : HistogramPointData";
    sout_ << "\n  count     : " << histogram_point_data.count_;
    sout_ << "\n  sum     : ";
    if (nostd::holds_alternative<double>(histogram_point_data.sum_))
    {
      sout_ << nostd::get<double>(histogram_point_data.sum_);
    }
    else if (nostd::holds_alternative<int64_t>(histogram_point_data.sum_))
    {
      sout_ << nostd::get<int64_t>(histogram_point_data.sum_);
    }

    if (histogram_point_data.record_min_max_)
    {
      if (nostd::holds_alternative<int64_t>(histogram_point_data.min_))
      {
        sout_ << "\n  min     : " << nostd::get<int64_t>(histogram_point_data.min_);
      }
      else if (nostd::holds_alternative<double>(histogram_point_data.min_))
      {
        sout_ << "\n  min     : " << nostd::get<double>(histogram_point_data.min_);
      }
      if (nostd::holds_alternative<int64_t>(histogram_point_data.max_))
      {
        sout_ << "\n  max     : " << nostd::get<int64_t>(histogram_point_data.max_);
      }
      if (nostd::holds_alternative<double>(histogram_point_data.max_))
      {
        sout_ << "\n  max     : " << nostd::get<double>(histogram_point_data.max_);
      }
    }

    sout_ << "\n  buckets     : ";
    printVec(sout_, histogram_point_data.boundaries_);

    sout_ << "\n  counts     : ";
    printVec(sout_, histogram_point_data.counts_);
  }
  else if (nostd::holds_alternative<sdk::metrics::LastValuePointData>(point_data))
  {
    auto last_point_data = nostd::get<sdk::metrics::LastValuePointData>(point_data);
    sout_ << "\n  type     : LastValuePointData";
    sout_ << "\n  timestamp     : "
          << std::to_string(last_point_data.sample_ts_.time_since_epoch().count()) << std::boolalpha
          << "\n  valid     : " << last_point_data.is_lastvalue_valid_;
    sout_ << "\n  value     : ";
    if (nostd::holds_alternative<double>(last_point_data.value_))
    {
      sout_ << nostd::get<double>(last_point_data.value_);
    }
    else if (nostd::holds_alternative<int64_t>(last_point_data.value_))
    {
      sout_ << nostd::get<int64_t>(last_point_data.value_);
    }
  }
  else if (nostd::holds_alternative<sdk::metrics::Base2ExponentialHistogramPointData>(point_data))
  {
    auto histogram_point_data =
        nostd::get<sdk::metrics::Base2ExponentialHistogramPointData>(point_data);
    if (!histogram_point_data.positive_buckets_ && !histogram_point_data.negative_buckets_)
    {
      return;
    }
    sout_ << "\n  type: Base2ExponentialHistogramPointData";
    sout_ << "\n  count: " << histogram_point_data.count_;
    sout_ << "\n  sum: " << histogram_point_data.sum_;
    sout_ << "\n  zero_count: " << histogram_point_data.zero_count_;
    if (histogram_point_data.record_min_max_)
    {
      sout_ << "\n  min: " << histogram_point_data.min_;
      sout_ << "\n  max: " << histogram_point_data.max_;
    }
    sout_ << "\n  scale: " << histogram_point_data.scale_;
    sout_ << "\n  positive buckets:";
    if (!histogram_point_data.positive_buckets_->Empty())
    {
      for (auto i = histogram_point_data.positive_buckets_->StartIndex();
           i <= histogram_point_data.positive_buckets_->EndIndex(); ++i)
      {
        sout_ << "\n\t" << i << ": " << histogram_point_data.positive_buckets_->Get(i);
      }
    }
    sout_ << "\n  negative buckets:";
    if (!histogram_point_data.negative_buckets_->Empty())
    {
      for (auto i = histogram_point_data.negative_buckets_->StartIndex();
           i <= histogram_point_data.negative_buckets_->EndIndex(); ++i)
      {
        sout_ << "\n\t" << i << ": " << histogram_point_data.negative_buckets_->Get(i);
      }
    }
  }
}

void OStreamMetricExporter::printPointAttributes(
    const opentelemetry::sdk::metrics::PointAttributes &point_attributes)
{
  sout_ << "\n  attributes\t\t: ";
  for (const auto &kv : point_attributes)
  {
    sout_ << "\n\t" << kv.first << ": ";
    opentelemetry::exporter::ostream_common::print_value(kv.second, sout_);
  }
}

bool OStreamMetricExporter::ForceFlush(std::chrono::microseconds /* timeout */) noexcept
{
  const std::lock_guard<std::mutex> serialize(serialize_lock_);
  sout_.flush();
  return true;
}

bool OStreamMetricExporter::Shutdown(std::chrono::microseconds /* timeout */) noexcept
{
  is_shutdown_ = true;
  return true;
}

bool OStreamMetricExporter::isShutdown() const noexcept
{
  return is_shutdown_;
}

}  // namespace metrics
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
