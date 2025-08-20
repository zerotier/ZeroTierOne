// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <mutex>

#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{

namespace logs
{

/**
 * Changes the singleton global LoggerProvider.
 */
class Provider
{
public:
  /**
   * Changes the singleton LoggerProvider.
   */
  static void SetLoggerProvider(
      const nostd::shared_ptr<opentelemetry::logs::LoggerProvider> &lp) noexcept;
};

}  // namespace logs
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
