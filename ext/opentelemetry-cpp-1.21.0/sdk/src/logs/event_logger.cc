// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <string>
#include <utility>

#include "opentelemetry/logs/log_record.h"
#include "opentelemetry/logs/logger.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/unique_ptr.h"
#include "opentelemetry/sdk/logs/event_logger.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace logs
{
#if OPENTELEMETRY_ABI_VERSION_NO < 2
EventLogger::EventLogger(
    opentelemetry::nostd::shared_ptr<opentelemetry::logs::Logger> delegate_logger,
    opentelemetry::nostd::string_view event_domain) noexcept
    : delegate_logger_(std::move(delegate_logger)), event_domain_(event_domain)
{}

const opentelemetry::nostd::string_view EventLogger::GetName() noexcept
{
  if (delegate_logger_)
  {
    return delegate_logger_->GetName();
  }

  return {};
}

opentelemetry::nostd::shared_ptr<opentelemetry::logs::Logger>
EventLogger::GetDelegateLogger() noexcept
{
  return delegate_logger_;
}

void EventLogger::EmitEvent(
    opentelemetry::nostd::string_view event_name,
    opentelemetry::nostd::unique_ptr<opentelemetry::logs::LogRecord> &&log_record) noexcept
{
  if (!delegate_logger_ || !log_record)
  {
    return;
  }

  if (!event_domain_.empty() && !event_name.empty())
  {
    log_record->SetAttribute("event.domain", event_domain_);
    log_record->SetAttribute("event.name", event_name);
  }

  delegate_logger_->EmitLogRecord(std::move(log_record));
}
#endif
}  // namespace logs
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
