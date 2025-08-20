// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#ifdef _WIN32

#  include <benchmark/benchmark.h>
#  include <gtest/gtest.h>
#  include <map>
#  include <string>
#  include <unordered_map>

#  include "opentelemetry/exporters/etw/etw_tracer_exporter.h"
#  include "opentelemetry/sdk/trace/simple_processor.h"

using namespace OPENTELEMETRY_NAMESPACE;

using namespace opentelemetry::exporter::etw;

namespace
{
static constexpr const char *providerName = "OpenTelemetry-ETW-StressTest";

static exporter::etw::TelemetryProviderOptions providerOptions = {
    {"enableTraceId", false},
    {"enableSpanId", false},
    {"enableActivityId", false},
    {"enableRelatedActivityId", false},
    {"enableAutoParent", false}};

class ETWProviderStressTest
{
  exporter::etw::TracerProvider provider_;
  std::string mode_;
  nostd::shared_ptr<trace::Tracer> tracer_;
  nostd::shared_ptr<trace::Span> span_;

public:
  /**
   * @brief Construct ETW Provider stress test object
   * @param mode Operational mode: "TLD" or "MsgPack"
   */
  ETWProviderStressTest(std::string mode = "TLD") : mode_(mode), provider_(providerOptions) {}

  /**
   * @brief Initializer tracer and start a Span
   */
  void Initialize()
  {
    tracer_ = provider_.GetTracer(providerName, mode_);
    span_   = tracer_->StartSpan("Span");
  }

  /**
   * @brief Obtain the tracer
   */
  nostd::shared_ptr<trace::Tracer> GetTracer() { return tracer_; }

  /**
   * @brief Add event using Properties container
   */
  bool AddProperties()
  {
    std::string eventName = "MyEvent";
    Properties event      = {{"uint32Key", (uint32_t)1234},
                             {"uint64Key", (uint64_t)1234567890},
                             {"strKey", "someValue"}};
    span_->AddEvent(eventName, event);
    return true;
  }

  /**
   * @brief Add event using static preallocated "reusable" Properties container.
   * This approach works well for single-threaded flows, but may not be safe in
   * some multithreaded scenarios in case if reusable `Properties` get concurrently
   * modified by different threads (writes to Properties are not thread-safe).
   */
  bool AddPropertiesStatic()
  {
    std::string eventName   = "MyEvent";
    static Properties event = {{"uint32Key", (uint32_t)1234},
                               {"uint64Key", (uint64_t)1234567890},
                               {"strKey", "someValue"}};
    span_->AddEvent(eventName, event);
    return true;
  }

  /**
   * @brief Add event using initializer list
   */
  bool AddInitList()
  {
    std::string eventName = "MyEvent";
    span_->AddEvent(eventName, {{"uint32Key", (uint32_t)1234},
                                {"uint64Key", (uint64_t)1234567890},
                                {"strKey", "someValue"}});
    return true;
  }

  /**
   * @brief Add event using unordered_map
   */
  bool AddMap()
  {
    std::string eventName                                      = "MyEvent";
    std::unordered_map<const char *, common::AttributeValue> m = {
        {"uint32Key", (uint32_t)1234},
        {"uint64Key", (uint64_t)1234567890},
        {"strKey", "someValue"}};
    span_->AddEvent(eventName, m);
    return true;
  }

  /**
   * @brief End Span and close tracer.
   */
  void Teardown()
  {
    span_->End();
#  if OPENTELEMETRY_ABI_VERSION_NO == 1
    tracer_->CloseWithMicroseconds(0);
#  endif
  }
};

ETWProviderStressTest provider;

/**
 * @brief Create Properties and AddEvent(Properties) to Tracer
 * @param state Benchmark state.
 */
void BM_AddPropertiesToTracer(benchmark::State &state)
{
  provider.Initialize();
  while (state.KeepRunning())
  {
    benchmark::DoNotOptimize(provider.AddProperties());
  }
  provider.Teardown();
}
BENCHMARK(BM_AddPropertiesToTracer);

/**
 * @brief Create static Properties and AddEvent(Properties) to Tracer
 * @param state Benchmark state.
 */
void BM_AddPropertiesStaticToTracer(benchmark::State &state)
{
  provider.Initialize();
  while (state.KeepRunning())
  {
    benchmark::DoNotOptimize(provider.AddPropertiesStatic());
  }
  provider.Teardown();
}
BENCHMARK(BM_AddPropertiesStaticToTracer);

/**
 * @brief Create event via initializer list and AddEvent({...}) to Tracer
 * @param state Benchmark state.
 */
void BM_AddInitListToTracer(benchmark::State &state)
{
  provider.Initialize();
  while (state.KeepRunning())
  {
    benchmark::DoNotOptimize(provider.AddInitList());
  }
  provider.Teardown();
}
BENCHMARK(BM_AddInitListToTracer);

/**
 * @brief Create event as `std::map<std::string, common::AttributeValue>`
 * and AddEvent(event) to Tracer.
 * @param state Benchmark state.
 */
void BM_AddMapToTracer(benchmark::State &state)
{
  provider.Initialize();
  while (state.KeepRunning())
  {
    benchmark::DoNotOptimize(provider.AddMap());
  }
  provider.Teardown();
}
BENCHMARK(BM_AddMapToTracer);

}  // namespace

BENCHMARK_MAIN();

#endif
