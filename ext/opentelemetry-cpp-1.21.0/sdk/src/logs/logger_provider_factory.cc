// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <memory>
#include <utility>
#include <vector>

#include "opentelemetry/sdk/instrumentationscope/scope_configurator.h"
#include "opentelemetry/sdk/logs/logger_config.h"
#include "opentelemetry/sdk/logs/logger_context.h"
#include "opentelemetry/sdk/logs/logger_provider.h"
#include "opentelemetry/sdk/logs/logger_provider_factory.h"
#include "opentelemetry/sdk/logs/processor.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace logs
{

std::unique_ptr<opentelemetry::sdk::logs::LoggerProvider> LoggerProviderFactory::Create(
    std::unique_ptr<LogRecordProcessor> &&processor)
{
  auto resource = opentelemetry::sdk::resource::Resource::Create({});
  return Create(std::move(processor), resource);
}

std::unique_ptr<opentelemetry::sdk::logs::LoggerProvider> LoggerProviderFactory::Create(
    std::unique_ptr<LogRecordProcessor> &&processor,
    const opentelemetry::sdk::resource::Resource &resource)
{
  auto logger_configurator =
      std::make_unique<instrumentationscope::ScopeConfigurator<LoggerConfig>>(
          instrumentationscope::ScopeConfigurator<LoggerConfig>::Builder(LoggerConfig::Default())
              .Build());
  return Create(std::move(processor), resource, std::move(logger_configurator));
}

std::unique_ptr<opentelemetry::sdk::logs::LoggerProvider> LoggerProviderFactory::Create(
    std::unique_ptr<LogRecordProcessor> &&processor,
    const resource::Resource &resource,
    std::unique_ptr<instrumentationscope::ScopeConfigurator<LoggerConfig>> logger_configurator)
{
  std::unique_ptr<opentelemetry::sdk::logs::LoggerProvider> provider(
      new LoggerProvider(std::move(processor), resource, std::move(logger_configurator)));
  return provider;
}

std::unique_ptr<opentelemetry::sdk::logs::LoggerProvider> LoggerProviderFactory::Create(
    std::vector<std::unique_ptr<LogRecordProcessor>> &&processors)
{
  auto resource = opentelemetry::sdk::resource::Resource::Create({});
  return Create(std::move(processors), resource);
}

std::unique_ptr<opentelemetry::sdk::logs::LoggerProvider> LoggerProviderFactory::Create(
    std::vector<std::unique_ptr<LogRecordProcessor>> &&processors,
    const opentelemetry::sdk::resource::Resource &resource)
{
  auto logger_configurator =
      std::make_unique<instrumentationscope::ScopeConfigurator<LoggerConfig>>(
          instrumentationscope::ScopeConfigurator<LoggerConfig>::Builder(LoggerConfig::Default())
              .Build());
  return Create(std::move(processors), resource, std::move(logger_configurator));
}

std::unique_ptr<opentelemetry::sdk::logs::LoggerProvider> LoggerProviderFactory::Create(
    std::vector<std::unique_ptr<LogRecordProcessor>> &&processors,
    const resource::Resource &resource,
    std::unique_ptr<instrumentationscope::ScopeConfigurator<LoggerConfig>> logger_configurator)
{
  std::unique_ptr<opentelemetry::sdk::logs::LoggerProvider> provider(
      new LoggerProvider(std::move(processors), resource, std::move(logger_configurator)));
  return provider;
}

std::unique_ptr<opentelemetry::sdk::logs::LoggerProvider> LoggerProviderFactory::Create(
    std::unique_ptr<LoggerContext> context)
{
  std::unique_ptr<opentelemetry::sdk::logs::LoggerProvider> provider(
      new LoggerProvider(std::move(context)));
  return provider;
}

}  // namespace logs
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
