// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <chrono>
#include <string>
#include <utility>

#include "opentelemetry/context/context.h"
#include "opentelemetry/context/context_value.h"
#include "opentelemetry/context/runtime_context.h"
#include "opentelemetry/logs/log_record.h"
#include "opentelemetry/logs/noop.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/unique_ptr.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/instrumentationscope/scope_configurator.h"
#include "opentelemetry/sdk/logs/logger.h"
#include "opentelemetry/sdk/logs/logger_config.h"
#include "opentelemetry/sdk/logs/logger_context.h"
#include "opentelemetry/sdk/logs/processor.h"
#include "opentelemetry/sdk/logs/recordable.h"
#include "opentelemetry/trace/span.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/span_metadata.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace logs
{
namespace trace_api = opentelemetry::trace;
namespace common    = opentelemetry::common;

opentelemetry::logs::NoopLogger Logger::kNoopLogger = opentelemetry::logs::NoopLogger();

Logger::Logger(
    opentelemetry::nostd::string_view name,
    std::shared_ptr<LoggerContext> context,
    std::unique_ptr<instrumentationscope::InstrumentationScope> instrumentation_scope) noexcept
    : logger_name_(std::string(name)),
      instrumentation_scope_(std::move(instrumentation_scope)),
      context_(std::move(context)),
      logger_config_(context_->GetLoggerConfigurator().ComputeConfig(*instrumentation_scope_))
{}

const opentelemetry::nostd::string_view Logger::GetName() noexcept
{
  if (!logger_config_.IsEnabled())
  {
    return kNoopLogger.GetName();
  }
  return logger_name_;
}

opentelemetry::nostd::unique_ptr<opentelemetry::logs::LogRecord> Logger::CreateLogRecord() noexcept
{
  if (!logger_config_.IsEnabled())
  {
    return kNoopLogger.CreateLogRecord();
  }

  auto recordable = context_->GetProcessor().MakeRecordable();

  recordable->SetObservedTimestamp(std::chrono::system_clock::now());

  if (opentelemetry::context::RuntimeContext::GetCurrent().HasKey(opentelemetry::trace::kSpanKey))
  {
    opentelemetry::context::ContextValue context_value =
        opentelemetry::context::RuntimeContext::GetCurrent().GetValue(
            opentelemetry::trace::kSpanKey);
    if (opentelemetry::nostd::holds_alternative<
            opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>>(context_value))
    {
      opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> &data =
          opentelemetry::nostd::get<opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>>(
              context_value);
      if (data)
      {
        recordable->SetTraceId(data->GetContext().trace_id());
        recordable->SetTraceFlags(data->GetContext().trace_flags());
        recordable->SetSpanId(data->GetContext().span_id());
      }
    }
    else if (opentelemetry::nostd::holds_alternative<
                 opentelemetry::nostd::shared_ptr<trace::SpanContext>>(context_value))
    {
      opentelemetry::nostd::shared_ptr<trace::SpanContext> &data =
          opentelemetry::nostd::get<opentelemetry::nostd::shared_ptr<trace::SpanContext>>(
              context_value);
      if (data)
      {
        recordable->SetTraceId(data->trace_id());
        recordable->SetTraceFlags(data->trace_flags());
        recordable->SetSpanId(data->span_id());
      }
    }
  }

  return opentelemetry::nostd::unique_ptr<opentelemetry::logs::LogRecord>(recordable.release());
}

void Logger::EmitLogRecord(
    opentelemetry::nostd::unique_ptr<opentelemetry::logs::LogRecord> &&log_record) noexcept
{
  if (!logger_config_.IsEnabled())
  {
    return kNoopLogger.EmitLogRecord(std::move(log_record));
  }

  if (!log_record)
  {
    return;
  }

  std::unique_ptr<Recordable> recordable =
      std::unique_ptr<Recordable>(static_cast<Recordable *>(log_record.release()));
  recordable->SetResource(context_->GetResource());
  recordable->SetInstrumentationScope(GetInstrumentationScope());

  auto &processor = context_->GetProcessor();

  // TODO: Sampler (should include check for minSeverity)

  // Send the log recordable to the processor
  processor.OnEmit(std::move(recordable));
}

const opentelemetry::sdk::instrumentationscope::InstrumentationScope &
Logger::GetInstrumentationScope() const noexcept
{
  return *instrumentation_scope_;
}

}  // namespace logs
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
