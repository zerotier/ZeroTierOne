// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chrono>
#include <memory>

#include "opentelemetry/nostd/span.h"
#include "opentelemetry/sdk/common/exporter_utils.h"
#include "opentelemetry/sdk/logs/recordable.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace logs
{
/**
 * LogRecordExporter defines the interface that log exporters must implement.
 */
class OPENTELEMETRY_EXPORT LogRecordExporter
{
public:
  LogRecordExporter();
  virtual ~LogRecordExporter();

  /**
   * Create a log recordable. This object will be used to record log data and
   * will subsequently be passed to LogRecordExporter::Export. Vendors can implement
   * custom recordables or use the default ReadWriteLogRecord recordable provided by the
   * SDK.
   * @return a newly initialized Recordable object
   *
   * Note: This method must be callable from multiple threads.
   */
  virtual std::unique_ptr<Recordable> MakeRecordable() noexcept = 0;

  /**
   * Exports the batch of log records to their export destination.
   * This method must not be called concurrently for the same exporter instance.
   * The exporter may attempt to retry sending the batch, but should drop
   * and return kFailure after a certain timeout.
   * @param records a span of unique pointers to log records
   * @returns an ExportResult code (whether export was success or failure)
   */
  virtual sdk::common::ExportResult Export(
      const nostd::span<std::unique_ptr<Recordable>> &records) noexcept = 0;

  /**
   * Force flush the log records pushed into this log exporter.
   */
  virtual bool ForceFlush(
      std::chrono::microseconds timeout = (std::chrono::microseconds::max)()) noexcept = 0;

  /**
   * Marks the exporter as ShutDown and cleans up any resources as required.
   * Shutdown should be called only once for each Exporter instance.
   * @param timeout minimum amount of microseconds to wait for shutdown before giving up and
   * returning failure.
   * @return true if the exporter shutdown succeeded, false otherwise
   */
  virtual bool Shutdown(
      std::chrono::microseconds timeout = (std::chrono::microseconds::max)()) noexcept = 0;
};
}  // namespace logs
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
