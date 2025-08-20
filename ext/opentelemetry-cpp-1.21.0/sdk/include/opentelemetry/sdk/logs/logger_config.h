// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace logs
{
/**
 * LoggerConfig defines various configurable aspects of a Logger's behavior.
 * This class should not be used directly to configure a Logger's behavior, instead a
 * ScopeConfigurator should be used to compute the desired LoggerConfig which can then be used to
 * configure a Logger.
 */
class OPENTELEMETRY_EXPORT LoggerConfig
{
public:
  bool operator==(const LoggerConfig &other) const noexcept;

  /**
   * Returns if the Logger is enabled or disabled. Loggers are enabled by default.
   * @return a boolean indicating if the Logger is enabled. Defaults to true.
   */
  bool IsEnabled() const noexcept;

  /**
   * Returns a LoggerConfig that represents an enabled Logger.
   * @return a static constant LoggerConfig that represents an enabled logger.
   */
  static LoggerConfig Enabled();

  /**
   * Returns a LoggerConfig that represents a disabled Logger. A disabled logger behaves like a
   * no-op logger.
   * @return a static constant LoggerConfig that represents a disabled logger.
   */
  static LoggerConfig Disabled();

  /**
   * Returns a LoggerConfig that represents a Logger configured with the default behavior.
   * The default behavior is guided by the OpenTelemetry specification.
   * @return a static constant LoggerConfig that represents a logger configured with default
   * behavior.
   */
  static LoggerConfig Default();

private:
  explicit LoggerConfig(const bool disabled = false) : disabled_(disabled) {}

  bool disabled_;
};
}  // namespace logs
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
