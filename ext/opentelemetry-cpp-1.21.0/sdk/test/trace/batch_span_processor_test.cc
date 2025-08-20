// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/sdk/common/attribute_utils.h"
#include "opentelemetry/sdk/common/exporter_utils.h"
#include "opentelemetry/sdk/common/global_log_handler.h"
#include "opentelemetry/sdk/trace/batch_span_processor.h"
#include "opentelemetry/sdk/trace/batch_span_processor_options.h"
#include "opentelemetry/sdk/trace/exporter.h"
#include "opentelemetry/sdk/trace/processor.h"
#include "opentelemetry/sdk/trace/recordable.h"
#include "opentelemetry/sdk/trace/span_data.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE

/**
 * Returns a mock span exporter meant exclusively for testing only
 */
class MockSpanExporter final : public sdk::trace::SpanExporter
{
public:
  MockSpanExporter(
      std::shared_ptr<std::vector<std::unique_ptr<sdk::trace::SpanData>>> spans_received,
      std::shared_ptr<std::atomic<std::size_t>> shut_down_counter,
      std::shared_ptr<std::atomic<bool>> is_shutdown,
      std::shared_ptr<std::atomic<bool>> is_export_completed =
          std::shared_ptr<std::atomic<bool>>(new std::atomic<bool>(false)),
      const std::chrono::milliseconds export_delay = std::chrono::milliseconds(0)) noexcept
      : spans_received_(std::move(spans_received)),
        shut_down_counter_(std::move(shut_down_counter)),
        is_shutdown_(std::move(is_shutdown)),
        is_export_completed_(std::move(is_export_completed)),
        export_delay_(export_delay)
  {}

  std::unique_ptr<sdk::trace::Recordable> MakeRecordable() noexcept override
  {
    return std::unique_ptr<sdk::trace::Recordable>(new sdk::trace::SpanData);
  }

  sdk::common::ExportResult Export(
      const nostd::span<std::unique_ptr<sdk::trace::Recordable>> &recordables) noexcept override
  {
    *is_export_completed_ = false;

    std::this_thread::sleep_for(export_delay_);

    for (auto &recordable : recordables)
    {
      auto span = std::unique_ptr<sdk::trace::SpanData>(
          static_cast<sdk::trace::SpanData *>(recordable.release()));

      if (span != nullptr)
      {
        spans_received_->push_back(std::move(span));
      }
    }

    *is_export_completed_ = true;
    return sdk::common::ExportResult::kSuccess;
  }

  bool ForceFlush(std::chrono::microseconds /*timeout*/) noexcept override
  {
    ++(*shut_down_counter_);
    return true;
  }

  bool Shutdown(std::chrono::microseconds /* timeout */) noexcept override
  {
    *is_shutdown_ = true;
    return true;
  }

  bool IsExportCompleted() { return is_export_completed_->load(); }

private:
  std::shared_ptr<std::vector<std::unique_ptr<sdk::trace::SpanData>>> spans_received_;
  std::shared_ptr<std::atomic<std::size_t>> shut_down_counter_;
  std::shared_ptr<std::atomic<bool>> is_shutdown_;
  std::shared_ptr<std::atomic<bool>> is_export_completed_;
  // Meant exclusively to test force flush timeout
  const std::chrono::milliseconds export_delay_;
};

/**
 * Fixture Class
 */
class BatchSpanProcessorTestPeer : public testing::Test
{
public:
  std::unique_ptr<std::vector<std::unique_ptr<sdk::trace::Recordable>>> GetTestSpans(
      const std::shared_ptr<sdk::trace::SpanProcessor> &processor,
      const int num_spans)
  {
    std::unique_ptr<std::vector<std::unique_ptr<sdk::trace::Recordable>>> test_spans(
        new std::vector<std::unique_ptr<sdk::trace::Recordable>>);

    for (int i = 0; i < num_spans; ++i)
    {
      test_spans->push_back(processor->MakeRecordable());
      static_cast<sdk::trace::SpanData *>(test_spans->at(i).get())
          ->SetName("Span " + std::to_string(i));
    }

    return test_spans;
  }
};

/* ##################################   TESTS   ############################################ */

TEST_F(BatchSpanProcessorTestPeer, TestShutdown)
{
  std::shared_ptr<std::atomic<std::size_t>> shut_down_counter(new std::atomic<std::size_t>(0));
  std::shared_ptr<std::atomic<bool>> is_shutdown(new std::atomic<bool>(false));
  std::shared_ptr<std::vector<std::unique_ptr<sdk::trace::SpanData>>> spans_received(
      new std::vector<std::unique_ptr<sdk::trace::SpanData>>);

  auto batch_processor =
      std::shared_ptr<sdk::trace::BatchSpanProcessor>(new sdk::trace::BatchSpanProcessor(
          std::unique_ptr<sdk::trace::SpanExporter>(
              new MockSpanExporter(spans_received, shut_down_counter, is_shutdown)),
          sdk::trace::BatchSpanProcessorOptions()));
  const int num_spans = 3;

  auto test_spans = GetTestSpans(batch_processor, num_spans);

  for (int i = 0; i < num_spans; ++i)
  {
    batch_processor->OnEnd(std::move(test_spans->at(i)));
  }

  EXPECT_TRUE(batch_processor->Shutdown());
  // It's safe to shutdown again
  EXPECT_TRUE(batch_processor->Shutdown());

  EXPECT_EQ(num_spans, spans_received->size());
  for (int i = 0; i < num_spans; ++i)
  {
    EXPECT_EQ("Span " + std::to_string(i), spans_received->at(i)->GetName());
  }

  EXPECT_TRUE(is_shutdown->load());
}

TEST_F(BatchSpanProcessorTestPeer, TestForceFlush)
{
  std::shared_ptr<std::atomic<std::size_t>> shut_down_counter(new std::atomic<std::size_t>(0));
  std::shared_ptr<std::atomic<bool>> is_shutdown(new std::atomic<bool>(false));
  std::shared_ptr<std::vector<std::unique_ptr<sdk::trace::SpanData>>> spans_received(
      new std::vector<std::unique_ptr<sdk::trace::SpanData>>);

  auto batch_processor =
      std::shared_ptr<sdk::trace::BatchSpanProcessor>(new sdk::trace::BatchSpanProcessor(
          std::unique_ptr<sdk::trace::SpanExporter>(
              new MockSpanExporter(spans_received, shut_down_counter, is_shutdown)),
          sdk::trace::BatchSpanProcessorOptions()));
  const int num_spans = 2048;

  auto test_spans = GetTestSpans(batch_processor, num_spans);

  for (int i = 0; i < num_spans; ++i)
  {
    batch_processor->OnEnd(std::move(test_spans->at(i)));
  }

  // Give some time to export
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  EXPECT_TRUE(batch_processor->ForceFlush());
  EXPECT_GE(shut_down_counter->load(), 1);

  EXPECT_EQ(num_spans, spans_received->size());
  for (int i = 0; i < num_spans; ++i)
  {
    EXPECT_EQ("Span " + std::to_string(i), spans_received->at(i)->GetName());
  }

  // Create some more spans to make sure that the processor still works
  auto more_test_spans = GetTestSpans(batch_processor, num_spans);
  for (int i = 0; i < num_spans; ++i)
  {
    batch_processor->OnEnd(std::move(more_test_spans->at(i)));
  }

  // Give some time to export the spans
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  auto shut_down_counter_before = shut_down_counter->load();
  EXPECT_TRUE(batch_processor->ForceFlush());
  EXPECT_GT(shut_down_counter->load(), shut_down_counter_before);

  EXPECT_EQ(num_spans * 2, spans_received->size());
  for (int i = 0; i < num_spans; ++i)
  {
    EXPECT_EQ("Span " + std::to_string(i % num_spans),
              spans_received->at(num_spans + i)->GetName());
  }
}

// A mock log handler to check whether log messages with a specific level were emitted.
struct MockLogHandler : public sdk::common::internal_log::LogHandler
{
  using Message = std::pair<sdk::common::internal_log::LogLevel, std::string>;

  void Handle(sdk::common::internal_log::LogLevel level,
              const char * /*file*/,
              int /*line*/,
              const char *msg,
              const sdk::common::AttributeMap & /*attributes*/) noexcept override
  {
    messages.emplace_back(level, msg);
  }

  std::vector<Message> messages;
};

TEST_F(BatchSpanProcessorTestPeer, TestManySpansLoss)
{
  /* Test that when exporting more than max_queue_size spans, some are most likely lost*/

  // Set up a log handler to verify a warning is generated.
  auto log_handler = nostd::shared_ptr<sdk::common::internal_log::LogHandler>(new MockLogHandler());
  sdk::common::internal_log::GlobalLogHandler::SetLogHandler(log_handler);

  std::shared_ptr<std::atomic<std::size_t>> shut_down_counter(new std::atomic<std::size_t>(0));
  std::shared_ptr<std::atomic<bool>> is_shutdown(new std::atomic<bool>(false));
  std::shared_ptr<std::vector<std::unique_ptr<sdk::trace::SpanData>>> spans_received(
      new std::vector<std::unique_ptr<sdk::trace::SpanData>>);

  const int max_queue_size = 4096;

  auto batch_processor =
      std::shared_ptr<sdk::trace::BatchSpanProcessor>(new sdk::trace::BatchSpanProcessor(
          std::unique_ptr<sdk::trace::SpanExporter>(
              new MockSpanExporter(spans_received, shut_down_counter, is_shutdown)),
          sdk::trace::BatchSpanProcessorOptions()));

  auto test_spans = GetTestSpans(batch_processor, max_queue_size);

  for (int i = 0; i < max_queue_size; ++i)
  {
    batch_processor->OnEnd(std::move(test_spans->at(i)));
  }

  // Give some time to export the spans
  std::this_thread::sleep_for(std::chrono::milliseconds(700));

  EXPECT_TRUE(batch_processor->ForceFlush());

  // Span should be exported by now
  EXPECT_GE(max_queue_size, spans_received->size());

  // If we haven't received all spans, some must have dropped, verify a warning was logged.
  // Only do this when the log level is warning or above.
#if OTEL_INTERNAL_LOG_LEVEL >= OTEL_INTERNAL_LOG_LEVEL_WARN
  if (max_queue_size > spans_received->size())
  {
    auto &messages = static_cast<MockLogHandler *>(log_handler.get())->messages;
    EXPECT_TRUE(
        std::find(messages.begin(), messages.end(),
                  MockLogHandler::Message(sdk::common::internal_log::LogLevel::Warning,
                                          "BatchSpanProcessor queue is full - dropping span.")) !=
        messages.end());
  }
#endif

  // Reinstate the default log handler.
  sdk::common::internal_log::GlobalLogHandler::SetLogHandler(
      nostd::shared_ptr<sdk::common::internal_log::LogHandler>(
          new sdk::common::internal_log::DefaultLogHandler()));
}

TEST_F(BatchSpanProcessorTestPeer, TestManySpansLossLess)
{
  /* Test that no spans are lost when sending max_queue_size spans */

  std::shared_ptr<std::atomic<std::size_t>> shut_down_counter(new std::atomic<std::size_t>(0));
  std::shared_ptr<std::atomic<bool>> is_shutdown(new std::atomic<bool>(false));
  std::shared_ptr<std::vector<std::unique_ptr<sdk::trace::SpanData>>> spans_received(
      new std::vector<std::unique_ptr<sdk::trace::SpanData>>);

  const int num_spans = 2048;

  auto batch_processor =
      std::shared_ptr<sdk::trace::BatchSpanProcessor>(new sdk::trace::BatchSpanProcessor(
          std::unique_ptr<sdk::trace::SpanExporter>(
              new MockSpanExporter(spans_received, shut_down_counter, is_shutdown)),
          sdk::trace::BatchSpanProcessorOptions()));

  auto test_spans = GetTestSpans(batch_processor, num_spans);

  for (int i = 0; i < num_spans; ++i)
  {
    batch_processor->OnEnd(std::move(test_spans->at(i)));
  }

  // Give some time to export the spans
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  EXPECT_TRUE(batch_processor->ForceFlush());

  EXPECT_EQ(num_spans, spans_received->size());
  for (int i = 0; i < num_spans; ++i)
  {
    EXPECT_EQ("Span " + std::to_string(i), spans_received->at(i)->GetName());
  }
}

TEST_F(BatchSpanProcessorTestPeer, TestScheduleDelayMillis)
{
  /* Test that max_export_batch_size spans are exported every schedule_delay_millis
     seconds */

  std::shared_ptr<std::atomic<std::size_t>> shut_down_counter(new std::atomic<std::size_t>(0));
  std::shared_ptr<std::atomic<bool>> is_shutdown(new std::atomic<bool>(false));
  std::shared_ptr<std::atomic<bool>> is_export_completed(new std::atomic<bool>(false));
  std::shared_ptr<std::vector<std::unique_ptr<sdk::trace::SpanData>>> spans_received(
      new std::vector<std::unique_ptr<sdk::trace::SpanData>>);
  const std::chrono::milliseconds export_delay(0);
  const size_t max_export_batch_size = 512;
  sdk::trace::BatchSpanProcessorOptions options{};
  options.schedule_delay_millis = std::chrono::milliseconds(2000);

  auto batch_processor =
      std::shared_ptr<sdk::trace::BatchSpanProcessor>(new sdk::trace::BatchSpanProcessor(
          std::unique_ptr<sdk::trace::SpanExporter>(new MockSpanExporter(
              spans_received, shut_down_counter, is_shutdown, is_export_completed, export_delay)),
          options));

  auto test_spans = GetTestSpans(batch_processor, max_export_batch_size);

  for (size_t i = 0; i < max_export_batch_size; ++i)
  {
    batch_processor->OnEnd(std::move(test_spans->at(i)));
  }

  // Sleep for schedule_delay_millis milliseconds
  std::this_thread::sleep_for(options.schedule_delay_millis);

  // small delay to give time to export
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Spans should be exported by now
  EXPECT_TRUE(is_export_completed->load());
  EXPECT_EQ(max_export_batch_size, spans_received->size());
  for (size_t i = 0; i < max_export_batch_size; ++i)
  {
    EXPECT_EQ("Span " + std::to_string(i), spans_received->at(i)->GetName());
  }
}

OPENTELEMETRY_END_NAMESPACE
