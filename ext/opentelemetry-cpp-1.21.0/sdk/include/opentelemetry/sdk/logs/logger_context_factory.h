// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <vector>

#include "opentelemetry/sdk/logs/logger_context.h"
#include "opentelemetry/sdk/logs/processor.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace logs
{

/**
 * Factory class for LoggerContext.
 */
class LoggerContextFactory
{
public:
  /**
   * Create a LoggerContext with specified LogRecordProcessors.
   * @param processors A vector of log processors that can parse the logs.
   * @return A unique pointer to the created LoggerContext object.
   */
  static std::unique_ptr<LoggerContext> Create(
      std::vector<std::unique_ptr<LogRecordProcessor>> &&processors);

  /**
   * Create a LoggerContext with specified LogRecordProcessors and OpenTelemetry resource.
   * @param processors A vector of log processors that can parse the logs.
   * @param resource The OpenTelemetry resource responsible for generating the logs.
   * @return A unique pointer to the created LoggerContext object.
   */
  static std::unique_ptr<LoggerContext> Create(
      std::vector<std::unique_ptr<LogRecordProcessor>> &&processors,
      const opentelemetry::sdk::resource::Resource &resource);

  /**
   * Create a LoggerContext with specified LogRecordProcessors, OpenTelemetry resource and a logger
   * ScopeConfigurator.
   * @param processors A vector of log processors that can parse the logs.
   * @param resource The OpenTelemetry resource responsible for generating the logs.
   * @param logger_configurator A ScopeConfigurator that can be used compute the LoggerConfig for a
   * given InstrumentationScope.
   * @return A unique pointer to the created LoggerContext object.
   */
  static std::unique_ptr<LoggerContext> Create(
      std::vector<std::unique_ptr<LogRecordProcessor>> &&processors,
      const opentelemetry::sdk::resource::Resource &resource,
      std::unique_ptr<instrumentationscope::ScopeConfigurator<LoggerConfig>> logger_configurator);
};

}  // namespace logs
}  // namespace sdk

OPENTELEMETRY_END_NAMESPACE
