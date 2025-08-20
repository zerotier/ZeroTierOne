// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0/

#pragma once

#include <memory>

#include "opentelemetry/sdk/logs/event_logger_provider.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace logs
{

#if OPENTELEMETRY_ABI_VERSION_NO < 2
/**
 * Factory class for EventLoggerProvider.
 */
class OPENTELEMETRY_DEPRECATED EventLoggerProviderFactory
{
public:
  /**
   * Create a EventLoggerProvider.
   */

  OPENTELEMETRY_DEPRECATED static std::unique_ptr<opentelemetry::sdk::logs::EventLoggerProvider>
  Create();
};
#endif

}  // namespace logs
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
