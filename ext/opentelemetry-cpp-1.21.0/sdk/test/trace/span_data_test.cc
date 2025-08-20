// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "opentelemetry/common/key_value_iterable_view.h"
#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/trace/span_data.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/span_id.h"
#include "opentelemetry/trace/span_metadata.h"
#include "opentelemetry/trace/trace_flags.h"
#include "opentelemetry/trace/trace_id.h"
#include "opentelemetry/trace/trace_state.h"

using opentelemetry::sdk::trace::SpanData;
namespace trace_api = opentelemetry::trace;
namespace common    = opentelemetry::common;

TEST(SpanData, DefaultValues)
{
  trace_api::SpanContext empty_span_context{false, false};
  trace_api::SpanId zero_span_id;
  SpanData data;

  ASSERT_EQ(data.GetTraceId(), empty_span_context.trace_id());
  ASSERT_EQ(data.GetSpanId(), empty_span_context.span_id());
  ASSERT_EQ(data.GetSpanContext(), empty_span_context);
  ASSERT_EQ(data.GetParentSpanId(), zero_span_id);
  ASSERT_EQ(data.GetName(), "");
  ASSERT_EQ(data.GetStatus(), trace_api::StatusCode::kUnset);
  ASSERT_EQ(data.GetDescription(), "");
  ASSERT_EQ(data.GetStartTime().time_since_epoch(), std::chrono::nanoseconds(0));
  ASSERT_EQ(data.GetDuration(), std::chrono::nanoseconds(0));
  ASSERT_EQ(data.GetAttributes().size(), 0);
  ASSERT_EQ(data.GetEvents().size(), 0);
}

TEST(SpanData, Set)
{
  constexpr uint8_t trace_id_buf[]       = {1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8};
  constexpr uint8_t span_id_buf[]        = {1, 2, 3, 4, 5, 6, 7, 8};
  constexpr uint8_t parent_span_id_buf[] = {8, 7, 6, 5, 4, 3, 2, 1};
  trace_api::TraceId trace_id{trace_id_buf};
  trace_api::SpanId span_id{span_id_buf};
  trace_api::SpanId parent_span_id{parent_span_id_buf};
  const auto trace_state = trace_api::TraceState::GetDefault()->Set("key1", "value");
  const trace_api::SpanContext span_context{
      trace_id, span_id, trace_api::TraceFlags{trace_api::TraceFlags::kIsSampled}, true,
      trace_state};
  common::SystemTimestamp now(std::chrono::system_clock::now());

  SpanData data;
  data.SetIdentity(span_context, parent_span_id);
  data.SetName("span name");
  data.SetSpanKind(trace_api::SpanKind::kServer);
  data.SetStatus(trace_api::StatusCode::kOk, "description");
  data.SetStartTime(now);
  data.SetDuration(std::chrono::nanoseconds(1000000));
  data.SetAttribute("attr1", static_cast<int64_t>(314159));
  data.AddEvent("event1", now);

  ASSERT_EQ(data.GetTraceId(), trace_id);
  ASSERT_EQ(data.GetSpanId(), span_id);
  ASSERT_EQ(data.GetSpanContext(), span_context);
  std::string trace_state_key1_value;
  ASSERT_EQ(data.GetSpanContext().trace_state()->Get("key1", trace_state_key1_value), true);
  ASSERT_EQ(trace_state_key1_value, "value");
  ASSERT_EQ(data.GetParentSpanId(), parent_span_id);
  ASSERT_EQ(data.GetName(), "span name");
  ASSERT_EQ(data.GetSpanKind(), trace_api::SpanKind::kServer);
  ASSERT_EQ(data.GetStatus(), trace_api::StatusCode::kOk);
  ASSERT_EQ(data.GetDescription(), "description");
  ASSERT_EQ(data.GetStartTime().time_since_epoch(), now.time_since_epoch());
  ASSERT_EQ(data.GetDuration(), std::chrono::nanoseconds(1000000));
  ASSERT_EQ(opentelemetry::nostd::get<int64_t>(data.GetAttributes().at("attr1")), 314159);
  ASSERT_EQ(data.GetEvents().at(0).GetName(), "event1");
  ASSERT_EQ(data.GetEvents().at(0).GetTimestamp(), now);
}

TEST(SpanData, EventAttributes)
{
  SpanData data;
  const int kNumAttributes                  = 3;
  std::string keys[kNumAttributes]          = {"attr1", "attr2", "attr3"};
  int64_t values[kNumAttributes]            = {3, 5, 20};
  std::map<std::string, int64_t> attributes = {
      {keys[0], values[0]}, {keys[1], values[1]}, {keys[2], values[2]}};

  data.AddEvent("Test Event", std::chrono::system_clock::now(),
                common::KeyValueIterableView<std::map<std::string, int64_t>>(attributes));

  for (int i = 0; i < kNumAttributes; i++)
  {
    EXPECT_EQ(
        opentelemetry::nostd::get<int64_t>(data.GetEvents().at(0).GetAttributes().at(keys[i])),
        values[i]);
  }
}

TEST(SpanData, Resources)
{
  SpanData data;
  auto resource          = opentelemetry::sdk::resource::Resource::Create({});
  const auto &input_attr = resource.GetAttributes();
  data.SetResource(resource);
  auto output_attr = data.GetResource().GetAttributes();
  EXPECT_EQ(input_attr, output_attr);
}

TEST(SpanData, Links)
{
  SpanData data;
  const int kNumAttributes                  = 3;
  std::string keys[kNumAttributes]          = {"attr1", "attr2", "attr3"};
  int64_t values[kNumAttributes]            = {4, 12, 33};
  std::map<std::string, int64_t> attributes = {
      {keys[0], values[0]}, {keys[1], values[1]}, {keys[2], values[2]}};

  // produce valid SpanContext with pseudo span and trace Id.
  uint8_t span_id_buf[trace_api::SpanId::kSize] = {
      1,
  };
  trace_api::SpanId span_id{span_id_buf};
  uint8_t trace_id_buf[trace_api::TraceId::kSize] = {
      2,
  };
  trace_api::TraceId trace_id{trace_id_buf};
  const auto span_context = trace_api::SpanContext(
      trace_id, span_id, trace_api::TraceFlags{trace_api::TraceFlags::kIsSampled}, true);

  data.AddLink(span_context,
               common::KeyValueIterableView<std::map<std::string, int64_t>>(attributes));

  EXPECT_EQ(data.GetLinks().at(0).GetSpanContext(), span_context);
  for (int i = 0; i < kNumAttributes; i++)
  {
    EXPECT_EQ(opentelemetry::nostd::get<int64_t>(data.GetLinks().at(0).GetAttributes().at(keys[i])),
              values[i]);
  }
}
