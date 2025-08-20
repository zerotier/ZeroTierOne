// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <chrono>
#include <memory>
#include <utility>
#include <vector>

#include "opentelemetry/sdk/logs/multi_log_record_processor.h"
#include "opentelemetry/sdk/logs/multi_recordable.h"
#include "opentelemetry/sdk/logs/processor.h"
#include "opentelemetry/sdk/logs/recordable.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace logs
{

MultiLogRecordProcessor::MultiLogRecordProcessor(
    std::vector<std::unique_ptr<LogRecordProcessor>> &&processors)
{
  for (auto &processor : processors)
  {
    AddProcessor(std::move(processor));
  }
}
MultiLogRecordProcessor::~MultiLogRecordProcessor()
{
  ForceFlush();
  Shutdown();
}

void MultiLogRecordProcessor::AddProcessor(std::unique_ptr<LogRecordProcessor> &&processor)
{
  // Add preocessor to end of the list.
  if (processor)
  {
    processors_.emplace_back(std::move(processor));
  }
}

std::unique_ptr<Recordable> MultiLogRecordProcessor::MakeRecordable() noexcept
{
  auto recordable       = std::unique_ptr<Recordable>(new MultiRecordable());
  auto multi_recordable = static_cast<MultiRecordable *>(recordable.get());
  for (auto &processor : processors_)
  {
    multi_recordable->AddRecordable(*processor, processor->MakeRecordable());
  }
  return recordable;
}

void MultiLogRecordProcessor::OnEmit(std::unique_ptr<Recordable> &&record) noexcept
{
  if (!record)
  {
    return;
  }
  auto multi_recordable = static_cast<MultiRecordable *>(record.get());

  for (auto &processor : processors_)
  {
    auto recordable = multi_recordable->ReleaseRecordable(*processor);
    if (recordable)
    {
      processor->OnEmit(std::move(recordable));
    }
  }
}

bool MultiLogRecordProcessor::ForceFlush(std::chrono::microseconds timeout) noexcept
{
  // Convert to nanos to prevent overflow
  std::chrono::nanoseconds timeout_ns = std::chrono::nanoseconds::max();
  if (std::chrono::duration_cast<std::chrono::microseconds>(timeout_ns) > timeout)
  {
    timeout_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(timeout);
  }
  bool result           = true;
  auto start_time       = std::chrono::system_clock::now();
  auto overflow_checker = std::chrono::system_clock::time_point::max();
  std::chrono::system_clock::time_point expire_time;
  if (overflow_checker - start_time <= timeout_ns)
  {
    expire_time = overflow_checker;
  }
  else
  {
    expire_time =
        start_time + std::chrono::duration_cast<std::chrono::system_clock::duration>(timeout_ns);
  }
  for (auto &processor : processors_)
  {
    if (!processor->ForceFlush(std::chrono::duration_cast<std::chrono::microseconds>(timeout_ns)))
    {
      result = false;
    }
    start_time = std::chrono::system_clock::now();
    if (expire_time > start_time)
    {
      timeout_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(expire_time - start_time);
    }
    else
    {
      timeout_ns = std::chrono::nanoseconds::zero();
    }
  }
  return result;
}

bool MultiLogRecordProcessor::Shutdown(std::chrono::microseconds timeout) noexcept
{
  // Converto nanos to prevent overflow
  std::chrono::nanoseconds timeout_ns = std::chrono::nanoseconds::max();
  if (std::chrono::duration_cast<std::chrono::microseconds>(timeout_ns) > timeout)
  {
    timeout_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(timeout);
  }
  bool result           = true;
  auto start_time       = std::chrono::system_clock::now();
  auto overflow_checker = std::chrono::system_clock::time_point::max();
  std::chrono::system_clock::time_point expire_time;
  if (overflow_checker - start_time <= timeout_ns)
  {
    expire_time = overflow_checker;
  }
  else
  {
    expire_time =
        start_time + std::chrono::duration_cast<std::chrono::system_clock::duration>(timeout_ns);
  }
  for (auto &processor : processors_)
  {
    result |=
        processor->Shutdown(std::chrono::duration_cast<std::chrono::microseconds>(timeout_ns));
    start_time = std::chrono::system_clock::now();
    if (expire_time > start_time)
    {
      timeout_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(expire_time - start_time);
    }
    else
    {
      timeout_ns = std::chrono::nanoseconds::zero();
    }
  }
  return result;
}

}  // namespace logs
}  // namespace sdk

OPENTELEMETRY_END_NAMESPACE
