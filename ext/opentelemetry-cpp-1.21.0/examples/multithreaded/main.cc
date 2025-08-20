// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "opentelemetry/exporters/ostream/span_exporter_factory.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/trace/exporter.h"
#include "opentelemetry/sdk/trace/processor.h"
#include "opentelemetry/sdk/trace/provider.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/trace/scope.h"
#include "opentelemetry/trace/span.h"
#include "opentelemetry/trace/tracer.h"
#include "opentelemetry/trace/tracer_provider.h"

namespace trace_api = opentelemetry::trace;
namespace trace_sdk = opentelemetry::sdk::trace;

namespace
{
void InitTracer()
{
  auto exporter  = opentelemetry::exporter::trace::OStreamSpanExporterFactory::Create();
  auto processor = trace_sdk::SimpleSpanProcessorFactory::Create(std::move(exporter));
  std::shared_ptr<opentelemetry::trace::TracerProvider> provider =
      trace_sdk::TracerProviderFactory::Create(std::move(processor),
                                               opentelemetry::sdk::resource::Resource::Create({}));
  // Set the global trace provider
  trace_sdk::Provider::SetTracerProvider(provider);
}

void CleanupTracer()
{
  std::shared_ptr<opentelemetry::trace::TracerProvider> none;
  trace_sdk::Provider::SetTracerProvider(none);
}

opentelemetry::nostd::shared_ptr<trace_api::Tracer> get_tracer()
{
  auto provider = trace_api::Provider::GetTracerProvider();
  return provider->GetTracer("foo_library");
}
}  // namespace

void run_threads()
{
  auto thread_span = get_tracer()->StartSpan(__func__);

  std::vector<std::thread> threads;
  threads.reserve(5);
  for (int thread_num = 0; thread_num < 5; ++thread_num)
  {
    // This shows how one can effectively use Scope objects to correctly
    // parent spans across threads.
    threads.push_back(std::thread([=] {
      trace_api::Scope scope(thread_span);
      auto thread_span_2 =
          get_tracer()->StartSpan(std::string("thread ") + std::to_string(thread_num));
    }));
  }

  std::for_each(threads.begin(), threads.end(), [](std::thread &th) { th.join(); });
}

int main(int /* argc */, char ** /* argv */)
{
  InitTracer();

  {
    auto root_span = get_tracer()->StartSpan(__func__);
    trace_api::Scope scope(root_span);

    run_threads();
  }

  CleanupTracer();
  return 0;
}
