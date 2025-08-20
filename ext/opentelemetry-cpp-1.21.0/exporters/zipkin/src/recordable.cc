// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <chrono>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>

#include "nlohmann/json.hpp"

#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/common/key_value_iterable.h"
#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/exporters/zipkin/recordable.h"
#include "opentelemetry/nostd/function_ref.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/semconv/service_attributes.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/span_id.h"
#include "opentelemetry/trace/span_metadata.h"
#include "opentelemetry/trace/trace_flags.h"
#include "opentelemetry/trace/trace_id.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace zipkin
{

using namespace opentelemetry::sdk::resource;
namespace trace_api = opentelemetry::trace;
namespace common    = opentelemetry::common;
namespace sdk       = opentelemetry::sdk;

// constexpr needs keys to be constexpr, const is next best to use.
static const std::map<trace_api::SpanKind, std::string> kSpanKindMap = {
    {trace_api::SpanKind::kClient, "CLIENT"},
    {trace_api::SpanKind::kServer, "SERVER"},
    {trace_api::SpanKind::kConsumer, "CONSUMER"},
    {trace_api::SpanKind::kProducer, "PRODUCER"},
};

//
// See `attribute_value.h` for details.
//
const int kAttributeValueSize = 16;

void Recordable::SetIdentity(const trace_api::SpanContext &span_context,
                             trace_api::SpanId parent_span_id) noexcept
{
  char trace_id_lower_base16[trace::TraceId::kSize * 2] = {0};
  span_context.trace_id().ToLowerBase16(trace_id_lower_base16);
  char span_id_lower_base16[trace::SpanId::kSize * 2] = {0};
  span_context.span_id().ToLowerBase16(span_id_lower_base16);
  if (parent_span_id.IsValid())
  {
    char parent_span_id_lower_base16[trace::SpanId::kSize * 2] = {0};
    parent_span_id.ToLowerBase16(parent_span_id_lower_base16);
    span_["parentId"] = std::string(parent_span_id_lower_base16, 16);
  }

  span_["id"]      = std::string(span_id_lower_base16, 16);
  span_["traceId"] = std::string(trace_id_lower_base16, 32);
}

void PopulateAttribute(nlohmann::json &attribute,
                       nostd::string_view key,
                       const common::AttributeValue &value)
{
  // Assert size of variant to ensure that this method gets updated if the variant
  // definition changes
  static_assert(nostd::variant_size<common::AttributeValue>::value == kAttributeValueSize,
                "AttributeValue contains unknown type");

  if (nostd::holds_alternative<bool>(value))
  {
    attribute[key.data()] = nostd::get<bool>(value);
  }
  else if (nostd::holds_alternative<int>(value))
  {
    attribute[key.data()] = nostd::get<int>(value);
  }
  else if (nostd::holds_alternative<int64_t>(value))
  {
    attribute[key.data()] = nostd::get<int64_t>(value);
  }
  else if (nostd::holds_alternative<unsigned int>(value))
  {
    attribute[key.data()] = nostd::get<unsigned int>(value);
  }
  else if (nostd::holds_alternative<uint64_t>(value))
  {
    attribute[key.data()] = nostd::get<uint64_t>(value);
  }
  else if (nostd::holds_alternative<double>(value))
  {
    attribute[key.data()] = nostd::get<double>(value);
  }
  else if (nostd::holds_alternative<const char *>(value))
  {
    attribute[key.data()] = nostd::get<const char *>(value);
  }
  else if (nostd::holds_alternative<nostd::string_view>(value))
  {
    attribute[key.data()] = nostd::string_view(nostd::get<nostd::string_view>(value).data(),
                                               nostd::get<nostd::string_view>(value).size());
  }
  else if (nostd::holds_alternative<nostd::span<const uint8_t>>(value))
  {
    attribute[key.data()] = {};
    for (const auto &val : nostd::get<nostd::span<const uint8_t>>(value))
    {
      attribute[key.data()].push_back(val);
    }
  }
  else if (nostd::holds_alternative<nostd::span<const bool>>(value))
  {
    attribute[key.data()] = {};
    for (const auto &val : nostd::get<nostd::span<const bool>>(value))
    {
      attribute[key.data()].push_back(val);
    }
  }
  else if (nostd::holds_alternative<nostd::span<const int>>(value))
  {
    attribute[key.data()] = {};
    for (const auto &val : nostd::get<nostd::span<const int>>(value))
    {
      attribute[key.data()].push_back(val);
    }
  }
  else if (nostd::holds_alternative<nostd::span<const int64_t>>(value))
  {
    attribute[key.data()] = {};
    for (const auto &val : nostd::get<nostd::span<const int64_t>>(value))
    {
      attribute[key.data()].push_back(val);
    }
  }
  else if (nostd::holds_alternative<nostd::span<const unsigned int>>(value))
  {
    attribute[key.data()] = {};
    for (const auto &val : nostd::get<nostd::span<const unsigned int>>(value))
    {
      attribute[key.data()].push_back(val);
    }
  }
  else if (nostd::holds_alternative<nostd::span<const uint64_t>>(value))
  {
    attribute[key.data()] = {};
    for (const auto &val : nostd::get<nostd::span<const uint64_t>>(value))
    {
      attribute[key.data()].push_back(val);
    }
  }
  else if (nostd::holds_alternative<nostd::span<const double>>(value))
  {
    attribute[key.data()] = {};
    for (const auto &val : nostd::get<nostd::span<const double>>(value))
    {
      attribute[key.data()].push_back(val);
    }
  }
  else if (nostd::holds_alternative<nostd::span<const nostd::string_view>>(value))
  {
    attribute[key.data()] = {};
    for (const auto &val : nostd::get<nostd::span<const nostd::string_view>>(value))
    {
      attribute[key.data()].push_back(std::string(val.data(), val.size()));
    }
  }
}

void Recordable::SetAttribute(nostd::string_view key, const common::AttributeValue &value) noexcept
{
  if (!span_.contains("tags"))
  {
    span_["tags"] = nlohmann::json::object();
  }
  PopulateAttribute(span_["tags"], key, value);
}

void Recordable::AddEvent(nostd::string_view name,
                          common::SystemTimestamp timestamp,
                          const common::KeyValueIterable &attributes) noexcept
{
  nlohmann::json attrs = nlohmann::json::object();  // empty object
  attributes.ForEachKeyValue([&](nostd::string_view key, common::AttributeValue value) noexcept {
    PopulateAttribute(attrs, key, value);
    return true;
  });

  nlohmann::json annotation = {{"value", nlohmann::json::object({{name.data(), attrs}}).dump()},
                               {"timestamp", std::chrono::duration_cast<std::chrono::microseconds>(
                                                 timestamp.time_since_epoch())
                                                 .count()}};

  if (!span_.contains("annotations"))
  {
    span_["annotations"] = nlohmann::json::array();
  }
  span_["annotations"].push_back(annotation);
}

void Recordable::AddLink(const trace_api::SpanContext & /* span_context */,
                         const common::KeyValueIterable & /* attributes */) noexcept
{
  // TODO: Currently not supported by specs:
  // https://github.com/open-telemetry/opentelemetry-specification/blob/master/specification/trace/sdk_exporters/zipkin.md
}

void Recordable::SetStatus(trace::StatusCode code, nostd::string_view description) noexcept
{
  if (code != trace::StatusCode::kUnset)
  {
    span_["tags"]["otel.status_code"] = code;
    if (code == trace::StatusCode::kError)
    {
      span_["tags"]["error"] = description;
    }
  }
}

void Recordable::SetName(nostd::string_view name) noexcept
{
  span_["name"] = name.data();
}

void Recordable::SetTraceFlags(opentelemetry::trace::TraceFlags /* flags */) noexcept
{
  // TODO: Currently not supported by specs:
  // https://github.com/open-telemetry/opentelemetry-specification/blob/master/specification/trace/sdk_exporters/zipkin.md
}

void Recordable::SetResource(const sdk::resource::Resource &resource) noexcept
{
  // only service.name attribute is supported by specs as of now.
  auto attributes = resource.GetAttributes();
  if (attributes.find(semconv::service::kServiceName) != attributes.end())
  {
    service_name_ = nostd::get<std::string>(attributes[semconv::service::kServiceName]);
  }
}

void Recordable::SetStartTime(common::SystemTimestamp start_time) noexcept
{
  span_["timestamp"] =
      std::chrono::duration_cast<std::chrono::microseconds>(start_time.time_since_epoch()).count();
}

void Recordable::SetDuration(std::chrono::nanoseconds duration) noexcept
{
  span_["duration"] = std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

void Recordable::SetSpanKind(trace_api::SpanKind span_kind) noexcept
{
  auto span_iter = kSpanKindMap.find(span_kind);
  if (span_iter != kSpanKindMap.end())
  {
    span_["kind"] = span_iter->second;
  }
}

void Recordable::SetInstrumentationScope(
    const sdk::instrumentationscope::InstrumentationScope &instrumentation_scope) noexcept
{
  span_["tags"]["otel.library.name"]    = instrumentation_scope.GetName();
  span_["tags"]["otel.library.version"] = instrumentation_scope.GetVersion();
}

}  // namespace zipkin
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
