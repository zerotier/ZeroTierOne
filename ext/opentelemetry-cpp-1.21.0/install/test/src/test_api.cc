// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <opentelemetry/version.h>

#include <opentelemetry/logs/provider.h>
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/trace/provider.h>

#include <opentelemetry/trace/scope.h>
#include <opentelemetry/trace/span.h>
#include <opentelemetry/trace/tracer.h>

#include <opentelemetry/logs/log_record.h>
#include <opentelemetry/logs/logger.h>

#include <opentelemetry/metrics/async_instruments.h>
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/sync_instruments.h>

TEST(ApiInstallTest, VersionCheck)
{
  EXPECT_GE(OPENTELEMETRY_VERSION_MAJOR, 0);
  EXPECT_GE(OPENTELEMETRY_VERSION_MINOR, 0);
  EXPECT_GE(OPENTELEMETRY_VERSION_PATCH, 0);
  EXPECT_NE(OPENTELEMETRY_VERSION, "not a version");
}

TEST(ApiInstallTest, TraceApiCheck)
{
  auto provider = opentelemetry::trace::Provider::GetTracerProvider();
  ASSERT_TRUE(provider != nullptr);

  auto tracer = provider->GetTracer("test-tracer");
  ASSERT_TRUE(tracer != nullptr);

  auto span = tracer->StartSpan("test-span");
  ASSERT_TRUE(span != nullptr);

  auto scope = opentelemetry::trace::Tracer::WithActiveSpan(span);
  span->AddEvent("test-event");
  span->SetAttribute("test-attribute", "test-value");
  span->SetStatus(opentelemetry::trace::StatusCode::kOk, "test-status");
  span->End();
}

TEST(ApiInstallTest, LogsApiCheck)
{
  auto provider = opentelemetry::logs::Provider::GetLoggerProvider();
  ASSERT_TRUE(provider != nullptr);

  auto logger = provider->GetLogger("test-logger");
  ASSERT_TRUE(logger != nullptr);

  auto record = logger->CreateLogRecord();
  ASSERT_TRUE(record != nullptr);
  record->SetSeverity(opentelemetry::logs::Severity::kInfo);
  record->SetBody("test-body");
  record->SetAttribute("test-attribute", "test-value");
  logger->EmitLogRecord(std::move(record));
}

TEST(ApiInstallTest, MetricsApiCheck)
{
  auto provider = opentelemetry::metrics::Provider::GetMeterProvider();
  ASSERT_TRUE(provider != nullptr);

  auto meter = provider->GetMeter("test-meter");
  ASSERT_TRUE(meter != nullptr);

  auto counter = meter->CreateUInt64Counter("test-counter");
  ASSERT_TRUE(counter != nullptr);
  counter->Add(1, {{"test-attribute", "test-value"}});

  auto histogram = meter->CreateDoubleHistogram("test-histogram");
  ASSERT_TRUE(histogram != nullptr);
  histogram->Record(1, {{"test-attribute", "test-value"}},
                    opentelemetry::context::RuntimeContext::GetCurrent());

  auto async_gauge = meter->CreateDoubleObservableGauge("test-gauge");
  ASSERT_TRUE(async_gauge != nullptr);

  auto async_counter = meter->CreateDoubleObservableCounter("test-async-counter");
  ASSERT_TRUE(async_counter != nullptr);

  auto async_updown_counter =
      meter->CreateInt64ObservableUpDownCounter("test-async-updown-counter");
  ASSERT_TRUE(async_updown_counter != nullptr);
}