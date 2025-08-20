// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <utility>

#include "opentelemetry/common/spin_lock_mutex.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/sdk/common/exporter_utils.h"
#include "opentelemetry/sdk/logs/exporter.h"
#include "opentelemetry/sdk/logs/recordable.h"
#include "opentelemetry/sdk/logs/simple_log_record_processor.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace logs
{
/**
 * Initialize a simple log processor.
 * @param exporter the configured exporter where log records are sent
 */
SimpleLogRecordProcessor::SimpleLogRecordProcessor(std::unique_ptr<LogRecordExporter> &&exporter)
    : exporter_(std::move(exporter)), is_shutdown_(false)
{}

SimpleLogRecordProcessor::~SimpleLogRecordProcessor() {}

std::unique_ptr<Recordable> SimpleLogRecordProcessor::MakeRecordable() noexcept
{
  return exporter_->MakeRecordable();
}

/**
 * Batches the log record it receives in a batch of 1 and immediately sends it
 * to the configured exporter
 */
void SimpleLogRecordProcessor::OnEmit(std::unique_ptr<Recordable> &&record) noexcept
{
  nostd::span<std::unique_ptr<Recordable>> batch(&record, 1);
  // Get lock to ensure Export() is never called concurrently
  const std::lock_guard<opentelemetry::common::SpinLockMutex> locked(lock_);

  if (exporter_->Export(batch) != sdk::common::ExportResult::kSuccess)
  {
    /* Alert user of the failed export */
  }
}
/**
 *  The simple processor does not have any log records to flush so this method is not used
 */
bool SimpleLogRecordProcessor::ForceFlush(std::chrono::microseconds timeout) noexcept
{
  if (exporter_ != nullptr)
  {
    return exporter_->ForceFlush(timeout);
  }
  return true;
}

bool SimpleLogRecordProcessor::Shutdown(std::chrono::microseconds timeout) noexcept
{
  // Should only shutdown exporter ONCE.
  if (!is_shutdown_.exchange(true, std::memory_order_acq_rel) && exporter_ != nullptr)
  {
    return exporter_->Shutdown(timeout);
  }

  return true;
}

bool SimpleLogRecordProcessor::IsShutdown() const noexcept
{
  return is_shutdown_.load(std::memory_order_acquire);
}

}  // namespace logs
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
