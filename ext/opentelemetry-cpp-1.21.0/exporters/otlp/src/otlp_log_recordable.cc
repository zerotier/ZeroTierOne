// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/exporters/otlp/otlp_log_recordable.h"
#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/exporters/otlp/otlp_populate_attribute_utils.h"
#include "opentelemetry/logs/severity.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/logs/readable_log_record.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/trace/span_id.h"
#include "opentelemetry/trace/trace_flags.h"
#include "opentelemetry/trace/trace_id.h"
#include "opentelemetry/version.h"

// clang-format off
#include "opentelemetry/exporters/otlp/protobuf_include_prefix.h" // IWYU pragma: keep
#include "opentelemetry/proto/logs/v1/logs.pb.h"
#include "opentelemetry/exporters/otlp/protobuf_include_suffix.h" // IWYU pragma: keep
// clang-format on

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{

const opentelemetry::sdk::resource::Resource &OtlpLogRecordable::GetResource() const noexcept
{
  if OPENTELEMETRY_LIKELY_CONDITION (nullptr != resource_)
  {
    return *resource_;
  }

  return opentelemetry::sdk::logs::ReadableLogRecord::GetDefaultResource();
}

const opentelemetry::sdk::instrumentationscope::InstrumentationScope &
OtlpLogRecordable::GetInstrumentationScope() const noexcept
{
  if OPENTELEMETRY_LIKELY_CONDITION (nullptr != instrumentation_scope_)
  {
    return *instrumentation_scope_;
  }

  return opentelemetry::sdk::logs::ReadableLogRecord::GetDefaultInstrumentationScope();
}

void OtlpLogRecordable::SetTimestamp(opentelemetry::common::SystemTimestamp timestamp) noexcept
{
  proto_record_.set_time_unix_nano(timestamp.time_since_epoch().count());
}

void OtlpLogRecordable::SetObservedTimestamp(
    opentelemetry::common::SystemTimestamp timestamp) noexcept
{
  proto_record_.set_observed_time_unix_nano(timestamp.time_since_epoch().count());
}

void OtlpLogRecordable::SetSeverity(opentelemetry::logs::Severity severity) noexcept
{
  switch (severity)
  {
    case opentelemetry::logs::Severity::kTrace: {
      proto_record_.set_severity_text("TRACE");
      proto_record_.set_severity_number(proto::logs::v1::SEVERITY_NUMBER_TRACE);
      break;
    }
    case opentelemetry::logs::Severity::kTrace2: {
      proto_record_.set_severity_text("TRACE2");
      proto_record_.set_severity_number(proto::logs::v1::SEVERITY_NUMBER_TRACE2);
      break;
    }
    case opentelemetry::logs::Severity::kTrace3: {
      proto_record_.set_severity_text("TRACE3");
      proto_record_.set_severity_number(proto::logs::v1::SEVERITY_NUMBER_TRACE3);
      break;
    }
    case opentelemetry::logs::Severity::kTrace4: {
      proto_record_.set_severity_text("TRACE4");
      proto_record_.set_severity_number(proto::logs::v1::SEVERITY_NUMBER_TRACE4);
      break;
    }
    case opentelemetry::logs::Severity::kDebug: {
      proto_record_.set_severity_text("DEBUG");
      proto_record_.set_severity_number(proto::logs::v1::SEVERITY_NUMBER_DEBUG);
      break;
    }
    case opentelemetry::logs::Severity::kDebug2: {
      proto_record_.set_severity_text("DEBUG2");
      proto_record_.set_severity_number(proto::logs::v1::SEVERITY_NUMBER_DEBUG2);
      break;
    }
    case opentelemetry::logs::Severity::kDebug3: {
      proto_record_.set_severity_text("DEBUG3");
      proto_record_.set_severity_number(proto::logs::v1::SEVERITY_NUMBER_DEBUG3);
      break;
    }
    case opentelemetry::logs::Severity::kDebug4: {
      proto_record_.set_severity_text("DEBUG4");
      proto_record_.set_severity_number(proto::logs::v1::SEVERITY_NUMBER_DEBUG4);
      break;
    }
    case opentelemetry::logs::Severity::kInfo: {
      proto_record_.set_severity_text("INFO");
      proto_record_.set_severity_number(proto::logs::v1::SEVERITY_NUMBER_INFO);
      break;
    }
    case opentelemetry::logs::Severity::kInfo2: {
      proto_record_.set_severity_text("INFO2");
      proto_record_.set_severity_number(proto::logs::v1::SEVERITY_NUMBER_INFO2);
      break;
    }
    case opentelemetry::logs::Severity::kInfo3: {
      proto_record_.set_severity_text("INFO3");
      proto_record_.set_severity_number(proto::logs::v1::SEVERITY_NUMBER_INFO3);
      break;
    }
    case opentelemetry::logs::Severity::kInfo4: {
      proto_record_.set_severity_text("INFO4");
      proto_record_.set_severity_number(proto::logs::v1::SEVERITY_NUMBER_INFO4);
      break;
    }
    case opentelemetry::logs::Severity::kWarn: {
      proto_record_.set_severity_text("WARN");
      proto_record_.set_severity_number(proto::logs::v1::SEVERITY_NUMBER_WARN);
      break;
    }
    case opentelemetry::logs::Severity::kWarn2: {
      proto_record_.set_severity_text("WARN2");
      proto_record_.set_severity_number(proto::logs::v1::SEVERITY_NUMBER_WARN2);
      break;
    }
    case opentelemetry::logs::Severity::kWarn3: {
      proto_record_.set_severity_text("WARN3");
      proto_record_.set_severity_number(proto::logs::v1::SEVERITY_NUMBER_WARN3);
      break;
    }
    case opentelemetry::logs::Severity::kWarn4: {
      proto_record_.set_severity_text("WARN4");
      proto_record_.set_severity_number(proto::logs::v1::SEVERITY_NUMBER_WARN4);
      break;
    }
    case opentelemetry::logs::Severity::kError: {
      proto_record_.set_severity_text("ERROR");
      proto_record_.set_severity_number(proto::logs::v1::SEVERITY_NUMBER_ERROR);
      break;
    }
    case opentelemetry::logs::Severity::kError2: {
      proto_record_.set_severity_text("ERROR2");
      proto_record_.set_severity_number(proto::logs::v1::SEVERITY_NUMBER_ERROR2);
      break;
    }
    case opentelemetry::logs::Severity::kError3: {
      proto_record_.set_severity_text("ERROR3");
      proto_record_.set_severity_number(proto::logs::v1::SEVERITY_NUMBER_ERROR3);
      break;
    }
    case opentelemetry::logs::Severity::kError4: {
      proto_record_.set_severity_text("ERROR4");
      proto_record_.set_severity_number(proto::logs::v1::SEVERITY_NUMBER_ERROR4);
      break;
    }
    case opentelemetry::logs::Severity::kFatal: {
      proto_record_.set_severity_text("FATAL");
      proto_record_.set_severity_number(proto::logs::v1::SEVERITY_NUMBER_FATAL);
      break;
    }
    case opentelemetry::logs::Severity::kFatal2: {
      proto_record_.set_severity_text("FATAL2");
      proto_record_.set_severity_number(proto::logs::v1::SEVERITY_NUMBER_FATAL2);
      break;
    }
    case opentelemetry::logs::Severity::kFatal3: {
      proto_record_.set_severity_text("FATAL3");
      proto_record_.set_severity_number(proto::logs::v1::SEVERITY_NUMBER_FATAL3);
      break;
    }
    case opentelemetry::logs::Severity::kFatal4: {
      proto_record_.set_severity_text("FATAL4");
      proto_record_.set_severity_number(proto::logs::v1::SEVERITY_NUMBER_FATAL4);
      break;
    }
    default: {
      proto_record_.set_severity_text("INVALID");
      proto_record_.set_severity_number(proto::logs::v1::SEVERITY_NUMBER_UNSPECIFIED);
      break;
    }
  }
}

void OtlpLogRecordable::SetBody(const opentelemetry::common::AttributeValue &message) noexcept
{
  OtlpPopulateAttributeUtils::PopulateAnyValue(proto_record_.mutable_body(), message);
}

void OtlpLogRecordable::SetEventId(int64_t /* id */, nostd::string_view event_name) noexcept
{
  proto_record_.set_event_name(event_name.data(), event_name.size());
}

void OtlpLogRecordable::SetTraceId(const opentelemetry::trace::TraceId &trace_id) noexcept
{
  if (trace_id.IsValid())
  {
    proto_record_.set_trace_id(reinterpret_cast<const char *>(trace_id.Id().data()),
                               trace_id.Id().size());
  }
  else
  {
    proto_record_.clear_trace_id();
  }
}

void OtlpLogRecordable::SetSpanId(const opentelemetry::trace::SpanId &span_id) noexcept
{
  if (span_id.IsValid())
  {
    proto_record_.set_span_id(reinterpret_cast<const char *>(span_id.Id().data()),
                              span_id.Id().size());
  }
  else
  {
    proto_record_.clear_span_id();
  }
}

void OtlpLogRecordable::SetTraceFlags(const opentelemetry::trace::TraceFlags &trace_flags) noexcept
{
  proto_record_.set_flags(trace_flags.flags());
}

void OtlpLogRecordable::SetAttribute(opentelemetry::nostd::string_view key,
                                     const opentelemetry::common::AttributeValue &value) noexcept
{
  OtlpPopulateAttributeUtils::PopulateAttribute(proto_record_.add_attributes(), key, value);
}

void OtlpLogRecordable::SetResource(const opentelemetry::sdk::resource::Resource &resource) noexcept
{
  resource_ = &resource;
}

void OtlpLogRecordable::SetInstrumentationScope(
    const opentelemetry::sdk::instrumentationscope::InstrumentationScope
        &instrumentation_scope) noexcept
{
  instrumentation_scope_ = &instrumentation_scope;
}

}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
