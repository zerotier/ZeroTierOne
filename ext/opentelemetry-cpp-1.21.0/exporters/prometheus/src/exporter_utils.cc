// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <prometheus/client_metric.h>
#include <prometheus/metric_family.h>
#include <prometheus/metric_type.h>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/exporters/prometheus/exporter_utils.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/sdk/common/attribute_utils.h"
#include "opentelemetry/sdk/common/global_log_handler.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/metrics/data/metric_data.h"
#include "opentelemetry/sdk/metrics/data/point_data.h"
#include "opentelemetry/sdk/metrics/export/metric_producer.h"
#include "opentelemetry/sdk/metrics/instruments.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/version.h"

namespace prometheus_client = ::prometheus;
namespace metric_sdk        = opentelemetry::sdk::metrics;

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace metrics
{
namespace
{

static constexpr const char *kScopeNameKey    = "otel_scope_name";
static constexpr const char *kScopeVersionKey = "otel_scope_version";

/**
 * Sanitize the given metric name by replacing invalid characters with _,
 * ensuring that multiple consecutive _ characters are collapsed to a single _.
 *
 * @param valid a callable with the signature `(int pos, char ch) -> bool` that
 *        returns whether `ch` is valid at position `pos` in the string
 * @param name the string to sanitize
 */
template <typename T>
inline std::string Sanitize(std::string name, const T &valid)
{
  static_assert(std::is_convertible<T, std::function<bool(int, char)>>::value,
                "valid should be a callable with the signature "
                "(int, char) -> bool");

  constexpr const auto replacement     = '_';
  constexpr const auto replacement_dup = '=';

  bool has_dup = false;
  for (int i = 0; i < static_cast<int>(name.size()); ++i)
  {
    if (valid(i, name[i]) && name[i] != replacement)
    {
      continue;
    }
    if (i > 0 && (name[i - 1] == replacement || name[i - 1] == replacement_dup))
    {
      has_dup = true;
      name[i] = replacement_dup;
    }
    else
    {
      name[i] = replacement;
    }
  }
  if (has_dup)
  {
    auto end = std::remove(name.begin(), name.end(), replacement_dup);
    return std::string{name.begin(), end};
  }
  return name;
}

/**
 * Sanitize the given metric label key according to Prometheus rule.
 * Prometheus metric label keys are required to match the following regex:
 *   [a-zA-Z_]([a-zA-Z0-9_])*
 * and multiple consecutive _ characters must be collapsed to a single _.
 */
std::string SanitizeLabel(std::string label_key)
{
  return Sanitize(std::move(label_key), [](int i, char c) {
    return (c >= 'a' && c <= 'z') ||  //
           (c >= 'A' && c <= 'Z') ||  //
           c == '_' ||                //
           (c >= '0' && c <= '9' && i > 0);
  });
}

}  // namespace

/**
 * Helper function to convert OpenTelemetry metrics data collection
 * to Prometheus metrics data collection
 *
 * @param data a collection of metrics in OpenTelemetry
 * @return a collection of translated metrics that is acceptable by Prometheus
 */
std::vector<prometheus_client::MetricFamily> PrometheusExporterUtils::TranslateToPrometheus(
    const sdk::metrics::ResourceMetrics &data,
    bool populate_target_info,
    bool without_otel_scope,
    bool without_units,
    bool without_type_suffix)
{

  // initialize output vector
  std::size_t reserve_size = 1;
  for (const auto &instrumentation_info : data.scope_metric_data_)
  {
    reserve_size += instrumentation_info.metric_data_.size();
  }

  std::vector<prometheus_client::MetricFamily> output;
  output.reserve(reserve_size);
  if (data.scope_metric_data_.empty())
  {
    return output;
  }
  // Append target_info as the first metric
  if (populate_target_info && !data.scope_metric_data_.empty())
  {
    SetTarget(data,
              data.scope_metric_data_.begin()->metric_data_.begin()->end_ts.time_since_epoch(),
              without_otel_scope ? nullptr : (*data.scope_metric_data_.begin()).scope_, &output);
  }

  for (const auto &instrumentation_info : data.scope_metric_data_)
  {
    for (const auto &metric_data : instrumentation_info.metric_data_)
    {
      auto origin_name = metric_data.instrument_descriptor.name_;
      auto unit        = metric_data.instrument_descriptor.unit_;
      prometheus_client::MetricFamily metric_family;
      metric_family.help = metric_data.instrument_descriptor.description_;
      auto time          = metric_data.end_ts.time_since_epoch();
      auto front         = metric_data.point_data_attr_.front();
      auto kind          = getAggregationType(front.point_data);
      bool is_monotonic  = true;
      if (kind == sdk::metrics::AggregationType::kSum)
      {
        is_monotonic = nostd::get<sdk::metrics::SumPointData>(front.point_data).is_monotonic_;
      }
      const prometheus_client::MetricType type = TranslateType(kind, is_monotonic);
      metric_family.name = MapToPrometheusName(metric_data.instrument_descriptor.name_,
                                               metric_data.instrument_descriptor.unit_, type,
                                               without_units, without_type_suffix);
      metric_family.type = type;
      const opentelemetry::sdk::instrumentationscope::InstrumentationScope *scope =
          without_otel_scope ? nullptr : instrumentation_info.scope_;

      for (const auto &point_data_attr : metric_data.point_data_attr_)
      {
        if (type == prometheus_client::MetricType::Histogram)  // Histogram
        {
          auto histogram_point_data =
              nostd::get<sdk::metrics::HistogramPointData>(point_data_attr.point_data);
          auto boundaries = histogram_point_data.boundaries_;
          auto counts     = histogram_point_data.counts_;
          double sum      = 0.0;
          if (nostd::holds_alternative<double>(histogram_point_data.sum_))
          {
            sum = nostd::get<double>(histogram_point_data.sum_);
          }
          else
          {
            sum = static_cast<double>(nostd::get<int64_t>(histogram_point_data.sum_));
          }
          SetData(std::vector<double>{sum, static_cast<double>(histogram_point_data.count_)},
                  boundaries, counts, point_data_attr.attributes, scope, time, &metric_family,
                  data.resource_);
        }
        else if (type == prometheus_client::MetricType::Gauge)
        {
          if (nostd::holds_alternative<sdk::metrics::LastValuePointData>(
                  point_data_attr.point_data))
          {
            auto last_value_point_data =
                nostd::get<sdk::metrics::LastValuePointData>(point_data_attr.point_data);
            std::vector<metric_sdk::ValueType> values{last_value_point_data.value_};
            SetData(values, point_data_attr.attributes, scope, type, time, &metric_family,
                    data.resource_);
          }
          else if (nostd::holds_alternative<sdk::metrics::SumPointData>(point_data_attr.point_data))
          {
            auto sum_point_data =
                nostd::get<sdk::metrics::SumPointData>(point_data_attr.point_data);
            std::vector<metric_sdk::ValueType> values{sum_point_data.value_};
            SetData(values, point_data_attr.attributes, scope, type, time, &metric_family,
                    data.resource_);
          }
          else
          {
            OTEL_INTERNAL_LOG_WARN(
                "[Prometheus Exporter] TranslateToPrometheus - "
                "invalid LastValuePointData type");
          }
        }
        else  // Counter, Untyped
        {
          if (nostd::holds_alternative<sdk::metrics::SumPointData>(point_data_attr.point_data))
          {
            auto sum_point_data =
                nostd::get<sdk::metrics::SumPointData>(point_data_attr.point_data);
            std::vector<metric_sdk::ValueType> values{sum_point_data.value_};
            SetData(values, point_data_attr.attributes, scope, type, time, &metric_family,
                    data.resource_);
          }
          else
          {
            OTEL_INTERNAL_LOG_WARN(
                "[Prometheus Exporter] TranslateToPrometheus - "
                "invalid SumPointData type");
          }
        }
      }
      output.emplace_back(metric_family);
    }
  }
  return output;
}

void PrometheusExporterUtils::AddPrometheusLabel(
    std::string name,
    std::string value,
    std::vector<::prometheus::ClientMetric::Label> *labels)
{
  prometheus_client::ClientMetric::Label prometheus_label;
  prometheus_label.name  = std::move(name);
  prometheus_label.value = std::move(value);
  labels->emplace_back(std::move(prometheus_label));
}

/**
 * Sanitize the given metric name or label according to Prometheus rule.
 *
 * This function is needed because names in OpenTelemetry can contain
 * alphanumeric characters, '_', '.', and '-', whereas in Prometheus the
 * name should only contain alphanumeric characters and '_'.
 */
std::string PrometheusExporterUtils::SanitizeNames(std::string name)
{
  constexpr const auto replacement     = '_';
  constexpr const auto replacement_dup = '=';

  auto valid = [](int i, char c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == ':' ||
        (c >= '0' && c <= '9' && i > 0))
    {
      return true;
    }
    return false;
  };

  bool has_dup = false;
  for (int i = 0; i < static_cast<int>(name.size()); ++i)
  {
    if (valid(i, name[i]))
    {
      continue;
    }
    if (i > 0 && (name[i - 1] == replacement || name[i - 1] == replacement_dup))
    {
      has_dup = true;
      name[i] = replacement_dup;
    }
    else
    {
      name[i] = replacement;
    }
  }
  if (has_dup)
  {
    auto end = std::remove(name.begin(), name.end(), replacement_dup);
    return std::string{name.begin(), end};
  }
  return name;
}

#if OPENTELEMETRY_HAVE_WORKING_REGEX
const std::regex INVALID_CHARACTERS_PATTERN("[^a-zA-Z0-9]");
const std::regex CHARACTERS_BETWEEN_BRACES_PATTERN("\\{(.*?)\\}");
const std::regex SANITIZE_LEADING_UNDERSCORES("^_+");
const std::regex SANITIZE_TRAILING_UNDERSCORES("_+$");
const std::regex SANITIZE_CONSECUTIVE_UNDERSCORES("[_]{2,}");
#endif

std::string PrometheusExporterUtils::GetEquivalentPrometheusUnit(
    const std::string &raw_metric_unit_name)
{
  if (raw_metric_unit_name.empty())
  {
    return raw_metric_unit_name;
  }

  std::string converted_metric_unit_name = RemoveUnitPortionInBraces(raw_metric_unit_name);
  converted_metric_unit_name = ConvertRateExpressedToPrometheusUnit(converted_metric_unit_name);

  return CleanUpString(GetPrometheusUnit(converted_metric_unit_name));
}

std::string PrometheusExporterUtils::GetPrometheusUnit(const std::string &unit_abbreviation)
{
  static std::unordered_map<std::string, std::string> units{// Time
                                                            {"d", "days"},
                                                            {"h", "hours"},
                                                            {"min", "minutes"},
                                                            {"s", "seconds"},
                                                            {"ms", "milliseconds"},
                                                            {"us", "microseconds"},
                                                            {"ns", "nanoseconds"},
                                                            // Bytes
                                                            {"By", "bytes"},
                                                            {"KiBy", "kibibytes"},
                                                            {"MiBy", "mebibytes"},
                                                            {"GiBy", "gibibytes"},
                                                            {"TiBy", "tibibytes"},
                                                            {"KBy", "kilobytes"},
                                                            {"MBy", "megabytes"},
                                                            {"GBy", "gigabytes"},
                                                            {"TBy", "terabytes"},
                                                            {"By", "bytes"},
                                                            {"KBy", "kilobytes"},
                                                            {"MBy", "megabytes"},
                                                            {"GBy", "gigabytes"},
                                                            {"TBy", "terabytes"},
                                                            // SI
                                                            {"m", "meters"},
                                                            {"V", "volts"},
                                                            {"A", "amperes"},
                                                            {"J", "joules"},
                                                            {"W", "watts"},
                                                            {"g", "grams"},
                                                            // Misc
                                                            {"Cel", "celsius"},
                                                            {"Hz", "hertz"},
                                                            {"1", ""},
                                                            {"%", "percent"}};
  auto res_it = units.find(unit_abbreviation);
  if (res_it == units.end())
  {
    return unit_abbreviation;
  }
  return res_it->second;
}

std::string PrometheusExporterUtils::GetPrometheusPerUnit(const std::string &per_unit_abbreviation)
{
  static std::unordered_map<std::string, std::string> per_units{
      {"s", "second"}, {"m", "minute"}, {"h", "hour"}, {"d", "day"},
      {"w", "week"},   {"mo", "month"}, {"y", "year"}};
  auto res_it = per_units.find(per_unit_abbreviation);
  if (res_it == per_units.end())
  {
    return per_unit_abbreviation;
  }
  return res_it->second;
}

std::string PrometheusExporterUtils::RemoveUnitPortionInBraces(const std::string &unit)
{
#if OPENTELEMETRY_HAVE_WORKING_REGEX
  return std::regex_replace(unit, CHARACTERS_BETWEEN_BRACES_PATTERN, "");
#else
  bool in_braces = false;
  std::string cleaned_unit;
  cleaned_unit.reserve(unit.size());
  for (auto c : unit)
  {
    if (in_braces)
    {
      if (c == '}')
      {
        in_braces = false;
      }
    }
    else if (c == '{')
    {
      in_braces = true;
    }
    else
    {
      cleaned_unit += c;
    }
  }
  return cleaned_unit;
#endif
}

std::string PrometheusExporterUtils::ConvertRateExpressedToPrometheusUnit(
    const std::string &rate_expressed_unit)
{
  size_t pos = rate_expressed_unit.find('/');
  if (pos == std::string::npos)
  {
    return rate_expressed_unit;
  }

  std::vector<std::string> rate_entities;
  rate_entities.push_back(rate_expressed_unit.substr(0, pos));
  rate_entities.push_back(rate_expressed_unit.substr(pos + 1));

  if (rate_entities[1].empty())
  {
    return rate_expressed_unit;
  }

  std::string prometheus_unit     = GetPrometheusUnit(rate_entities[0]);
  std::string prometheus_per_unit = GetPrometheusPerUnit(rate_entities[1]);

  return prometheus_unit + "_per_" + prometheus_per_unit;
}

std::string PrometheusExporterUtils::CleanUpString(const std::string &str)
{
#if OPENTELEMETRY_HAVE_WORKING_REGEX
  std::string cleaned_string = std::regex_replace(str, INVALID_CHARACTERS_PATTERN, "_");
  cleaned_string = std::regex_replace(cleaned_string, SANITIZE_CONSECUTIVE_UNDERSCORES, "_");
  cleaned_string = std::regex_replace(cleaned_string, SANITIZE_TRAILING_UNDERSCORES, "");
  cleaned_string = std::regex_replace(cleaned_string, SANITIZE_LEADING_UNDERSCORES, "");
  return cleaned_string;
#else
  std::string cleaned_string = str;
  if (cleaned_string.empty())
  {
    return cleaned_string;
  }
  std::transform(cleaned_string.begin(), cleaned_string.end(), cleaned_string.begin(),
                 [](const char c) {
                   if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
                   {
                     return c;
                   }
                   return '_';
                 });

  std::string::size_type trim_start = 0;
  std::string::size_type trim_end   = 0;
  bool previous_underscore          = false;
  for (std::string::size_type i = 0; i < cleaned_string.size(); ++i)
  {
    if (cleaned_string[i] == '_')
    {
      if (previous_underscore)
      {
        continue;
      }

      previous_underscore = true;
    }
    else
    {
      previous_underscore = false;
    }

    if (trim_end != i)
    {
      cleaned_string[trim_end] = cleaned_string[i];
    }
    ++trim_end;
  }

  while (trim_end > 0 && cleaned_string[trim_end - 1] == '_')
  {
    --trim_end;
  }
  while (trim_start < trim_end && cleaned_string[trim_start] == '_')
  {
    ++trim_start;
  }

  // All characters are underscore
  if (trim_start >= trim_end)
  {
    return "_";
  }
  if (0 != trim_start || cleaned_string.size() != trim_end)
  {
    return cleaned_string.substr(trim_start, trim_end - trim_start);
  }

  return cleaned_string;
#endif
}

std::string PrometheusExporterUtils::MapToPrometheusName(
    const std::string &name,
    const std::string &unit,
    prometheus_client::MetricType prometheus_type,
    bool without_units,
    bool without_type_suffix)
{
  auto sanitized_name = SanitizeNames(name);
  // append unit suffixes
  if (!without_units)
  {
    std::string prometheus_equivalent_unit = GetEquivalentPrometheusUnit(unit);
    // Append prometheus unit if not null or empty.
    if (!prometheus_equivalent_unit.empty() &&
        sanitized_name.find(prometheus_equivalent_unit) == std::string::npos)
    {
      sanitized_name += "_" + prometheus_equivalent_unit;
    }
    // Special case - gauge
    if (unit == "1" && prometheus_type == prometheus_client::MetricType::Gauge &&
        sanitized_name.find("ratio") == std::string::npos)
    {
      // this is replacing the unit name
      sanitized_name += "_ratio";
    }
  }

  // append type suffixes
  if (!without_type_suffix)
  {
    // Special case - counter
    if (prometheus_type == prometheus_client::MetricType::Counter)
    {
      auto t_pos           = sanitized_name.rfind("_total");
      bool ends_with_total = t_pos == sanitized_name.size() - 6;
      if (!ends_with_total)
      {
        sanitized_name += "_total";
      }
    }
  }

  return CleanUpString(SanitizeNames(sanitized_name));
}

metric_sdk::AggregationType PrometheusExporterUtils::getAggregationType(
    const metric_sdk::PointType &point_type)
{

  if (nostd::holds_alternative<sdk::metrics::SumPointData>(point_type))
  {
    return metric_sdk::AggregationType::kSum;
  }
  else if (nostd::holds_alternative<sdk::metrics::DropPointData>(point_type))
  {
    return metric_sdk::AggregationType::kDrop;
  }
  else if (nostd::holds_alternative<sdk::metrics::HistogramPointData>(point_type))
  {
    return metric_sdk::AggregationType::kHistogram;
  }
  else if (nostd::holds_alternative<sdk::metrics::Base2ExponentialHistogramPointData>(point_type))
  {
    return metric_sdk::AggregationType::kBase2ExponentialHistogram;
  }
  else if (nostd::holds_alternative<sdk::metrics::LastValuePointData>(point_type))
  {
    return metric_sdk::AggregationType::kLastValue;
  }
  return metric_sdk::AggregationType::kDefault;
}

/**
 * Translate the OTel metric type to Prometheus metric type
 */
prometheus_client::MetricType PrometheusExporterUtils::TranslateType(
    metric_sdk::AggregationType kind,
    bool is_monotonic)
{
  switch (kind)
  {
    case metric_sdk::AggregationType::kSum:
      if (!is_monotonic)
      {
        return prometheus_client::MetricType::Gauge;
      }
      else
      {
        return prometheus_client::MetricType::Counter;
      }
      break;
    case metric_sdk::AggregationType::kHistogram:
      return prometheus_client::MetricType::Histogram;
      break;
    case metric_sdk::AggregationType::kLastValue:
      return prometheus_client::MetricType::Gauge;
      break;
    default:
      return prometheus_client::MetricType::Untyped;
  }
}

void PrometheusExporterUtils::SetTarget(
    const sdk::metrics::ResourceMetrics &data,
    std::chrono::nanoseconds time,
    const opentelemetry::sdk::instrumentationscope::InstrumentationScope *scope,
    std::vector<::prometheus::MetricFamily> *output)
{
  if (output == nullptr || data.resource_ == nullptr)
  {
    return;
  }

  prometheus_client::MetricFamily metric_family;
  metric_family.name = "target";
  metric_family.help = "Target metadata";
  metric_family.type = prometheus_client::MetricType::Info;
  metric_family.metric.emplace_back();

  prometheus_client::ClientMetric &metric = metric_family.metric.back();
  metric.info.value                       = 1.0;

  metric_sdk::PointAttributes empty_attributes;
  SetMetricBasic(metric, empty_attributes, time, scope, data.resource_);

  for (auto &label : data.resource_->GetAttributes())
  {
    AddPrometheusLabel(SanitizeNames(label.first), AttributeValueToString(label.second),
                       &metric.label);
  }

  output->emplace_back(std::move(metric_family));
}

/**
 * Set metric data for:
 * sum => Prometheus Counter
 */
template <typename T>
void PrometheusExporterUtils::SetData(
    std::vector<T> values,
    const metric_sdk::PointAttributes &labels,
    const opentelemetry::sdk::instrumentationscope::InstrumentationScope *scope,
    prometheus_client::MetricType type,
    std::chrono::nanoseconds time,
    prometheus_client::MetricFamily *metric_family,
    const opentelemetry::sdk::resource::Resource *resource)
{
  metric_family->metric.emplace_back();
  prometheus_client::ClientMetric &metric = metric_family->metric.back();
  SetMetricBasic(metric, labels, time, scope, resource);
  SetValue(values, type, &metric);
}

/**
 * Set metric data for:
 * Histogram => Prometheus Histogram
 */
template <typename T>
void PrometheusExporterUtils::SetData(
    std::vector<T> values,
    const std::vector<double> &boundaries,
    const std::vector<uint64_t> &counts,
    const metric_sdk::PointAttributes &labels,
    const opentelemetry::sdk::instrumentationscope::InstrumentationScope *scope,
    std::chrono::nanoseconds time,
    prometheus_client::MetricFamily *metric_family,
    const opentelemetry::sdk::resource::Resource *resource)
{
  metric_family->metric.emplace_back();
  prometheus_client::ClientMetric &metric = metric_family->metric.back();
  SetMetricBasic(metric, labels, time, scope, resource);
  SetValue(values, boundaries, counts, &metric);
}

/**
 * Set labels to metric data
 */
void PrometheusExporterUtils::SetMetricBasic(
    prometheus_client::ClientMetric &metric,
    const metric_sdk::PointAttributes &labels,
    std::chrono::nanoseconds time,
    const opentelemetry::sdk::instrumentationscope::InstrumentationScope *scope,
    const opentelemetry::sdk::resource::Resource *resource)
{
  metric.timestamp_ms = time.count() / 1000000;
  if (labels.empty() && nullptr == resource)
  {
    return;
  }

  // Concatenate values for keys that collide after sanitation.
  // Note that attribute keys are sorted, but sanitized keys can be out-of-order.
  // We could sort the sanitized keys again, but this seems too expensive to do
  // in this hot code path. Instead, we ignore out-of-order keys and emit a warning.
  metric.label.reserve(labels.size() + 2);
  std::string previous_key;
  for (auto const &label : labels)
  {
    auto sanitized = SanitizeLabel(label.first);
    int comparison = previous_key.compare(sanitized);
    if (metric.label.empty() || comparison < 0)  // new key
    {
      previous_key = sanitized;
      metric.label.push_back({sanitized, AttributeValueToString(label.second)});
    }
    else if (comparison == 0)  // key collision after sanitation
    {
      metric.label.back().value += ";" + AttributeValueToString(label.second);
    }
    else  // order inversion introduced by sanitation
    {
      OTEL_INTERNAL_LOG_WARN(
          "[Prometheus Exporter] SetMetricBase - "
          "the sort order of labels has changed because of sanitization: '"
          << label.first << "' became '" << sanitized << "' which is less than '" << previous_key
          << "'. Ignoring this label.");
    }
  }
  if (!scope)
  {
    return;
  }
  auto scope_name = scope->GetName();
  if (!scope_name.empty())
  {
    metric.label.emplace_back();
    metric.label.back().name  = kScopeNameKey;
    metric.label.back().value = std::move(scope_name);
  }
  auto scope_version = scope->GetVersion();
  if (!scope_version.empty())
  {
    metric.label.emplace_back();
    metric.label.back().name  = kScopeVersionKey;
    metric.label.back().value = std::move(scope_version);
  }
}

std::string PrometheusExporterUtils::AttributeValueToString(
    const opentelemetry::sdk::common::OwnedAttributeValue &value)
{
  std::string result;
  if (nostd::holds_alternative<bool>(value))
  {
    result = nostd::get<bool>(value) ? "true" : "false";
  }
  else if (nostd::holds_alternative<int>(value))
  {
    result = std::to_string(nostd::get<int>(value));
  }
  else if (nostd::holds_alternative<int64_t>(value))
  {
    result = std::to_string(nostd::get<int64_t>(value));
  }
  else if (nostd::holds_alternative<unsigned int>(value))
  {
    result = std::to_string(nostd::get<unsigned int>(value));
  }
  else if (nostd::holds_alternative<uint64_t>(value))
  {
    result = std::to_string(nostd::get<uint64_t>(value));
  }
  else if (nostd::holds_alternative<double>(value))
  {
    result = std::to_string(nostd::get<double>(value));
  }
  else if (nostd::holds_alternative<std::string>(value))
  {
    result = nostd::get<std::string>(value);
  }
  else
  {
    OTEL_INTERNAL_LOG_WARN(
        "[Prometheus Exporter] AttributeValueToString - "
        " Nested attributes not supported - ignored");
  }
  return result;
}

/**
 * Handle Counter.
 */
template <typename T>
void PrometheusExporterUtils::SetValue(const std::vector<T> &values,
                                       prometheus_client::MetricType type,
                                       prometheus_client::ClientMetric *metric)
{
  double value          = 0.0;
  const auto &value_var = values[0];
  if (nostd::holds_alternative<int64_t>(value_var))
  {
    value = static_cast<double>(nostd::get<int64_t>(value_var));
  }
  else
  {
    value = nostd::get<double>(value_var);
  }

  switch (type)
  {
    case prometheus_client::MetricType::Counter: {
      metric->counter.value = value;
      break;
    }
    case prometheus_client::MetricType::Gauge: {
      metric->gauge.value = value;
      break;
    }
    case prometheus_client::MetricType::Untyped: {
      metric->untyped.value = value;
      break;
    }
    default:
      return;
  }
}

/**
 * Handle Histogram
 */
template <typename T>
void PrometheusExporterUtils::SetValue(const std::vector<T> &values,
                                       const std::vector<double> &boundaries,
                                       const std::vector<uint64_t> &counts,
                                       prometheus_client::ClientMetric *metric)
{
  metric->histogram.sample_sum   = static_cast<double>(values[0]);
  metric->histogram.sample_count = static_cast<std::uint64_t>(values[1]);
  std::uint64_t cumulative       = 0;
  std::vector<prometheus_client::ClientMetric::Bucket> buckets;
  uint32_t idx = 0;
  for (const auto &boundary : boundaries)
  {
    prometheus_client::ClientMetric::Bucket bucket;
    cumulative += counts[idx];
    bucket.cumulative_count = cumulative;
    bucket.upper_bound      = boundary;
    buckets.emplace_back(bucket);
    ++idx;
  }
  prometheus_client::ClientMetric::Bucket bucket;
  cumulative += counts[idx];
  bucket.cumulative_count = cumulative;
  bucket.upper_bound      = std::numeric_limits<double>::infinity();
  buckets.emplace_back(bucket);
  metric->histogram.bucket = buckets;
}

}  // namespace metrics
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
