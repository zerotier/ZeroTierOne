// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>  // IWYU pragma: keep
#include <string>
#include <utility>

#include "opentelemetry/exporters/otlp/otlp_http_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_exporter_options.h"
#include "opentelemetry/exporters/otlp/otlp_http_exporter_runtime_options.h"
#include "opentelemetry/exporters/otlp/otlp_http_log_record_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_log_record_exporter_options.h"
#include "opentelemetry/exporters/otlp/otlp_http_log_record_exporter_runtime_options.h"
#include "opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_metric_exporter_options.h"
#include "opentelemetry/exporters/otlp/otlp_http_metric_exporter_runtime_options.h"
#include "opentelemetry/logs/logger_provider.h"
#include "opentelemetry/metrics/meter_provider.h"
#include "opentelemetry/sdk/logs/batch_log_record_processor_factory.h"
#include "opentelemetry/sdk/logs/batch_log_record_processor_options.h"
#include "opentelemetry/sdk/logs/batch_log_record_processor_runtime_options.h"
#include "opentelemetry/sdk/logs/exporter.h"
#include "opentelemetry/sdk/logs/logger_provider.h"
#include "opentelemetry/sdk/logs/logger_provider_factory.h"
#include "opentelemetry/sdk/logs/processor.h"
#include "opentelemetry/sdk/logs/provider.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_options.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_runtime_options.h"
#include "opentelemetry/sdk/metrics/meter_context.h"
#include "opentelemetry/sdk/metrics/meter_context_factory.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/meter_provider_factory.h"
#include "opentelemetry/sdk/metrics/metric_reader.h"
#include "opentelemetry/sdk/metrics/provider.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"
#include "opentelemetry/sdk/trace/batch_span_processor_factory.h"
#include "opentelemetry/sdk/trace/batch_span_processor_options.h"
#include "opentelemetry/sdk/trace/batch_span_processor_runtime_options.h"
#include "opentelemetry/sdk/trace/exporter.h"
#include "opentelemetry/sdk/trace/processor.h"
#include "opentelemetry/sdk/trace/provider.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/trace/tracer_provider.h"

#ifdef BAZEL_BUILD
#  include "examples/common/logs_foo_library/foo_library.h"
#  include "examples/common/metrics_foo_library/foo_library.h"
#else
#  include "logs_foo_library/foo_library.h"
#  include "metrics_foo_library/foo_library.h"
#endif

namespace
{

#ifdef ENABLE_THREAD_INSTRUMENTATION_PREVIEW

std::mutex serialize;

/**
 The purpose of MyThreadInstrumentation is to demonstrate
 how notifications are delivered to the application.

 Printing to std::cout is useful for debugging,
 to understand the overall thread execution in the library.

 In production, a real application would instead:
 - set thread priorities / CPU affinity
 - set thread local storage keys
 - set a thread name to the operating system
 - set network namespaces
 in the OnXXX() code here.
*/
class MyThreadInstrumentation : public opentelemetry::sdk::common::ThreadInstrumentation
{
public:
  MyThreadInstrumentation(const std::string &thread_name,
                          const std::string &network_name,
                          const std::string &priority)
      : thread_name_(thread_name), network_name_(network_name), priority_(priority)
  {}
  ~MyThreadInstrumentation() override = default;

  void OnStart() override
  {
    std::lock_guard<std::mutex> lock_guard(serialize);
    std::cout << "OnStart() thread " << thread_name_ << ", id " << std::this_thread::get_id();
    if (!network_name_.empty())
    {
      std::cout << ", network_name " << network_name_;
    }
    if (!priority_.empty())
    {
      std::cout << ", priority " << priority_;
    }
    std::cout << std::endl << std::flush;
  }

  void OnEnd() override
  {
    std::lock_guard<std::mutex> lock_guard(serialize);
    std::cout << "OnEnd() thread " << thread_name_ << ", id " << std::this_thread::get_id()
              << std::endl
              << std::flush;
  }

  void BeforeWait() override
  {
    std::lock_guard<std::mutex> lock_guard(serialize);
    std::cout << "BeforeWait() thread " << thread_name_ << ", id " << std::this_thread::get_id()
              << ", waiting" << std::endl
              << std::flush;
  }

  void AfterWait() override
  {
    std::lock_guard<std::mutex> lock_guard(serialize);
    std::cout << "AfterWait() thread " << thread_name_ << ", id " << std::this_thread::get_id()
              << ", done waiting" << std::endl
              << std::flush;
  }

  void BeforeLoad() override
  {
    std::lock_guard<std::mutex> lock_guard(serialize);
    std::cout << "BeforeLoad() thread " << thread_name_ << ", id " << std::this_thread::get_id()
              << ", about to work" << std::endl
              << std::flush;
  }

  void AfterLoad() override
  {
    std::lock_guard<std::mutex> lock_guard(serialize);
    std::cout << "AfterLoad() thread " << thread_name_ << ", id " << std::this_thread::get_id()
              << ", done working" << std::endl
              << std::flush;
  }

private:
  std::string thread_name_;
  std::string network_name_;
  std::string priority_;
};

#endif /* ENABLE_THREAD_INSTRUMENTATION_PREVIEW */

opentelemetry::exporter::otlp::OtlpHttpExporterOptions tracer_opts;
opentelemetry::exporter::otlp::OtlpHttpMetricExporterOptions meter_opts;
opentelemetry::exporter::otlp::OtlpHttpLogRecordExporterOptions logger_opts;

std::shared_ptr<opentelemetry::sdk::trace::TracerProvider> tracer_provider;
std::shared_ptr<opentelemetry::sdk::metrics::MeterProvider> meter_provider;
std::shared_ptr<opentelemetry::sdk::logs::LoggerProvider> logger_provider;

void InitTracer()
{
  // Create OTLP exporter instance
  opentelemetry::exporter::otlp::OtlpHttpExporterRuntimeOptions exp_rt_opts;
#ifdef ENABLE_THREAD_INSTRUMENTATION_PREVIEW
  auto exp_instr = std::shared_ptr<opentelemetry::sdk::common::ThreadInstrumentation>(
      new MyThreadInstrumentation("OtlpHttpExporter", "trace-net", "high"));
  exp_rt_opts.thread_instrumentation = exp_instr;
#endif /* ENABLE_THREAD_INSTRUMENTATION_PREVIEW */
  auto exporter =
      opentelemetry::exporter::otlp::OtlpHttpExporterFactory::Create(tracer_opts, exp_rt_opts);

  // Create Processor instance
  opentelemetry::sdk::trace::BatchSpanProcessorOptions pro_opts;
  opentelemetry::sdk::trace::BatchSpanProcessorRuntimeOptions pro_rt_opts;
#ifdef ENABLE_THREAD_INSTRUMENTATION_PREVIEW
  auto pro_instr = std::shared_ptr<opentelemetry::sdk::common::ThreadInstrumentation>(
      new MyThreadInstrumentation("BatchSpanProcessor", "", "high"));
  pro_rt_opts.thread_instrumentation = pro_instr;
#endif /* ENABLE_THREAD_INSTRUMENTATION_PREVIEW */
  auto processor = opentelemetry::sdk::trace::BatchSpanProcessorFactory::Create(
      std::move(exporter), pro_opts, pro_rt_opts);

  // Create Provider instance
  tracer_provider = opentelemetry::sdk::trace::TracerProviderFactory::Create(std::move(processor));

  // Set the global trace provider
  std::shared_ptr<opentelemetry::trace::TracerProvider> api_provider = tracer_provider;
  opentelemetry::sdk::trace::Provider::SetTracerProvider(api_provider);
}

void CleanupTracer()
{
  // We call ForceFlush to prevent to cancel running exportings, It's optional.
  if (tracer_provider)
  {
    tracer_provider->ForceFlush();
    tracer_provider->Shutdown();
  }

  tracer_provider.reset();
  std::shared_ptr<opentelemetry::trace::TracerProvider> none;
  opentelemetry::sdk::trace::Provider::SetTracerProvider(none);
}

void InitMetrics()
{
  // Create OTLP exporter instance
  opentelemetry::exporter::otlp::OtlpHttpMetricExporterRuntimeOptions exp_rt_opts;
#ifdef ENABLE_THREAD_INSTRUMENTATION_PREVIEW
  auto exp_instr = std::shared_ptr<opentelemetry::sdk::common::ThreadInstrumentation>(
      new MyThreadInstrumentation("OtlpHttpMetricExporter", "metric-net", "medium"));
  exp_rt_opts.thread_instrumentation = exp_instr;
#endif /* ENABLE_THREAD_INSTRUMENTATION_PREVIEW */
  auto exporter =
      opentelemetry::exporter::otlp::OtlpHttpMetricExporterFactory::Create(meter_opts, exp_rt_opts);

  std::string version{"1.2.0"};
  std::string schema{"https://opentelemetry.io/schemas/1.2.0"};

  // Initialize and set the global MeterProvider
  opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions reader_options;
  reader_options.export_interval_millis = std::chrono::milliseconds(1000);
  reader_options.export_timeout_millis  = std::chrono::milliseconds(500);

  opentelemetry::sdk::metrics::PeriodicExportingMetricReaderRuntimeOptions reader_rt_opts;
#ifdef ENABLE_THREAD_INSTRUMENTATION_PREVIEW
  auto reader_periodic_instr = std::shared_ptr<opentelemetry::sdk::common::ThreadInstrumentation>(
      new MyThreadInstrumentation("PeriodicExportingMetricReader(periodic)", "", "medium"));
  auto reader_collect_instr = std::shared_ptr<opentelemetry::sdk::common::ThreadInstrumentation>(
      new MyThreadInstrumentation("PeriodicExportingMetricReader(collect)", "", "medium"));
  reader_rt_opts.periodic_thread_instrumentation = reader_periodic_instr;
  reader_rt_opts.collect_thread_instrumentation  = reader_collect_instr;
#endif /* ENABLE_THREAD_INSTRUMENTATION_PREVIEW */
  auto reader = opentelemetry::sdk::metrics::PeriodicExportingMetricReaderFactory::Create(
      std::move(exporter), reader_options, reader_rt_opts);

  auto context = opentelemetry::sdk::metrics::MeterContextFactory::Create();
  context->AddMetricReader(std::move(reader));

  meter_provider = opentelemetry::sdk::metrics::MeterProviderFactory::Create(std::move(context));
  std::shared_ptr<opentelemetry::metrics::MeterProvider> api_provider = meter_provider;

  opentelemetry::sdk::metrics::Provider::SetMeterProvider(api_provider);
}

void CleanupMetrics()
{
  // We call ForceFlush to prevent to cancel running exportings, It's optional.
  if (meter_provider)
  {
    meter_provider->ForceFlush();
    meter_provider->Shutdown();
  }

  meter_provider.reset();
  std::shared_ptr<opentelemetry::metrics::MeterProvider> none;
  opentelemetry::sdk::metrics::Provider::SetMeterProvider(none);
}

void InitLogger()
{
  // Create OTLP exporter instance
  opentelemetry::exporter::otlp::OtlpHttpLogRecordExporterRuntimeOptions exp_rt_opts;
#ifdef ENABLE_THREAD_INSTRUMENTATION_PREVIEW
  auto exp_instr = std::shared_ptr<opentelemetry::sdk::common::ThreadInstrumentation>(
      new MyThreadInstrumentation("OtlpHttpLogRecordExporter", "log-net", "low"));
  exp_rt_opts.thread_instrumentation = exp_instr;
#endif /* ENABLE_THREAD_INSTRUMENTATION_PREVIEW */
  auto exporter = opentelemetry::exporter::otlp::OtlpHttpLogRecordExporterFactory::Create(
      logger_opts, exp_rt_opts);

  // Create Processor instance
  opentelemetry::sdk::logs::BatchLogRecordProcessorOptions pro_opts;
  opentelemetry::sdk::logs::BatchLogRecordProcessorRuntimeOptions pro_rt_opts;
#ifdef ENABLE_THREAD_INSTRUMENTATION_PREVIEW
  auto pro_instr = std::shared_ptr<opentelemetry::sdk::common::ThreadInstrumentation>(
      new MyThreadInstrumentation("BatchLogRecordProcessor", "", "low"));
  pro_rt_opts.thread_instrumentation = pro_instr;
#endif /* ENABLE_THREAD_INSTRUMENTATION_PREVIEW */
  auto processor = opentelemetry::sdk::logs::BatchLogRecordProcessorFactory::Create(
      std::move(exporter), pro_opts, pro_rt_opts);

  logger_provider = opentelemetry::sdk::logs::LoggerProviderFactory::Create(std::move(processor));

  std::shared_ptr<opentelemetry::logs::LoggerProvider> api_provider = logger_provider;
  opentelemetry::sdk::logs::Provider::SetLoggerProvider(api_provider);
}

void CleanupLogger()
{
  // We call ForceFlush to prevent to cancel running exportings, It's optional.
  if (logger_provider)
  {
    logger_provider->ForceFlush();
    logger_provider->Shutdown();
  }

  logger_provider.reset();
  std::shared_ptr<opentelemetry::logs::LoggerProvider> none;
  opentelemetry::sdk::logs::Provider::SetLoggerProvider(none);
}

}  // namespace

/*
  Usage:
  - example_otlp_instrumented_http
  - example_otlp_instrumented_http <TRACE_URL> <METRIC_URL> <LOG_URL>
*/
int main(int argc, char *argv[])
{
  if (argc > 1)
  {
    tracer_opts.url = argv[1];
  }
  else
  {
    tracer_opts.url = "http://localhost:4318/v1/traces";
  }

  if (argc > 2)
  {
    meter_opts.url = argv[2];
  }
  else
  {
    meter_opts.url = "http://localhost:4318/v1/metrics";
  }

  if (argc > 3)
  {
    logger_opts.url = argv[3];
  }
  else
  {
    logger_opts.url = "http://localhost:4318/v1/logs";
  }

  std::cout << "Initializing opentelemetry-cpp" << std::endl << std::flush;

  InitTracer();
  InitMetrics();
  InitLogger();

  std::cout << "Application payload" << std::endl << std::flush;

  foo_library();

  std::string name{"otlp_http_metric_example"};
  foo_library::observable_counter_example(name);

  std::cout << "Shutting down opentelemetry-cpp" << std::endl << std::flush;

  CleanupLogger();
  CleanupMetrics();
  CleanupTracer();

  std::cout << "Done" << std::endl << std::flush;
  return 0;
}
