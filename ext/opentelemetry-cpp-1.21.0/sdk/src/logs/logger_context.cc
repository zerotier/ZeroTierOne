// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <chrono>
#include <memory>
#include <utility>
#include <vector>

#include "opentelemetry/sdk/instrumentationscope/scope_configurator.h"
#include "opentelemetry/sdk/logs/logger_config.h"
#include "opentelemetry/sdk/logs/logger_context.h"
#include "opentelemetry/sdk/logs/multi_log_record_processor.h"
#include "opentelemetry/sdk/logs/processor.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace logs
{

LoggerContext::LoggerContext(std::vector<std::unique_ptr<LogRecordProcessor>> &&processors,
                             const opentelemetry::sdk::resource::Resource &resource,
                             std::unique_ptr<instrumentationscope::ScopeConfigurator<LoggerConfig>>
                                 logger_configurator) noexcept
    : resource_(resource),
      processor_(
          std::unique_ptr<LogRecordProcessor>(new MultiLogRecordProcessor(std::move(processors)))),
      logger_configurator_(std::move(logger_configurator))
{}

void LoggerContext::AddProcessor(std::unique_ptr<LogRecordProcessor> processor) noexcept
{
  auto multi_processor = static_cast<MultiLogRecordProcessor *>(processor_.get());
  multi_processor->AddProcessor(std::move(processor));
}

LogRecordProcessor &LoggerContext::GetProcessor() const noexcept
{
  return *processor_;
}

const opentelemetry::sdk::resource::Resource &LoggerContext::GetResource() const noexcept
{
  return resource_;
}

const instrumentationscope::ScopeConfigurator<LoggerConfig> &LoggerContext::GetLoggerConfigurator()
    const noexcept
{
  return *logger_configurator_;
}

bool LoggerContext::ForceFlush(std::chrono::microseconds timeout) noexcept
{
  return processor_->ForceFlush(timeout);
}

bool LoggerContext::Shutdown(std::chrono::microseconds timeout) noexcept
{
  return processor_->Shutdown(timeout);
}

}  // namespace logs
}  // namespace sdk

OPENTELEMETRY_END_NAMESPACE
