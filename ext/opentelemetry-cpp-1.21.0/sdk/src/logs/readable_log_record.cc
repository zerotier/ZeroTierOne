// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <cstddef>
#include <memory>
#include <type_traits>

#include "opentelemetry/logs/severity.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/logs/readable_log_record.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/version/version.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace logs
{

nostd::string_view ReadableLogRecord::GetSeverityText() const noexcept
{
  std::size_t severity_index = static_cast<std::size_t>(GetSeverity());
  if (severity_index >= std::extent<decltype(opentelemetry::logs::SeverityNumToText)>::value)
  {
    return opentelemetry::logs::SeverityNumToText[0];
  }

  return opentelemetry::logs::SeverityNumToText[severity_index];
}

const opentelemetry::sdk::instrumentationscope::InstrumentationScope &
ReadableLogRecord::GetDefaultInstrumentationScope() noexcept
{
  // FIXME: Use shared default instrumentation scope?
  static std::unique_ptr<opentelemetry::sdk::instrumentationscope::InstrumentationScope>
      default_scope = opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create(
          "otel-cpp", OPENTELEMETRY_SDK_VERSION, "https://opentelemetry.io/schemas/1.15.0");
  return *default_scope;
}

const opentelemetry::sdk::resource::Resource &ReadableLogRecord::GetDefaultResource() noexcept
{
  static opentelemetry::sdk::resource::Resource default_resource =
      opentelemetry::sdk::resource::Resource::Create(
          {}, GetDefaultInstrumentationScope().GetSchemaURL());

  return default_resource;
}

}  // namespace logs
}  // namespace sdk

OPENTELEMETRY_END_NAMESPACE
