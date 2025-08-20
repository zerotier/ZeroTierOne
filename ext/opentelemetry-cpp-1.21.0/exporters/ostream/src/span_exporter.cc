// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <atomic>
#include <chrono>
#include <iostream>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/exporters/ostream/common_utils.h"
#include "opentelemetry/exporters/ostream/span_exporter.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/sdk/common/attribute_utils.h"
#include "opentelemetry/sdk/common/exporter_utils.h"
#include "opentelemetry/sdk/common/global_log_handler.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/trace/recordable.h"
#include "opentelemetry/sdk/trace/span_data.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/span_id.h"
#include "opentelemetry/trace/span_metadata.h"
#include "opentelemetry/trace/trace_id.h"
#include "opentelemetry/trace/trace_state.h"
#include "opentelemetry/version.h"

namespace trace_sdk = opentelemetry::sdk::trace;
namespace trace_api = opentelemetry::trace;
namespace sdkcommon = opentelemetry::sdk::common;

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace trace
{

std::ostream &operator<<(std::ostream &os, trace_api::SpanKind span_kind)
{
  switch (span_kind)
  {
    case trace_api::SpanKind::kClient:
      return os << "Client";
    case trace_api::SpanKind::kInternal:
      return os << "Internal";
    case trace_api::SpanKind::kServer:
      return os << "Server";
    case trace_api::SpanKind::kProducer:
      return os << "Producer";
    case trace_api::SpanKind::kConsumer:
      return os << "Consumer";
  };
  return os << "";
}

OStreamSpanExporter::OStreamSpanExporter(std::ostream &sout) noexcept : sout_(sout) {}

std::unique_ptr<trace_sdk::Recordable> OStreamSpanExporter::MakeRecordable() noexcept
{
  return std::unique_ptr<trace_sdk::Recordable>(new trace_sdk::SpanData);
}

sdk::common::ExportResult OStreamSpanExporter::Export(
    const opentelemetry::nostd::span<std::unique_ptr<trace_sdk::Recordable>> &spans) noexcept
{
  if (isShutdown())
  {
    OTEL_INTERNAL_LOG_ERROR("[Ostream Trace Exporter] Exporting "
                            << spans.size() << " span(s) failed, exporter is shutdown");
    return sdk::common::ExportResult::kFailure;
  }

  for (auto &recordable : spans)
  {
    auto span = std::unique_ptr<trace_sdk::SpanData>(
        static_cast<trace_sdk::SpanData *>(recordable.release()));

    if (span != nullptr)
    {

      char trace_id[32]       = {0};
      char span_id[16]        = {0};
      char parent_span_id[16] = {0};

      span->GetTraceId().ToLowerBase16(trace_id);
      span->GetSpanId().ToLowerBase16(span_id);
      span->GetParentSpanId().ToLowerBase16(parent_span_id);

      sout_ << "{" << "\n  name          : " << span->GetName()
            << "\n  trace_id      : " << std::string(trace_id, 32)
            << "\n  span_id       : " << std::string(span_id, 16)
            << "\n  tracestate    : " << span->GetSpanContext().trace_state()->ToHeader()
            << "\n  parent_span_id: " << std::string(parent_span_id, 16)
            << "\n  start         : " << span->GetStartTime().time_since_epoch().count()
            << "\n  duration      : " << span->GetDuration().count()
            << "\n  description   : " << span->GetDescription()
            << "\n  span kind     : " << span->GetSpanKind()
            << "\n  status        : " << statusMap[static_cast<int>(span->GetStatus())]
            << "\n  attributes    : ";
      printAttributes(span->GetAttributes());
      sout_ << "\n  events        : ";
      printEvents(span->GetEvents());
      sout_ << "\n  links         : ";
      printLinks(span->GetLinks());
      sout_ << "\n  resources     : ";
      printResources(span->GetResource());
      sout_ << "\n  instr-lib     : ";
      printInstrumentationScope(span->GetInstrumentationScope());
      sout_ << "\n}\n";
    }
  }

  return sdk::common::ExportResult::kSuccess;
}

bool OStreamSpanExporter::ForceFlush(std::chrono::microseconds /* timeout */) noexcept
{
  sout_.flush();
  return true;
}

bool OStreamSpanExporter::Shutdown(std::chrono::microseconds /* timeout */) noexcept
{
  is_shutdown_ = true;
  return true;
}

bool OStreamSpanExporter::isShutdown() const noexcept
{
  return is_shutdown_;
}

void OStreamSpanExporter::printAttributes(
    const std::unordered_map<std::string, sdkcommon::OwnedAttributeValue> &map,
    const std::string &prefix)
{
  for (const auto &kv : map)
  {
    sout_ << prefix << kv.first << ": ";
    opentelemetry::exporter::ostream_common::print_value(kv.second, sout_);
  }
}

void OStreamSpanExporter::printEvents(const std::vector<trace_sdk::SpanDataEvent> &events)
{
  for (const auto &event : events)
  {
    sout_ << "\n\t{" << "\n\t  name          : " << event.GetName()
          << "\n\t  timestamp     : " << event.GetTimestamp().time_since_epoch().count()
          << "\n\t  attributes    : ";
    printAttributes(event.GetAttributes(), "\n\t\t");
    sout_ << "\n\t}";
  }
}

void OStreamSpanExporter::printLinks(const std::vector<trace_sdk::SpanDataLink> &links)
{
  for (const auto &link : links)
  {
    char trace_id[32] = {0};
    char span_id[16]  = {0};
    link.GetSpanContext().trace_id().ToLowerBase16(trace_id);
    link.GetSpanContext().span_id().ToLowerBase16(span_id);
    sout_ << "\n\t{" << "\n\t  trace_id      : " << std::string(trace_id, 32)
          << "\n\t  span_id       : " << std::string(span_id, 16)
          << "\n\t  tracestate    : " << link.GetSpanContext().trace_state()->ToHeader()
          << "\n\t  attributes    : ";
    printAttributes(link.GetAttributes(), "\n\t\t");
    sout_ << "\n\t}";
  }
}

void OStreamSpanExporter::printResources(const opentelemetry::sdk::resource::Resource &resources)
{
  const auto &attributes = resources.GetAttributes();
  if (attributes.size())
  {
    printAttributes(attributes, "\n\t");
  }
}

void OStreamSpanExporter::printInstrumentationScope(
    const opentelemetry::sdk::instrumentationscope::InstrumentationScope &instrumentation_scope)
{
  sout_ << instrumentation_scope.GetName();
  const auto &version = instrumentation_scope.GetVersion();
  if (version.size())
  {
    sout_ << "-" << version;
  }
}

}  // namespace trace
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
