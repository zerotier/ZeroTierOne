// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/sdk/logs/event_logger_provider_factory.h"
#include "opentelemetry/sdk/logs/event_logger_provider.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace logs
{
#if OPENTELEMETRY_ABI_VERSION_NO < 2
std::unique_ptr<opentelemetry::sdk::logs::EventLoggerProvider> EventLoggerProviderFactory::Create()
{
  return std::unique_ptr<opentelemetry::sdk::logs::EventLoggerProvider>(new EventLoggerProvider());
}
#endif
}  // namespace logs
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
