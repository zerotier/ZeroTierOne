// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstddef>
#include <list>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "opentelemetry/exporters/otlp/otlp_log_recordable.h"
#include "opentelemetry/exporters/otlp/otlp_populate_attribute_utils.h"
#include "opentelemetry/exporters/otlp/otlp_recordable.h"
#include "opentelemetry/exporters/otlp/otlp_recordable_utils.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/logs/recordable.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/trace/recordable.h"
#include "opentelemetry/version.h"

// clang-format off
#include "opentelemetry/exporters/otlp/protobuf_include_prefix.h"  // IWYU pragma: keep
#include "opentelemetry/proto/collector/logs/v1/logs_service.pb.h"
#include "opentelemetry/proto/collector/trace/v1/trace_service.pb.h"
#include "opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/logs/v1/logs.pb.h"
#include "opentelemetry/proto/resource/v1/resource.pb.h"
#include "opentelemetry/proto/trace/v1/trace.pb.h"
#include "opentelemetry/exporters/otlp/protobuf_include_suffix.h"  // IWYU pragma: keep
// clang-format on

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{

namespace
{
struct InstrumentationScopePointerHasher
{
  std::size_t operator()(const opentelemetry::sdk::instrumentationscope::InstrumentationScope
                             *instrumentation) const noexcept
  {
    return instrumentation->HashCode();
  }
};

struct InstrumentationScopePointerEqual
{
  std::size_t operator()(
      const opentelemetry::sdk::instrumentationscope::InstrumentationScope *left,
      const opentelemetry::sdk::instrumentationscope::InstrumentationScope *right) const noexcept
  {
    return *left == *right;
  }
};
}  // namespace

void OtlpRecordableUtils::PopulateRequest(
    const opentelemetry::nostd::span<std::unique_ptr<opentelemetry::sdk::trace::Recordable>> &spans,
    proto::collector::trace::v1::ExportTraceServiceRequest *request) noexcept
{
  if (nullptr == request)
  {
    return;
  }

  using spans_by_scope =
      std::unordered_map<const opentelemetry::sdk::instrumentationscope::InstrumentationScope *,
                         std::vector<std::unique_ptr<OtlpRecordable>>>;
  std::unordered_map<const opentelemetry::sdk::resource::Resource *, spans_by_scope> spans_index;

  // Collect spans per resource and instrumentation scope
  for (auto &recordable : spans)
  {
    auto rec = std::unique_ptr<OtlpRecordable>(static_cast<OtlpRecordable *>(recordable.release()));
    auto resource        = rec->GetResource();
    auto instrumentation = rec->GetInstrumentationScope();

    spans_index[resource][instrumentation].emplace_back(std::move(rec));
  }

  // Add all resource spans
  for (auto &input_resource_spans : spans_index)
  {
    // Add the resource
    auto resource_spans = request->add_resource_spans();
    if (input_resource_spans.first)
    {
      proto::resource::v1::Resource resource_proto;
      OtlpPopulateAttributeUtils::PopulateAttribute(&resource_proto, *input_resource_spans.first);
      *resource_spans->mutable_resource() = resource_proto;
      resource_spans->set_schema_url(input_resource_spans.first->GetSchemaURL());
    }

    // Add all scope spans
    for (auto &input_scope_spans : input_resource_spans.second)
    {
      // Add the instrumentation scope
      auto scope_spans = resource_spans->add_scope_spans();
      if (input_scope_spans.first)
      {
        proto::common::v1::InstrumentationScope instrumentation_scope_proto;
        instrumentation_scope_proto.set_name(input_scope_spans.first->GetName());
        instrumentation_scope_proto.set_version(input_scope_spans.first->GetVersion());
        OtlpPopulateAttributeUtils::PopulateAttribute(&instrumentation_scope_proto,
                                                      *input_scope_spans.first);

        *scope_spans->mutable_scope() = instrumentation_scope_proto;
        scope_spans->set_schema_url(input_scope_spans.first->GetSchemaURL());
      }

      // Add all spans to this scope spans
      for (auto &input_span : input_scope_spans.second)
      {
        *scope_spans->add_spans() = std::move(input_span->span());
      }
    }
  }
}

void OtlpRecordableUtils::PopulateRequest(
    const opentelemetry::nostd::span<std::unique_ptr<opentelemetry::sdk::logs::Recordable>> &logs,
    proto::collector::logs::v1::ExportLogsServiceRequest *request) noexcept
{
  if (nullptr == request)
  {
    return;
  }

  using logs_index_by_instrumentation_type =
      std::unordered_map<const opentelemetry::sdk::instrumentationscope::InstrumentationScope *,
                         std::list<std::unique_ptr<OtlpLogRecordable>>,
                         InstrumentationScopePointerHasher, InstrumentationScopePointerEqual>;
  std::unordered_map<const opentelemetry::sdk::resource::Resource *,
                     logs_index_by_instrumentation_type>
      logs_index_by_resource;

  for (auto &recordable : logs)
  {
    auto rec =
        std::unique_ptr<OtlpLogRecordable>(static_cast<OtlpLogRecordable *>(recordable.release()));
    auto instrumentation = &rec->GetInstrumentationScope();
    auto resource        = &rec->GetResource();

    logs_index_by_resource[resource][instrumentation].emplace_back(std::move(rec));
  }

  for (auto &input_resource_log : logs_index_by_resource)
  {
    auto output_resource_log = request->add_resource_logs();
    for (auto &input_scope_log : input_resource_log.second)
    {
      auto output_scope_log = output_resource_log->add_scope_logs();
      for (auto &input_log_record : input_scope_log.second)
      {
        if (!output_resource_log->has_resource())
        {
          OtlpPopulateAttributeUtils::PopulateAttribute(output_resource_log->mutable_resource(),
                                                        *input_resource_log.first);
          output_resource_log->set_schema_url(input_resource_log.first->GetSchemaURL());
        }

        if (!output_scope_log->has_scope())
        {
          auto proto_scope = output_scope_log->mutable_scope();
          if (proto_scope != nullptr)
          {
            proto_scope->set_name(input_scope_log.first->GetName());
            proto_scope->set_version(input_scope_log.first->GetVersion());

            OtlpPopulateAttributeUtils::PopulateAttribute(proto_scope, *input_scope_log.first);
          }
          output_scope_log->set_schema_url(input_scope_log.first->GetSchemaURL());
        }

        *output_scope_log->add_log_records() = std::move(input_log_record->log_record());
      }
    }
  }
}
}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
