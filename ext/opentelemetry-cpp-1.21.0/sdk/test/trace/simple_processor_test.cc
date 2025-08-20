// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "opentelemetry/exporters/memory/in_memory_span_data.h"
#include "opentelemetry/exporters/memory/in_memory_span_exporter.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/sdk/common/exporter_utils.h"
#include "opentelemetry/sdk/trace/exporter.h"
#include "opentelemetry/sdk/trace/recordable.h"
#include "opentelemetry/sdk/trace/simple_processor.h"
#include "opentelemetry/sdk/trace/span_data.h"
#include "opentelemetry/trace/span_context.h"

using namespace opentelemetry::sdk::trace;
using namespace opentelemetry::sdk::common;
using opentelemetry::exporter::memory::InMemorySpanData;
using opentelemetry::exporter::memory::InMemorySpanExporter;
using opentelemetry::trace::SpanContext;

TEST(SimpleProcessor, ToInMemorySpanExporter)
{
  InMemorySpanExporter *exporter              = new InMemorySpanExporter();
  std::shared_ptr<InMemorySpanData> span_data = exporter->GetData();
  SimpleSpanProcessor processor(std::unique_ptr<SpanExporter>{exporter});

  auto recordable = processor.MakeRecordable();

  processor.OnStart(*recordable, SpanContext::GetInvalid());

  ASSERT_EQ(0, span_data->GetSpans().size());

  processor.OnEnd(std::move(recordable));

  ASSERT_EQ(1, span_data->GetSpans().size());

  EXPECT_TRUE(processor.Shutdown());
}

// An exporter that does nothing but record (and give back ) the # of times Shutdown was called.
class RecordShutdownExporter final : public SpanExporter
{
public:
  RecordShutdownExporter(int *force_flush_counter, int *shutdown_counter)
      : force_flush_counter_(force_flush_counter), shutdown_counter_(shutdown_counter)
  {}

  std::unique_ptr<Recordable> MakeRecordable() noexcept override
  {
    return std::unique_ptr<Recordable>(new SpanData());
  }

  ExportResult Export(const opentelemetry::nostd::span<std::unique_ptr<Recordable>>
                          & /* recordables */) noexcept override
  {
    return ExportResult::kSuccess;
  }

  bool ForceFlush(std::chrono::microseconds /* timeout */) noexcept override
  {
    *force_flush_counter_ += 1;
    return true;
  }

  bool Shutdown(std::chrono::microseconds /* timeout */) noexcept override
  {
    *shutdown_counter_ += 1;
    return true;
  }

private:
  int *force_flush_counter_;
  int *shutdown_counter_;
};

TEST(SimpleSpanProcessor, ShutdownCalledOnce)
{
  int force_flush                  = 0;
  int shutdowns                    = 0;
  RecordShutdownExporter *exporter = new RecordShutdownExporter(&force_flush, &shutdowns);
  SimpleSpanProcessor processor(std::unique_ptr<SpanExporter>{exporter});
  EXPECT_EQ(0, shutdowns);
  processor.Shutdown();
  EXPECT_EQ(1, shutdowns);
  processor.Shutdown();
  EXPECT_EQ(1, shutdowns);

  EXPECT_EQ(0, force_flush);
}

TEST(SimpleSpanProcessor, ForceFlush)
{
  int force_flush                  = 0;
  int shutdowns                    = 0;
  RecordShutdownExporter *exporter = new RecordShutdownExporter(&force_flush, &shutdowns);
  SimpleSpanProcessor processor(std::unique_ptr<SpanExporter>{exporter});
  processor.ForceFlush();
  EXPECT_EQ(0, shutdowns);
  EXPECT_EQ(1, force_flush);
  processor.ForceFlush();
  EXPECT_EQ(2, force_flush);
}

// An exporter that does nothing but record (and give back ) the # of times Shutdown was called.
class FailShutDownForceFlushExporter final : public SpanExporter
{
public:
  FailShutDownForceFlushExporter() {}

  std::unique_ptr<Recordable> MakeRecordable() noexcept override
  {
    return std::unique_ptr<Recordable>(new SpanData());
  }

  ExportResult Export(const opentelemetry::nostd::span<std::unique_ptr<Recordable>>
                          & /* recordables */) noexcept override
  {
    return ExportResult::kSuccess;
  }

  bool ForceFlush(std::chrono::microseconds /* timeout */) noexcept override { return false; }

  bool Shutdown(std::chrono::microseconds /* timeout */) noexcept override { return false; }
};

TEST(SimpleSpanProcessor, ShutdownFail)
{
  SimpleSpanProcessor processor(
      std::unique_ptr<SpanExporter>{new FailShutDownForceFlushExporter()});
  EXPECT_EQ(false, processor.Shutdown());
}

TEST(SimpleSpanProcessor, ForceFlushFail)
{
  SimpleSpanProcessor processor(
      std::unique_ptr<SpanExporter>{new FailShutDownForceFlushExporter()});
  EXPECT_EQ(false, processor.ForceFlush());
}
