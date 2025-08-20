// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

// Include common Trace Provider API and processor
#include "opentelemetry/sdk/trace/simple_processor.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"
#include "opentelemetry/trace/provider.h"

#include "opentelemetry/exporters/etw/etw_tracer_exporter.h"

#include <iostream>
#include <thread>

#include <cstdio>
#ifndef LOG_DEBUG
#  include <mutex>

/**
 * @brief Thread-safe logger routine.
 * @param format
 * @param args
 * @return
 */
static inline int log_vprintf(const char *format, va_list args)
{
  static std::mutex cout_mutex;
  std::lock_guard<std::mutex> lk(cout_mutex);
  return vprintf(format, args);
};

/**
 * @brief Thread-safe logger routine.
 * @param format
 * @param
 * @return
 */
static inline int log_printf(const char *format, ...)
{
  int result;
  va_list ap;
  va_start(ap, format);
  result = log_vprintf(format, ap);
  va_end(ap);
  return result;
};

// Debug macros
#  define LOG_DEBUG(fmt_, ...) log_printf(" " fmt_ "\n", ##__VA_ARGS__)
#  define LOG_TRACE(fmt_, ...) log_printf(" " fmt_ "\n", ##__VA_ARGS__)
#  define LOG_INFO(fmt_, ...) log_printf(" " fmt_ "\n", ##__VA_ARGS__)
#  define LOG_WARN(fmt_, ...) log_printf(" " fmt_ "\n", ##__VA_ARGS__)
#  define LOG_ERROR(fmt_, ...) log_printf(" " fmt_ "\n", ##__VA_ARGS__)
#endif

using namespace OPENTELEMETRY_NAMESPACE;

using namespace opentelemetry::exporter::etw;

// Specify destination ETW Provider Name or GUID.
//
// In Visual Studio:
// - View -> Other Windows -> Diagnostic Events...
// - Click Configure (gear on top).
// - Specify "OpenTelemetry-ETW-TLD" in the list of providers.
//
// Event view shows event flow in realtime.
const char *kGlobalProviderName = "OpenTelemetry-ETW-TLD";

std::string providerName = kGlobalProviderName;

// One provider can be used to associate with different destinations.
exporter::etw::TracerProvider provider;

// Code below is generic and may be reused with any other TracerProvider.
// In ETW provider case - instrumentation name must match destination
// ETW provider name.
nostd::shared_ptr<trace::Tracer> tracer = provider.GetTracer(providerName, "1.0");

// Obtain numeric thread id
static inline uint64_t gettid()
{
  std::stringstream ss;
  ss << std::this_thread::get_id();
  uint64_t id = std::stoull(ss.str());
  return id;
}

// bop gets called by beep.
void bop()
{
  LOG_TRACE("bop worker tid=%u", gettid());
  auto span = tracer->StartSpan("bop");
  span->AddEvent("BopEvent", {{"tid", gettid()}});
  span->SetAttribute("attrib", 2);
  span->End();
}

// beep gets called in its own thread.
// It is running in a thread pool, invoked via lambda by dispatcher.
void beep()
{
  LOG_TRACE("beep worker tid=%u", gettid());
  auto span = tracer->StartSpan("beep");
  span->AddEvent("BeepEvent", {{"tid", gettid()}});
  span->SetAttribute("attrib", 1);
  {
    auto bopScope = tracer->WithActiveSpan(span);
    bop();
  }
  span->End();
}

// Max number of threads to spawn
constexpr int kMaxThreads = 10;

int main(int /*arc*/, char ** /*argv*/)
{
  std::thread pool[kMaxThreads];

  // Main dispatcher span: parent of all child thread spans.
  auto mainSpan = tracer->StartSpan("beep_bop");
  mainSpan->SetAttribute("attrib", 0);

  // Start several threads to perform beep-bop actions.
  LOG_TRACE("beep-boop dispatcher tid=%u", gettid());
  for (auto i = 0; i < kMaxThreads; i++)
  {
    pool[i] = std::thread([&]() {
      // This code runs in its own worker thread.
      auto beepScope = tracer->WithActiveSpan(mainSpan);
      // call beep -> bop
      beep();
    });
  };

  // Join all worker threads in the pool
  for (auto &th : pool)
  {
    try
    {
      if (th.joinable())
        th.join();
    }
    catch (...)
    {
      // thread might have been gracefully terminated already
    }
  }

  // End span
  mainSpan->End();

  return 0;
}
