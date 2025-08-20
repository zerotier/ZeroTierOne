// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <utility>

#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/logs/log_record.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/sdk/logs/multi_recordable.h"
#include "opentelemetry/sdk/logs/processor.h"
#include "opentelemetry/sdk/logs/recordable.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace logs
{

namespace
{
std::size_t MakeKey(const opentelemetry::sdk::logs::LogRecordProcessor &processor)
{
  return reinterpret_cast<std::size_t>(&processor);
}

}  // namespace

void MultiRecordable::AddRecordable(const LogRecordProcessor &processor,
                                    std::unique_ptr<Recordable> recordable) noexcept
{
  recordables_[MakeKey(processor)] = std::move(recordable);
}

const std::unique_ptr<Recordable> &MultiRecordable::GetRecordable(
    const LogRecordProcessor &processor) const noexcept
{
  // TODO - return nullptr ref on failed lookup?
  auto i = recordables_.find(MakeKey(processor));
  if (i != recordables_.end())
  {
    return i->second;
  }
  static std::unique_ptr<Recordable> empty(nullptr);
  return empty;
}

std::unique_ptr<Recordable> MultiRecordable::ReleaseRecordable(
    const LogRecordProcessor &processor) noexcept
{
  auto i = recordables_.find(MakeKey(processor));
  if (i != recordables_.end())
  {
    std::unique_ptr<Recordable> result(i->second.release());
    recordables_.erase(MakeKey(processor));
    return result;
  }
  return std::unique_ptr<Recordable>(nullptr);
}

void MultiRecordable::SetTimestamp(opentelemetry::common::SystemTimestamp timestamp) noexcept
{
  for (auto &recordable : recordables_)
  {
    if (recordable.second)
    {
      recordable.second->SetTimestamp(timestamp);
    }
  }
}

void MultiRecordable::SetObservedTimestamp(
    opentelemetry::common::SystemTimestamp timestamp) noexcept
{
  for (auto &recordable : recordables_)
  {
    if (recordable.second)
    {
      recordable.second->SetObservedTimestamp(timestamp);
    }
  }
}

void MultiRecordable::SetSeverity(opentelemetry::logs::Severity severity) noexcept
{
  for (auto &recordable : recordables_)
  {
    if (recordable.second)
    {
      recordable.second->SetSeverity(severity);
    }
  }
}

void MultiRecordable::SetBody(const opentelemetry::common::AttributeValue &message) noexcept
{
  for (auto &recordable : recordables_)
  {
    if (recordable.second)
    {
      recordable.second->SetBody(message);
    }
  }
}

void MultiRecordable::SetEventId(int64_t id, nostd::string_view name) noexcept
{
  for (auto &recordable : recordables_)
  {
    if (recordable.second)
    {
      recordable.second->SetEventId(id, name);
    }
  }
}

void MultiRecordable::SetTraceId(const opentelemetry::trace::TraceId &trace_id) noexcept
{
  for (auto &recordable : recordables_)
  {
    if (recordable.second)
    {
      recordable.second->SetTraceId(trace_id);
    }
  }
}

void MultiRecordable::SetSpanId(const opentelemetry::trace::SpanId &span_id) noexcept
{
  for (auto &recordable : recordables_)
  {
    if (recordable.second)
    {
      recordable.second->SetSpanId(span_id);
    }
  }
}

void MultiRecordable::SetTraceFlags(const opentelemetry::trace::TraceFlags &trace_flags) noexcept
{
  for (auto &recordable : recordables_)
  {
    if (recordable.second)
    {
      recordable.second->SetTraceFlags(trace_flags);
    }
  }
}

void MultiRecordable::SetAttribute(nostd::string_view key,
                                   const opentelemetry::common::AttributeValue &value) noexcept
{
  for (auto &recordable : recordables_)
  {
    if (recordable.second)
    {
      recordable.second->SetAttribute(key, value);
    }
  }
}

void MultiRecordable::SetResource(const opentelemetry::sdk::resource::Resource &resource) noexcept
{
  for (auto &recordable : recordables_)
  {
    if (recordable.second)
    {
      recordable.second->SetResource(resource);
    }
  }
}

void MultiRecordable::SetInstrumentationScope(
    const opentelemetry::sdk::instrumentationscope::InstrumentationScope
        &instrumentation_scope) noexcept
{
  for (auto &recordable : recordables_)
  {
    if (recordable.second)
    {
      recordable.second->SetInstrumentationScope(instrumentation_scope);
    }
  }
}

}  // namespace logs
}  // namespace sdk

OPENTELEMETRY_END_NAMESPACE
