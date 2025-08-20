/*
 * Copyright The OpenTelemetry Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>
#include <stdint.h>
#include <algorithm>
#include <chrono>
#include <functional>
#include <initializer_list>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "opentracing/span.h"
#include "opentracing/string_view.h"
#include "opentracing/util.h"
#include "opentracing/value.h"

#include "opentelemetry/baggage/baggage.h"
#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/unique_ptr.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/opentracingshim/span_shim.h"
#include "opentelemetry/opentracingshim/tracer_shim.h"
#include "opentelemetry/trace/span.h"
#include "opentelemetry/trace/span_metadata.h"

#include "shim_mocks.h"

namespace trace_api = opentelemetry::trace;
namespace baggage   = opentelemetry::baggage;
namespace nostd     = opentelemetry::nostd;
namespace shim      = opentelemetry::opentracingshim;

class SpanShimTest : public testing::Test
{
public:
  nostd::unique_ptr<shim::SpanShim> span_shim;
  MockSpan *mock_span;

protected:
  virtual void SetUp() override
  {
    mock_span        = new MockSpan();
    auto span        = nostd::shared_ptr<trace_api::Span>(mock_span);
    auto tracer      = shim::TracerShim::createTracerShim();
    auto tracer_shim = static_cast<shim::TracerShim *>(tracer.get());
    auto baggage     = baggage::Baggage::GetDefault()->Set("baggage", "item");
    span_shim = nostd::unique_ptr<shim::SpanShim>(new shim::SpanShim(*tracer_shim, span, baggage));
  }

  virtual void TearDown() override { span_shim.reset(); }
};

TEST_F(SpanShimTest, HandleError)
{
  span_shim->handleError(true);
  ASSERT_EQ(mock_span->status_.first, trace_api::StatusCode::kError);
  span_shim->handleError("true");
  ASSERT_EQ(mock_span->status_.first, trace_api::StatusCode::kError);
  span_shim->handleError(false);
  ASSERT_EQ(mock_span->status_.first, trace_api::StatusCode::kOk);
  span_shim->handleError("false");
  ASSERT_EQ(mock_span->status_.first, trace_api::StatusCode::kOk);
  span_shim->handleError(nullptr);
  ASSERT_EQ(mock_span->status_.first, trace_api::StatusCode::kUnset);
  span_shim->handleError("unknown");
  ASSERT_EQ(mock_span->status_.first, trace_api::StatusCode::kUnset);
  span_shim->handleError(42);
  ASSERT_EQ(mock_span->status_.first, trace_api::StatusCode::kUnset);
}

TEST_F(SpanShimTest, FinishWithOptions)
{
  opentracing::FinishSpanOptions options;
  options.finish_steady_timestamp = opentracing::SteadyTime::clock::now();
  ASSERT_NE(mock_span->options_.end_steady_time, options.finish_steady_timestamp);
  span_shim->FinishWithOptions(options);
  ASSERT_EQ(mock_span->options_.end_steady_time, options.finish_steady_timestamp);
}

TEST_F(SpanShimTest, SetOperationName)
{
  ASSERT_NE(mock_span->name_, "foo");
  span_shim->SetOperationName("foo");
  ASSERT_EQ(mock_span->name_, "foo");
}

TEST_F(SpanShimTest, SetTag_Normal)
{
  ASSERT_NE(mock_span->attribute_.first, "foo");
  span_shim->SetTag("foo", "bar");
  ASSERT_EQ(mock_span->attribute_.first, "foo");
  ASSERT_STREQ(nostd::get<const char *>(mock_span->attribute_.second), "bar");
}

TEST_F(SpanShimTest, SetTag_Error)
{
  ASSERT_NE(mock_span->attribute_.first, "error");
  span_shim->SetTag("error", true);
  ASSERT_NE(mock_span->attribute_.first, "error");
  span_shim->SetTag("error", "false");
  ASSERT_NE(mock_span->attribute_.first, "error");
  span_shim->SetTag("error", 42);
  ASSERT_NE(mock_span->attribute_.first, "error");
  span_shim->SetTag("error", nullptr);
  ASSERT_NE(mock_span->attribute_.first, "error");
}

TEST_F(SpanShimTest, BaggageItem)
{
  ASSERT_EQ(span_shim->BaggageItem({}), "");
  ASSERT_EQ(span_shim->BaggageItem(""), "");
  ASSERT_EQ(span_shim->BaggageItem("invalid"), "");
  ASSERT_EQ(span_shim->BaggageItem("baggage"), "item");
}

TEST_F(SpanShimTest, SetBaggageItem)
{
  span_shim->SetBaggageItem("new", "entry");
  ASSERT_EQ(span_shim->BaggageItem("new"), "entry");
  ASSERT_EQ(span_shim->BaggageItem("baggage"), "item");
  span_shim->SetBaggageItem("empty", "");
  ASSERT_EQ(span_shim->BaggageItem("empty"), "");
  span_shim->SetBaggageItem("no value", {});
  ASSERT_EQ(span_shim->BaggageItem("no value"), "");
}

TEST_F(SpanShimTest, SetBaggageItem_MultiThreaded)
{
  auto span        = nostd::shared_ptr<trace_api::Span>(new MockSpan());
  auto tracer      = shim::TracerShim::createTracerShim();
  auto tracer_shim = static_cast<shim::TracerShim *>(tracer.get());
  auto baggage     = baggage::Baggage::GetDefault();
  shim::SpanShim span_shim(*tracer_shim, span, baggage);

  std::vector<std::thread> threads;
  std::vector<std::string> keys;
  std::vector<std::string> values;
  int thread_count = 100;

  for (int index = 0; index < thread_count; ++index)
  {
    keys.emplace_back("key-" + std::to_string(index));
    values.emplace_back("value-" + std::to_string(index));
    threads.emplace_back(
        std::bind(&shim::SpanShim::SetBaggageItem, &span_shim, keys[index], values[index]));
  }

  for (auto &thread : threads)
  {
    thread.join();
  }

  for (int index = 0; index < thread_count; ++index)
  {
    ASSERT_EQ(span_shim.BaggageItem(keys[index]), values[index]);
  }
}

TEST_F(SpanShimTest, Log_NoEvent)
{
  std::string name;
  common::SystemTimestamp timestamp;
  std::unordered_map<std::string, common::AttributeValue> attributes;

  span_shim->Log({{"test", 42}});
  std::tie(name, timestamp, attributes) = mock_span->event_;

  ASSERT_EQ(name, "log");
  ASSERT_EQ(timestamp, common::SystemTimestamp{});
  ASSERT_EQ(attributes.size(), 1);
  ASSERT_EQ(nostd::get<int64_t>(attributes["test"]), 42);
}

TEST_F(SpanShimTest, Log_NoEvent_Timestamp)
{
  std::string name;
  common::SystemTimestamp timestamp;
  std::unordered_map<std::string, common::AttributeValue> attributes;

  auto logtime = opentracing::SystemTime::time_point::clock::now();
  span_shim->Log(logtime, {{"foo", "bar"}});
  std::tie(name, timestamp, attributes) = mock_span->event_;

  ASSERT_EQ(name, "log");
  ASSERT_EQ(timestamp, common::SystemTimestamp{logtime});
  ASSERT_EQ(attributes.size(), 1);
  ASSERT_STREQ(nostd::get<const char *>(attributes["foo"]), "bar");
}

TEST_F(SpanShimTest, Log_Event)
{
  std::string name;
  common::SystemTimestamp timestamp;
  std::unordered_map<std::string, common::AttributeValue> attributes;

  auto logtime = opentracing::SystemTime::time_point::clock::now();
  std::initializer_list<std::pair<opentracing::string_view, opentracing::Value>> fields{
      {"event", "normal"},
      {"foo", opentracing::string_view{"bar"}},
      {"error.kind", 42},
      {"message", "hello"},
      {"stack", "overflow"}};
  span_shim->Log(logtime, fields);
  std::tie(name, timestamp, attributes) = mock_span->event_;

  ASSERT_EQ(name, "normal");
  ASSERT_EQ(timestamp, common::SystemTimestamp{logtime});
  ASSERT_EQ(attributes.size(), 5);
  ASSERT_STREQ(nostd::get<const char *>(attributes["event"]), "normal");
  ASSERT_EQ(nostd::get<nostd::string_view>(attributes["foo"]), nostd::string_view{"bar"});
  ASSERT_EQ(nostd::get<int64_t>(attributes["error.kind"]), 42);
  ASSERT_STREQ(nostd::get<const char *>(attributes["message"]), "hello");
  ASSERT_STREQ(nostd::get<const char *>(attributes["stack"]), "overflow");
}

TEST_F(SpanShimTest, Log_Error)
{
  std::string name;
  common::SystemTimestamp timestamp;
  std::unordered_map<std::string, common::AttributeValue> attributes;

  auto logtime = opentracing::SystemTime::time_point::clock::now();
  std::vector<std::pair<opentracing::string_view, opentracing::Value>> fields{
      {"event", "error"},
      {"foo", opentracing::string_view{"bar"}},
      {"error.kind", 42},
      {"message", "hello"},
      {"stack", "overflow"}};
  span_shim->Log(logtime, fields);
  std::tie(name, timestamp, attributes) = mock_span->event_;

  ASSERT_EQ(name, "exception");
  ASSERT_EQ(timestamp, common::SystemTimestamp{logtime});
  ASSERT_EQ(attributes.size(), 5);
  ASSERT_STREQ(nostd::get<const char *>(attributes["event"]), "error");
  ASSERT_EQ(nostd::get<nostd::string_view>(attributes["foo"]), nostd::string_view{"bar"});
  ASSERT_EQ(nostd::get<int64_t>(attributes["exception.type"]), 42);
  ASSERT_STREQ(nostd::get<const char *>(attributes["exception.message"]), "hello");
  ASSERT_STREQ(nostd::get<const char *>(attributes["exception.stacktrace"]), "overflow");
}
