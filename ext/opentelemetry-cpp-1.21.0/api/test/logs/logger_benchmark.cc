// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <benchmark/benchmark.h>
#include <stdint.h>
#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "opentelemetry/common/key_value_iterable_view.h"
#include "opentelemetry/logs/event_id.h"
#include "opentelemetry/logs/logger.h"
#include "opentelemetry/logs/logger_provider.h"
#include "opentelemetry/logs/provider.h"
#include "opentelemetry/logs/severity.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/string_view.h"

using opentelemetry::logs::EventId;
using opentelemetry::logs::Provider;
using opentelemetry::logs::Severity;

namespace
{

constexpr int64_t kMaxIterations = 1000000000;

class Barrier
{
public:
  explicit Barrier(std::size_t iCount) : mThreshold(iCount), mCount(iCount) {}

  void Wait()
  {
    std::unique_lock<std::mutex> lLock{mMutex};
    auto lGen = mGeneration;
    if (!--mCount)
    {
      mGeneration++;
      mCount = mThreshold;
      mCond.notify_all();
    }
    else
    {
      mCond.wait(lLock, [this, lGen] { return lGen != mGeneration; });
    }
  }

private:
  std::mutex mMutex;
  std::condition_variable mCond;
  std::size_t mThreshold;
  std::size_t mCount;
  std::size_t mGeneration{0};
};

static void ThreadRoutine(Barrier &barrier,
                          benchmark::State &state,
                          int thread_id,
                          const std::function<void()> &func)
{
  barrier.Wait();

  if (thread_id == 0)
  {
    state.ResumeTiming();
  }

  barrier.Wait();

  func();

  if (thread_id == 0)
  {
    state.PauseTiming();
  }

  barrier.Wait();
}

void MultiThreadRunner(benchmark::State &state, const std::function<void()> &func)
{
  uint32_t num_threads = std::thread::hardware_concurrency();

  Barrier barrier(num_threads);

  std::vector<std::thread> threads;

  threads.reserve(num_threads);
  for (uint32_t i = 0; i < num_threads; i++)
  {
    threads.emplace_back(ThreadRoutine, std::ref(barrier), std::ref(state), i, func);
  }

  for (auto &thread : threads)
  {
    thread.join();
  }
}

static void BM_UnstructuredLog(benchmark::State &state)
{
  auto lp     = Provider::GetLoggerProvider();
  auto logger = lp->GetLogger("UnstructuredLog");

  for (auto _ : state)
  {
    state.PauseTiming();

    MultiThreadRunner(state, [&logger]() {
      for (int64_t i = 0; i < kMaxIterations; i++)
      {
        logger->Trace("This is a simple unstructured log message");
      }
    });

    state.ResumeTiming();
  }
}
BENCHMARK(BM_UnstructuredLog);

static void BM_StructuredLogWithTwoAttributes(benchmark::State &state)
{
  auto lp     = Provider::GetLoggerProvider();
  auto logger = lp->GetLogger("StructuredLog");

  for (auto _ : state)
  {
    state.PauseTiming();

    MultiThreadRunner(state, [&logger]() {
      for (int64_t i = 0; i < kMaxIterations; i++)
      {
        logger->Trace(
            "This is a simple structured log message from {process_id}:{thread_id}",
            opentelemetry::common::MakeAttributes({{"process_id", 12347}, {"thread_id", 12348}}));
      }
    });

    state.ResumeTiming();
  }
}
BENCHMARK(BM_StructuredLogWithTwoAttributes);

static void BM_StructuredLogWithEventIdAndTwoAttributes(benchmark::State &state)
{
  auto lp     = Provider::GetLoggerProvider();
  auto logger = lp->GetLogger("StructuredLogWithEventId");

  for (auto _ : state)
  {
    state.PauseTiming();

    MultiThreadRunner(state, [&logger]() {
      for (int64_t i = 0; i < kMaxIterations; i++)
      {
        logger->Trace(
            0x12345678, "This is a simple structured log message from {process_id}:{thread_id}",
            opentelemetry::common::MakeAttributes({{"process_id", 12347}, {"thread_id", 12348}}));
      }
    });

    state.ResumeTiming();
  }
}
BENCHMARK(BM_StructuredLogWithEventIdAndTwoAttributes);

static void BM_StructuredLogWithEventIdStructAndTwoAttributes(benchmark::State &state)
{
  auto lp     = Provider::GetLoggerProvider();
  auto logger = lp->GetLogger("StructuredLogWithEventId");

  const EventId function_name_event_id{0x12345678, "Company.Component.SubComponent.FunctionName"};

  for (auto _ : state)
  {
    state.PauseTiming();

    MultiThreadRunner(state, [&logger, &function_name_event_id]() {
      for (int64_t i = 0; i < kMaxIterations; i++)
      {
        logger->Trace(
            function_name_event_id,
            "Simulate function enter trace message from {process_id}:{thread_id}",
            opentelemetry::common::MakeAttributes({{"process_id", 12347}, {"thread_id", 12348}}));
      }
    });

    state.ResumeTiming();
  }
}
BENCHMARK(BM_StructuredLogWithEventIdStructAndTwoAttributes);

static void BM_EnabledOnSeverityReturnFalse(benchmark::State &state)
{
  auto lp     = Provider::GetLoggerProvider();
  auto logger = lp->GetLogger("UnstructuredLogWithEnabledReturnFalse");

  for (auto _ : state)
  {
    state.PauseTiming();

    MultiThreadRunner(state, [&logger]() {
      for (int64_t i = 0; i < kMaxIterations; i++)
      {
        if (logger->Enabled(Severity::kTrace))
        {
          logger->Trace("This log line would not be called");
        }
      }
    });

    state.ResumeTiming();
  }
}
BENCHMARK(BM_EnabledOnSeverityReturnFalse);

static void BM_EnabledOnSeverityAndEventIdReturnFalse(benchmark::State &state)
{
  auto lp     = Provider::GetLoggerProvider();
  auto logger = lp->GetLogger("UnstructuredLogWithEnabledReturnFalse");

  for (auto _ : state)
  {
    state.PauseTiming();

    MultiThreadRunner(state, [&logger]() {
      for (int64_t i = 0; i < kMaxIterations; i++)
      {
        if (logger->Enabled(Severity::kTrace, 0x12345678))
        {
          logger->Trace("This log line would not be called");
        }
      }
    });

    state.ResumeTiming();
  }
}
BENCHMARK(BM_EnabledOnSeverityAndEventIdReturnFalse);

}  // namespace

int main(int argc, char **argv)
{
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
}
