// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>
#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/logs/log_record.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/sdk/common/exporter_utils.h"
#include "opentelemetry/sdk/logs/exporter.h"
#include "opentelemetry/sdk/logs/recordable.h"
#include "opentelemetry/sdk/logs/simple_log_record_processor.h"

using namespace opentelemetry::sdk::logs;
using namespace opentelemetry::sdk::common;
namespace nostd = opentelemetry::nostd;

class TestLogRecordable final : public opentelemetry::sdk::logs::Recordable
{
public:
  void SetTimestamp(opentelemetry::common::SystemTimestamp) noexcept override {}

  void SetObservedTimestamp(opentelemetry::common::SystemTimestamp) noexcept override {}

  void SetSeverity(opentelemetry::logs::Severity) noexcept override {}

  nostd::string_view GetBody() const noexcept { return body_; }

  void SetBody(const opentelemetry::common::AttributeValue &message) noexcept override
  {
    if (nostd::holds_alternative<const char *>(message))
    {
      body_ = nostd::get<const char *>(message);
    }
    else if (nostd::holds_alternative<nostd::string_view>(message))
    {
      body_ = static_cast<std::string>(nostd::get<nostd::string_view>(message));
    }
  }

  void SetBody(const char *message) noexcept { body_ = message; }

  void SetEventId(int64_t, nostd::string_view) noexcept override {}

  void SetTraceId(const opentelemetry::trace::TraceId &) noexcept override {}

  void SetSpanId(const opentelemetry::trace::SpanId &) noexcept override {}

  void SetTraceFlags(const opentelemetry::trace::TraceFlags &) noexcept override {}

  void SetAttribute(nostd::string_view,
                    const opentelemetry::common::AttributeValue &) noexcept override
  {}

  void SetResource(const opentelemetry::sdk::resource::Resource &) noexcept override {}

  void SetInstrumentationScope(
      const opentelemetry::sdk::instrumentationscope::InstrumentationScope &) noexcept override
  {}

private:
  std::string body_;
};

/*
 * A test exporter that can return a vector of all the records it has received,
 * and keep track of the number of times its Shutdown() function was called.
 */
class TestExporter final : public LogRecordExporter
{
public:
  TestExporter(int *force_flush_counter,
               int *shutdown_counter,
               std::shared_ptr<std::vector<std::unique_ptr<TestLogRecordable>>> logs_received,
               size_t *batch_size_received)
      : force_flush_counter_(force_flush_counter),
        shutdown_counter_(shutdown_counter),
        logs_received_(std::move(logs_received)),
        batch_size_received(batch_size_received)
  {}

  std::unique_ptr<Recordable> MakeRecordable() noexcept override
  {
    return std::unique_ptr<Recordable>(new TestLogRecordable());
  }

  // Stores the names of the log records this exporter receives to an internal list
  ExportResult Export(const nostd::span<std::unique_ptr<Recordable>> &records) noexcept override
  {
    *batch_size_received = records.size();
    for (auto &record : records)
    {
      auto log_record =
          std::unique_ptr<TestLogRecordable>(static_cast<TestLogRecordable *>(record.release()));

      if (log_record != nullptr)
      {
        logs_received_->push_back(std::move(log_record));
      }
    }
    return ExportResult::kSuccess;
  }

  bool ForceFlush(std::chrono::microseconds /* timeout */) noexcept override
  {
    if (nullptr != force_flush_counter_)
    {
      ++(*force_flush_counter_);
    }
    return true;
  }

  // Increment the shutdown counter everytime this method is called
  bool Shutdown(std::chrono::microseconds /* timeout */) noexcept override
  {
    if (nullptr != shutdown_counter_)
    {
      *shutdown_counter_ += 1;
    }
    return true;
  }

private:
  int *force_flush_counter_;
  int *shutdown_counter_;
  std::shared_ptr<std::vector<std::unique_ptr<TestLogRecordable>>> logs_received_;
  size_t *batch_size_received;
};

// Tests whether the simple processor successfully creates a batch of size 1
// and whether the contents of the record is sent to the exporter correctly
TEST(SimpleLogRecordProcessorTest, SendReceivedLogsToExporter)
{
  // Create a simple processor with a TestExporter attached
  std::shared_ptr<std::vector<std::unique_ptr<TestLogRecordable>>> logs_received(
      new std::vector<std::unique_ptr<TestLogRecordable>>);
  size_t batch_size_received = 0;

  std::unique_ptr<TestExporter> exporter(
      new TestExporter(nullptr, nullptr, logs_received, &batch_size_received));

  SimpleLogRecordProcessor processor(std::move(exporter));

  // Send some log records to the processor (which should then send to the TestExporter)
  const int num_logs = 5;
  for (int i = 0; i < num_logs; i++)
  {
    auto recordable = processor.MakeRecordable();
    static_cast<TestLogRecordable *>(recordable.get())->SetBody("Log Body");
    processor.OnEmit(std::move(recordable));

    // Verify that the batch of 1 log record sent by processor matches what exporter received
    EXPECT_EQ(1, batch_size_received);
  }

  // Test whether the processor's log sent matches the log record received by the exporter
  EXPECT_EQ(logs_received->size(), num_logs);
  for (int i = 0; i < num_logs; i++)
  {
    EXPECT_EQ("Log Body", logs_received->at(i)->GetBody());
  }
}

// Tests behavior when calling the processor's ShutDown() multiple times
TEST(SimpleLogRecordProcessorTest, ShutdownCalledOnce)
{
  // Create a TestExporter
  int num_shutdowns = 0;

  std::unique_ptr<TestExporter> exporter(
      new TestExporter(nullptr, &num_shutdowns, nullptr, nullptr));

  // Create a processor with the previous test exporter
  SimpleLogRecordProcessor processor(std::move(exporter));

  // The first time processor shutdown is called
  EXPECT_EQ(0, num_shutdowns);
  EXPECT_EQ(true, processor.Shutdown());
  EXPECT_EQ(1, num_shutdowns);

  EXPECT_EQ(true, processor.Shutdown());
  // Processor::ShutDown(), even if called more than once, should only shutdown exporter once
  EXPECT_EQ(1, num_shutdowns);
}

TEST(SimpleLogRecordProcessorTest, ForceFlush)
{
  // Create a TestExporter
  int num_force_flush = 0;

  std::unique_ptr<TestExporter> exporter(
      new TestExporter(&num_force_flush, nullptr, nullptr, nullptr));

  // Create a processor with the previous test exporter
  SimpleLogRecordProcessor processor(std::move(exporter));

  // The first time processor shutdown is called
  EXPECT_EQ(0, num_force_flush);
  EXPECT_EQ(true, processor.ForceFlush());
  EXPECT_EQ(1, num_force_flush);
}

// A test exporter that always returns failure when shut down
class FailShutDownForceFlushExporter final : public LogRecordExporter
{
public:
  FailShutDownForceFlushExporter() {}

  std::unique_ptr<Recordable> MakeRecordable() noexcept override
  {
    return std::unique_ptr<Recordable>(new TestLogRecordable());
  }

  ExportResult Export(
      const nostd::span<std::unique_ptr<Recordable>> & /* records */) noexcept override
  {
    return ExportResult::kSuccess;
  }

  bool ForceFlush(std::chrono::microseconds /* timeout */) noexcept override { return false; }

  bool Shutdown(std::chrono::microseconds /* timeout */) noexcept override { return false; }
};

// Tests for when when processor should fail to shutdown
TEST(SimpleLogRecordProcessorTest, ShutDownFail)
{
  std::unique_ptr<FailShutDownForceFlushExporter> exporter(new FailShutDownForceFlushExporter());
  SimpleLogRecordProcessor processor(std::move(exporter));

  // Expect failure result when exporter fails to shutdown
  EXPECT_EQ(false, processor.Shutdown());
}

// Tests for when when processor should fail to force flush
TEST(SimpleLogRecordProcessorTest, ForceFlushFail)
{
  std::unique_ptr<FailShutDownForceFlushExporter> exporter(new FailShutDownForceFlushExporter());
  SimpleLogRecordProcessor processor(std::move(exporter));

  // Expect failure result when exporter fails to force flush
  EXPECT_EQ(false, processor.ForceFlush());
}
