// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <stdlib.h>

#include "opentelemetry/metrics/meter_provider.h"
#include "opentelemetry/metrics/noop.h"
#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/sdk/metrics/provider.h"

#if defined(_MSC_VER)
#  include "opentelemetry/sdk/common/env_variables.h"
using opentelemetry::sdk::common::setenv;
using opentelemetry::sdk::common::unsetenv;
#endif

using opentelemetry::metrics::MeterProvider;
using opentelemetry::metrics::NoopMeterProvider;
using opentelemetry::metrics::Provider;

namespace metrics_api = opentelemetry::metrics;
namespace metrics_sdk = opentelemetry::sdk::metrics;

TEST(Provider, SetMeterProviderDefault)
{
#ifndef NO_GETENV
  unsetenv("OTEL_SDK_DISABLED");
#endif

  auto tf = opentelemetry::nostd::shared_ptr<MeterProvider>(new NoopMeterProvider());
  metrics_sdk::Provider::SetMeterProvider(tf);
  ASSERT_EQ(tf, metrics_api::Provider::GetMeterProvider());
}

#ifndef NO_GETENV
TEST(Provider, SetMeterProviderEnabled)
{
  setenv("OTEL_SDK_DISABLED", "false", 1);

  auto tf = opentelemetry::nostd::shared_ptr<MeterProvider>(new NoopMeterProvider());
  metrics_sdk::Provider::SetMeterProvider(tf);
  ASSERT_EQ(tf, metrics_api::Provider::GetMeterProvider());

  unsetenv("OTEL_SDK_DISABLED");
}

TEST(Provider, SetMeterProviderDisabled)
{
  setenv("OTEL_SDK_DISABLED", "true", 1);

  auto tf = opentelemetry::nostd::shared_ptr<MeterProvider>(new NoopMeterProvider());
  metrics_sdk::Provider::SetMeterProvider(tf);
  ASSERT_NE(tf, metrics_api::Provider::GetMeterProvider());

  unsetenv("OTEL_SDK_DISABLED");
}
#endif

TEST(Provider, MultipleMeterProviders)
{
  auto tf = opentelemetry::nostd::shared_ptr<MeterProvider>(new NoopMeterProvider());
  metrics_sdk::Provider::SetMeterProvider(tf);
  auto tf2 = opentelemetry::nostd::shared_ptr<MeterProvider>(new NoopMeterProvider());
  metrics_sdk::Provider::SetMeterProvider(tf2);

  ASSERT_NE(metrics_api::Provider::GetMeterProvider(), tf);
}
