// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <chrono>
#include <mutex>
#include <utility>
#include <vector>

#include "opentelemetry/common/key_value_iterable.h"
#include "opentelemetry/logs/logger.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/sdk/common/global_log_handler.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/instrumentationscope/scope_configurator.h"
#include "opentelemetry/sdk/logs/logger.h"
#include "opentelemetry/sdk/logs/logger_config.h"
#include "opentelemetry/sdk/logs/logger_context.h"
#include "opentelemetry/sdk/logs/logger_provider.h"
#include "opentelemetry/sdk/logs/processor.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace logs
{

LoggerProvider::LoggerProvider(
    std::unique_ptr<LogRecordProcessor> &&processor,
    const opentelemetry::sdk::resource::Resource &resource,
    std::unique_ptr<instrumentationscope::ScopeConfigurator<LoggerConfig>>
        logger_configurator) noexcept
{
  std::vector<std::unique_ptr<LogRecordProcessor>> processors;
  processors.emplace_back(std::move(processor));
  context_ = std::make_shared<LoggerContext>(std::move(processors), resource,
                                             std::move(logger_configurator));
  OTEL_INTERNAL_LOG_DEBUG("[LoggerProvider] LoggerProvider created.");
}

LoggerProvider::LoggerProvider(
    std::vector<std::unique_ptr<LogRecordProcessor>> &&processors,
    const opentelemetry::sdk::resource::Resource &resource,
    std::unique_ptr<instrumentationscope::ScopeConfigurator<LoggerConfig>>
        logger_configurator) noexcept
    : context_{std::make_shared<LoggerContext>(std::move(processors),
                                               resource,
                                               std::move(logger_configurator))}
{}

LoggerProvider::LoggerProvider() noexcept
    : context_{std::make_shared<LoggerContext>(std::vector<std::unique_ptr<LogRecordProcessor>>{})}
{}

LoggerProvider::LoggerProvider(std::unique_ptr<LoggerContext> context) noexcept
    : context_(std::move(context))
{}

LoggerProvider::~LoggerProvider()
{
  // Logger hold the shared pointer to the context. So we can not use destructor of LoggerContext to
  // Shutdown and flush all pending recordables when we have more than one loggers. These
  // recordables may use the raw pointer of instrumentation_scope_ in Logger
  if (context_)
  {
    context_->Shutdown();
  }
}

opentelemetry::nostd::shared_ptr<opentelemetry::logs::Logger> LoggerProvider::GetLogger(
    opentelemetry::nostd::string_view logger_name,
    opentelemetry::nostd::string_view library_name,
    opentelemetry::nostd::string_view library_version,
    opentelemetry::nostd::string_view schema_url,
    const opentelemetry::common::KeyValueIterable &attributes) noexcept
{
  // https://github.com/open-telemetry/opentelemetry-specification/blob/main/specification/logs/data-model.md#field-instrumentationscope
  if (library_name.empty())
  {
    library_name = logger_name;
  }

  // Ensure only one thread can read/write from the map of loggers
  std::lock_guard<std::mutex> lock_guard{lock_};

  // If a logger with a name "logger_name" already exists, return it
  for (auto &logger : loggers_)
  {
    auto &logger_lib = logger->GetInstrumentationScope();
    if (logger->GetName() == logger_name &&
        logger_lib.equal(library_name, library_version, schema_url, &attributes))
    {
      return opentelemetry::nostd::shared_ptr<opentelemetry::logs::Logger>{logger};
    }
  }

  // Check if creating a new logger would exceed the max number of loggers
  // TODO: Remove the noexcept from the API's and SDK's GetLogger(~)
  /*
  if (loggers_.size() > MAX_LOGGER_COUNT)
  {
#if __EXCEPTIONS
    throw std::length_error("Number of loggers exceeds max count");
#else
    std::terminate();
#endif
  }
  */

  std::unique_ptr<instrumentationscope::InstrumentationScope> lib =
      instrumentationscope::InstrumentationScope::Create(library_name, library_version, schema_url,
                                                         attributes);

  loggers_.push_back(std::shared_ptr<opentelemetry::sdk::logs::Logger>(
      new Logger(logger_name, context_, std::move(lib))));
  return opentelemetry::nostd::shared_ptr<opentelemetry::logs::Logger>{loggers_.back()};
}

void LoggerProvider::AddProcessor(std::unique_ptr<LogRecordProcessor> processor) noexcept
{
  context_->AddProcessor(std::move(processor));
}

const opentelemetry::sdk::resource::Resource &LoggerProvider::GetResource() const noexcept
{
  return context_->GetResource();
}

bool LoggerProvider::Shutdown(std::chrono::microseconds timeout) noexcept
{
  return context_->Shutdown(timeout);
}

bool LoggerProvider::ForceFlush(std::chrono::microseconds timeout) noexcept
{
  return context_->ForceFlush(timeout);
}

}  // namespace logs
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
