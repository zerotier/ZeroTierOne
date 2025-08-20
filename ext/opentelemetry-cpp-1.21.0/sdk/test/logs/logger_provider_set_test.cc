// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <stdlib.h>

#include "opentelemetry/common/key_value_iterable.h"
#include "opentelemetry/logs/event_logger.h"  // IWYU pragma: keep
#include "opentelemetry/logs/event_logger_provider.h"
#include "opentelemetry/logs/logger.h"  // IWYU pragma: keep
#include "opentelemetry/logs/logger_provider.h"
#include "opentelemetry/logs/provider.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/sdk/logs/provider.h"

#if defined(_MSC_VER)
#  include "opentelemetry/sdk/common/env_variables.h"
using opentelemetry::sdk::common::setenv;
using opentelemetry::sdk::common::unsetenv;
#endif

#if OPENTELEMETRY_ABI_VERSION_NO < 2
using opentelemetry::logs::EventLogger;
using opentelemetry::logs::EventLoggerProvider;
#endif
using opentelemetry::logs::Logger;
using opentelemetry::logs::LoggerProvider;
using opentelemetry::logs::Provider;
using opentelemetry::nostd::shared_ptr;

namespace nostd    = opentelemetry::nostd;
namespace logs_api = opentelemetry::logs;
namespace logs_sdk = opentelemetry::sdk::logs;

class TestProvider : public LoggerProvider
{
  nostd::shared_ptr<Logger> GetLogger(
      nostd::string_view /* logger_name */,
      nostd::string_view /* library_name */,
      nostd::string_view /* library_version */,
      nostd::string_view /* schema_url */,
      const opentelemetry::common::KeyValueIterable & /* attributes */) override
  {
    return shared_ptr<Logger>(nullptr);
  }
};

TEST(Provider, SetLoggerProviderDefault)
{
#ifndef NO_GETENV
  unsetenv("OTEL_SDK_DISABLED");
#endif

  auto tf = shared_ptr<LoggerProvider>(new TestProvider());
  logs_sdk::Provider::SetLoggerProvider(tf);
  ASSERT_EQ(tf, logs_api::Provider::GetLoggerProvider());
}

#ifndef NO_GETENV
TEST(Provider, SetLoggerProviderEnabled)
{
  setenv("OTEL_SDK_DISABLED", "false", 1);

  auto tf = shared_ptr<LoggerProvider>(new TestProvider());
  logs_sdk::Provider::SetLoggerProvider(tf);
  ASSERT_EQ(tf, logs_api::Provider::GetLoggerProvider());

  unsetenv("OTEL_SDK_DISABLED");
}

TEST(Provider, SetLoggerProviderDisabled)
{
  setenv("OTEL_SDK_DISABLED", "true", 1);

  auto tf = shared_ptr<LoggerProvider>(new TestProvider());
  logs_sdk::Provider::SetLoggerProvider(tf);
  ASSERT_NE(tf, logs_api::Provider::GetLoggerProvider());

  unsetenv("OTEL_SDK_DISABLED");
}
#endif

TEST(Provider, MultipleLoggerProviders)
{
  auto tf = shared_ptr<LoggerProvider>(new TestProvider());
  logs_sdk::Provider::SetLoggerProvider(tf);
  auto tf2 = shared_ptr<LoggerProvider>(new TestProvider());
  logs_sdk::Provider::SetLoggerProvider(tf2);

  ASSERT_NE(logs_api::Provider::GetLoggerProvider(), tf);
}
