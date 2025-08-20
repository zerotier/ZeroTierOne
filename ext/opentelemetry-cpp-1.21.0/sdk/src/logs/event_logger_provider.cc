// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/sdk/logs/event_logger_provider.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/sdk/common/global_log_handler.h"
#include "opentelemetry/sdk/logs/event_logger.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace logs
{
#if OPENTELEMETRY_ABI_VERSION_NO < 2
EventLoggerProvider::EventLoggerProvider() noexcept
{
  OTEL_INTERNAL_LOG_DEBUG("[EventLoggerProvider] EventLoggerProvider created.");
}

EventLoggerProvider::~EventLoggerProvider() {}

opentelemetry::nostd::shared_ptr<opentelemetry::logs::EventLogger>
EventLoggerProvider::CreateEventLogger(
    opentelemetry::nostd::shared_ptr<opentelemetry::logs::Logger> delegate_logger,
    opentelemetry::nostd::string_view event_domain) noexcept
{
  return opentelemetry::nostd::shared_ptr<opentelemetry::logs::EventLogger>{
      new EventLogger(delegate_logger, event_domain)};
}
#endif
}  // namespace logs
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
