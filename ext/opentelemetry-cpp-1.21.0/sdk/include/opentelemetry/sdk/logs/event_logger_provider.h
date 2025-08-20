// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0/

#pragma once

#include "opentelemetry/logs/event_logger.h"
#include "opentelemetry/logs/event_logger_provider.h"
#include "opentelemetry/logs/logger.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/version.h"

// Define the maximum number of loggers that are allowed to be registered to the loggerprovider.
// TODO: Add link to logging spec once this is added to it
#define MAX_LOGGER_COUNT 100

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace logs
{
#if OPENTELEMETRY_ABI_VERSION_NO < 2
class OPENTELEMETRY_EXPORT OPENTELEMETRY_DEPRECATED EventLoggerProvider final
    : public opentelemetry::logs::EventLoggerProvider
{
public:
  EventLoggerProvider() noexcept;

  ~EventLoggerProvider() override;

  nostd::shared_ptr<opentelemetry::logs::EventLogger> CreateEventLogger(
      nostd::shared_ptr<opentelemetry::logs::Logger> delegate_logger,
      nostd::string_view event_domain) noexcept override;
};
#endif
}  // namespace logs
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
