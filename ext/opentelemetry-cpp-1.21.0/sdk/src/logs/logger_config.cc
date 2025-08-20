// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/sdk/logs/logger_config.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace logs
{

OPENTELEMETRY_EXPORT bool LoggerConfig::operator==(const LoggerConfig &other) const noexcept
{
  return disabled_ == other.disabled_;
}

OPENTELEMETRY_EXPORT bool LoggerConfig::IsEnabled() const noexcept
{
  return !disabled_;
}

OPENTELEMETRY_EXPORT LoggerConfig LoggerConfig::Enabled()
{
  return Default();
}

OPENTELEMETRY_EXPORT LoggerConfig LoggerConfig::Disabled()
{
  static const auto kDisabledConfig = LoggerConfig(true);
  return kDisabledConfig;
}

OPENTELEMETRY_EXPORT LoggerConfig LoggerConfig::Default()
{
  static const auto kDefaultConfig = LoggerConfig();
  return kDefaultConfig;
}

}  // namespace logs
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
