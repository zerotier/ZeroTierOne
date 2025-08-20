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

#include <opentelemetry/exporters/ostream/log_record_exporter.h>
#include <opentelemetry/exporters/ostream/log_record_exporter_factory.h>
#include <opentelemetry/exporters/ostream/metric_exporter_factory.h>
#include <opentelemetry/exporters/ostream/span_exporter_factory.h>

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

TEST(ExtDllInstallTest, LoggerProviderCheck)
{
  {
    auto exporter = std::unique_ptr<logs_sdk::LogRecordExporter>(
        new opentelemetry::exporter::logs::OStreamLogRecordExporter());
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
}

TEST(ExtDllInstallTest, TracerProviderCheck)
{
  {
    auto exporter     = opentelemetry::exporter::trace::OStreamSpanExporterFactory::Create();
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
}

TEST(ExtDllInstallTest, MeterProviderCheck)
{
  {
    auto exporter = opentelemetry::exporter::metrics::OStreamMetricExporterFactory::Create();
    auto reader   = metrics_sdk::PeriodicExportingMetricReaderFactory::Create(
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
}