// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <opentelemetry/logs/provider.h>
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/trace/provider.h>

#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/sdk/resource/resource_detector.h>
#include <opentelemetry/sdk/version/version.h>

#include <opentelemetry/sdk/instrumentationscope/instrumentation_scope.h>

#include <opentelemetry/sdk/logs/exporter.h>
#include <opentelemetry/sdk/metrics/instruments.h>
#include <opentelemetry/sdk/metrics/push_metric_exporter.h>
#include <opentelemetry/sdk/trace/exporter.h>

#include <opentelemetry/sdk/trace/provider.h>
#include <opentelemetry/sdk/trace/simple_processor_factory.h>
#include <opentelemetry/sdk/trace/tracer_provider_factory.h>

#include <opentelemetry/sdk/logs/logger_provider_factory.h>
#include <opentelemetry/sdk/logs/provider.h>
#include <opentelemetry/sdk/logs/simple_log_record_processor_factory.h>

#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h>
#include <opentelemetry/sdk/metrics/meter_context.h>
#include <opentelemetry/sdk/metrics/meter_context_factory.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>
#include <opentelemetry/sdk/metrics/provider.h>

namespace nostd        = opentelemetry::nostd;
namespace version_sdk  = opentelemetry::sdk::version;
namespace common       = opentelemetry::common;
namespace common_sdk   = opentelemetry::sdk::common;
namespace scope_sdk    = opentelemetry::sdk::instrumentationscope;
namespace resource_sdk = opentelemetry::sdk::resource;
namespace metrics_sdk  = opentelemetry::sdk::metrics;
namespace metrics      = opentelemetry::metrics;
namespace logs_sdk     = opentelemetry::sdk::logs;
namespace logs         = opentelemetry::logs;
namespace trace_sdk    = opentelemetry::sdk::trace;
namespace trace        = opentelemetry::trace;

class NoopLogRecordable : public logs_sdk::Recordable
{
public:
  ~NoopLogRecordable() override = default;
  void SetTimestamp(common::SystemTimestamp timestamp) noexcept override {}
  void SetObservedTimestamp(common::SystemTimestamp timestamp) noexcept override {}
  void SetSeverity(logs::Severity severity) noexcept override {}
  void SetBody(const common::AttributeValue &message) noexcept override {}
  void SetAttribute(nostd::string_view key, const common::AttributeValue &value) noexcept override
  {}
  void SetEventId(int64_t id, nostd::string_view name = {}) noexcept override {}
  void SetTraceId(const trace::TraceId &trace_id) noexcept override {}
  void SetSpanId(const trace::SpanId &span_id) noexcept override {}
  void SetTraceFlags(const trace::TraceFlags &trace_flags) noexcept override {}
  void SetResource(const opentelemetry::sdk::resource::Resource &resource) noexcept override {}
  void SetInstrumentationScope(
      const scope_sdk::InstrumentationScope &instrumentation_scope) noexcept override
  {}
};

class NoopLogRecordExporter : public logs_sdk::LogRecordExporter
{
public:
  ~NoopLogRecordExporter() override = default;
  std::unique_ptr<logs_sdk::Recordable> MakeRecordable() noexcept override
  {
    return std::move(std::unique_ptr<logs_sdk::Recordable>{new NoopLogRecordable()});
  }
  common_sdk::ExportResult Export(
      const nostd::span<std::unique_ptr<logs_sdk::Recordable>> &records) noexcept override
  {
    return common_sdk::ExportResult::kSuccess;
  }
  bool ForceFlush(
      std::chrono::microseconds timeout = (std::chrono::microseconds::max)()) noexcept override
  {
    return true;
  }
  bool Shutdown(
      std::chrono::microseconds timeout = (std::chrono::microseconds::max)()) noexcept override
  {
    return true;
  }
};

class NoopSpanRecordable : public trace_sdk::Recordable
{
public:
  ~NoopSpanRecordable() override = default;
  void SetIdentity(const trace::SpanContext &span_context,
                   trace::SpanId parent_span_id) noexcept override
  {}
  void SetAttribute(nostd::string_view key,
                    const opentelemetry::common::AttributeValue &value) noexcept override
  {}
  void AddEvent(nostd::string_view name,
                opentelemetry::common::SystemTimestamp timestamp,
                const opentelemetry::common::KeyValueIterable &attributes) noexcept override
  {}
  void AddLink(const trace::SpanContext &span_context,
               const opentelemetry::common::KeyValueIterable &attributes) noexcept override
  {}
  void SetStatus(trace::StatusCode code, nostd::string_view description) noexcept override {}
  void SetName(nostd::string_view name) noexcept override {}
  void SetSpanKind(trace::SpanKind span_kind) noexcept override {}
  void SetResource(const opentelemetry::sdk::resource::Resource &resource) noexcept override {}
  void SetStartTime(opentelemetry::common::SystemTimestamp start_time) noexcept override {}
  void SetDuration(std::chrono::nanoseconds duration) noexcept override {}
  void SetInstrumentationScope(
      const scope_sdk::InstrumentationScope &instrumentation_scope) noexcept override
  {}
};

class NoopSpanExporter : public trace_sdk::SpanExporter
{
public:
  ~NoopSpanExporter() override = default;
  std::unique_ptr<trace_sdk::Recordable> MakeRecordable() noexcept override
  {
    return std::move(std::unique_ptr<trace_sdk::Recordable>{new NoopSpanRecordable()});
  }
  common_sdk::ExportResult Export(
      const nostd::span<std::unique_ptr<trace_sdk::Recordable>> &spans) noexcept override
  {
    return common_sdk::ExportResult::kSuccess;
  }
  bool ForceFlush(
      std::chrono::microseconds timeout = (std::chrono::microseconds::max)()) noexcept override
  {
    return true;
  }
  bool Shutdown(
      std::chrono::microseconds timeout = (std::chrono::microseconds::max)()) noexcept override
  {
    return true;
  }
};

class NoopPushMetricExporter : public metrics_sdk::PushMetricExporter
{
public:
  ~NoopPushMetricExporter() override = default;
  common_sdk::ExportResult Export(
      const metrics_sdk::ResourceMetrics &resource_metrics) noexcept override
  {
    return common_sdk::ExportResult::kSuccess;
  }

  metrics_sdk::AggregationTemporality GetAggregationTemporality(
      metrics_sdk::InstrumentType instrument_type) const noexcept override
  {
    return metrics_sdk::AggregationTemporality::kCumulative;
  }

  bool ForceFlush(
      std::chrono::microseconds timeout = (std::chrono::microseconds::max)()) noexcept override
  {
    return true;
  }
  bool Shutdown(
      std::chrono::microseconds timeout = (std::chrono::microseconds::max)()) noexcept override
  {
    return true;
  }
};

TEST(SdkInstallTest, SdkVersionCheck)
{
  EXPECT_NE(OPENTELEMETRY_SDK_VERSION, "not a version");
  EXPECT_GE(version_sdk::major_version, 0);
  EXPECT_GE(version_sdk::minor_version, 0);
  EXPECT_GE(version_sdk::patch_version, 0);
  EXPECT_NE(version_sdk::full_version, "");
  EXPECT_NE(version_sdk::short_version, "");
}

TEST(SdkInstallTest, ResourceDetectorCheck)
{
  auto resource = resource_sdk::Resource::GetDefault();
  resource_sdk::OTELResourceDetector detector;
  resource.Merge(detector.Detect());
  resource_sdk::ResourceAttributes attributes = resource.GetAttributes();
  EXPECT_NE(attributes.size(), 0);
}

TEST(SdkInstallTest, LoggerProviderCheck)
{
  {
    auto exporter     = nostd::unique_ptr<logs_sdk::LogRecordExporter>(new NoopLogRecordExporter());
    auto processor    = logs_sdk::SimpleLogRecordProcessorFactory::Create(std::move(exporter));
    auto sdk_provider = logs_sdk::LoggerProviderFactory::Create(std::move(processor));
    nostd::shared_ptr<opentelemetry::logs::LoggerProvider> new_provider{sdk_provider.release()};
    logs::Provider::SetLoggerProvider(new_provider);
  }

  auto provider = opentelemetry::logs::Provider::GetLoggerProvider();
  ASSERT_TRUE(provider != nullptr);
  {
    auto logger = provider->GetLogger("test-logger");
    ASSERT_TRUE(logger != nullptr);
    logger->Info("test-message");
  }
  auto sdk_provider = static_cast<logs_sdk::LoggerProvider *>(provider.get());
  sdk_provider->ForceFlush();
}

TEST(SdkInstallTest, TracerProviderCheck)
{
  {
    auto exporter     = nostd::unique_ptr<trace_sdk::SpanExporter>(new NoopSpanExporter());
    auto processor    = trace_sdk::SimpleSpanProcessorFactory::Create(std::move(exporter));
    auto sdk_provider = trace_sdk::TracerProviderFactory::Create(std::move(processor));
    nostd::shared_ptr<trace::TracerProvider> new_provider{sdk_provider.release()};
    trace::Provider::SetTracerProvider(new_provider);
  }

  auto provider = trace::Provider::GetTracerProvider();
  ASSERT_TRUE(provider != nullptr);
  {
    auto tracer = provider->GetTracer("test-tracer");
    ASSERT_TRUE(tracer != nullptr);
    auto span = tracer->StartSpan("test-span");
    ASSERT_TRUE(span != nullptr);
    span->End();
  }
  auto sdk_provider = static_cast<trace_sdk::TracerProvider *>(provider.get());
  sdk_provider->ForceFlush();
}

TEST(SdkInstallTest, MeterProviderCheck)
{
  {
    auto exporter =
        nostd::unique_ptr<metrics_sdk::PushMetricExporter>(new NoopPushMetricExporter());
    auto reader = metrics_sdk::PeriodicExportingMetricReaderFactory::Create(
        std::move(exporter), metrics_sdk::PeriodicExportingMetricReaderOptions{});
    auto context      = metrics_sdk::MeterContextFactory::Create();
    auto sdk_provider = metrics_sdk::MeterProviderFactory::Create(std::move(context));
    sdk_provider->AddMetricReader(std::move(reader));
    nostd::shared_ptr<metrics::MeterProvider> new_provider{sdk_provider.release()};
    metrics::Provider::SetMeterProvider(new_provider);
  }

  auto provider = metrics::Provider::GetMeterProvider();
  ASSERT_TRUE(provider != nullptr);
  {
    auto meter = provider->GetMeter("test-meter");
    ASSERT_TRUE(meter != nullptr);
    auto counter = meter->CreateUInt64Counter("test-counter");
    ASSERT_TRUE(counter != nullptr);
    counter->Add(1);
  }
  auto sdk_provider = static_cast<metrics_sdk::MeterProvider *>(provider.get());
  sdk_provider->ForceFlush();
}