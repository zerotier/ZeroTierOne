// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>
#include <vector>

#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/common/key_value_iterable_view.h"
#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/exporters/zipkin/recordable.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/trace/recordable.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/span_id.h"
#include "opentelemetry/trace/span_metadata.h"
#include "opentelemetry/trace/trace_flags.h"
#include "opentelemetry/trace/trace_id.h"

#if defined(__GNUC__)
// GCC raises -Wsuggest-override warnings on GTest,
// in code related to TYPED_TEST() .
#  pragma GCC diagnostic ignored "-Wsuggest-override"
#endif

namespace trace    = opentelemetry::trace;
namespace nostd    = opentelemetry::nostd;
namespace sdktrace = opentelemetry::sdk::trace;
namespace common   = opentelemetry::common;
namespace zipkin   = opentelemetry::exporter::zipkin;
using json         = nlohmann::json;

// Testing Shutdown functionality of OStreamSpanExporter, should expect no data to be sent to Stream
TEST(ZipkinSpanRecordable, SetIdentity)
{
  json j_span = {{"id", "0000000000000002"},
                 {"parentId", "0000000000000003"},
                 {"traceId", "00000000000000000000000000000001"}};
  zipkin::Recordable rec;
  const trace::TraceId trace_id(std::array<const uint8_t, trace::TraceId::kSize>(
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}));

  const trace::SpanId span_id(
      std::array<const uint8_t, trace::SpanId::kSize>({0, 0, 0, 0, 0, 0, 0, 2}));

  const trace::SpanId parent_span_id(
      std::array<const uint8_t, trace::SpanId::kSize>({0, 0, 0, 0, 0, 0, 0, 3}));

  const trace::SpanContext span_context{trace_id, span_id,
                                        trace::TraceFlags{trace::TraceFlags::kIsSampled}, true};

  rec.SetIdentity(span_context, parent_span_id);
  EXPECT_EQ(rec.span(), j_span);
}

// according to https://zipkin.io/zipkin-api/#/ in case root span is created
// the parentId filed should be absent.
TEST(ZipkinSpanRecordable, SetIdentityEmptyParent)
{
  json j_span = {{"id", "0000000000000002"}, {"traceId", "00000000000000000000000000000001"}};
  zipkin::Recordable rec;
  const trace::TraceId trace_id(std::array<const uint8_t, trace::TraceId::kSize>(
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}));

  const trace::SpanId span_id(
      std::array<const uint8_t, trace::SpanId::kSize>({0, 0, 0, 0, 0, 0, 0, 2}));

  const trace::SpanId parent_span_id(
      std::array<const uint8_t, trace::SpanId::kSize>({0, 0, 0, 0, 0, 0, 0, 0}));

  const trace::SpanContext span_context{trace_id, span_id,
                                        trace::TraceFlags{trace::TraceFlags::kIsSampled}, true};

  rec.SetIdentity(span_context, parent_span_id);
  EXPECT_EQ(rec.span(), j_span);
}

TEST(ZipkinSpanRecordable, SetName)
{
  nostd::string_view name = "Test Span";
  json j_span             = {{"name", name}};
  zipkin::Recordable rec;
  rec.SetName(name);
  EXPECT_EQ(rec.span(), j_span);
}

TEST(ZipkinSpanRecordable, SetStartTime)
{
  zipkin::Recordable rec;
  std::chrono::system_clock::time_point start_time = std::chrono::system_clock::now();
  common::SystemTimestamp start_timestamp(start_time);

  uint64_t unix_start =
      std::chrono::duration_cast<std::chrono::microseconds>(start_time.time_since_epoch()).count();
  json j_span = {{"timestamp", unix_start}};
  rec.SetStartTime(start_timestamp);
  EXPECT_EQ(rec.span(), j_span);
}

TEST(ZipkinSpanRecordable, SetDuration)
{
  std::chrono::nanoseconds durationNS(1000000000);  // in ns
  std::chrono::microseconds durationMS =
      std::chrono::duration_cast<std::chrono::microseconds>(durationNS);  // in ms
  json j_span = {{"duration", durationMS.count()}, {"timestamp", 0}};
  zipkin::Recordable rec;
  // Start time is 0
  common::SystemTimestamp start_timestamp;

  rec.SetStartTime(start_timestamp);
  rec.SetDuration(durationNS);
  EXPECT_EQ(rec.span(), j_span);
}

TEST(ZipkinSpanRecordable, SetInstrumentationScope)
{
  using InstrumentationScope = opentelemetry::sdk::instrumentationscope::InstrumentationScope;

  const char *library_name    = "otel-cpp";
  const char *library_version = "0.5.0";
  json j_span                 = {
      {"tags", {{"otel.library.name", library_name}, {"otel.library.version", library_version}}}};
  zipkin::Recordable rec;

  rec.SetInstrumentationScope(*InstrumentationScope::Create(library_name, library_version));

  EXPECT_EQ(rec.span(), j_span);
}

TEST(ZipkinSpanRecordable, SetStatus)
{
  std::string description                     = "Error description";
  std::vector<trace::StatusCode> status_codes = {trace::StatusCode::kError, trace::StatusCode::kOk};
  for (auto &status_code : status_codes)
  {
    zipkin::Recordable rec;
    trace::StatusCode code(status_code);
    json j_span;
    if (status_code == trace::StatusCode::kError)
      j_span = {{"tags", {{"otel.status_code", status_code}, {"error", description}}}};
    else
      j_span = {{"tags", {{"otel.status_code", status_code}}}};

    rec.SetStatus(code, description);
    EXPECT_EQ(rec.span(), j_span);
  }
}

TEST(ZipkinSpanRecordable, SetSpanKind)
{
  json j_json_client = {{"kind", "CLIENT"}};
  zipkin::Recordable rec;
  rec.SetSpanKind(trace::SpanKind::kClient);
  EXPECT_EQ(rec.span(), j_json_client);
}

TEST(ZipkinSpanRecordable, AddEventDefault)
{
  zipkin::Recordable rec;
  nostd::string_view name = "Test Event";

  std::chrono::system_clock::time_point event_time = std::chrono::system_clock::now();
  common::SystemTimestamp event_timestamp(event_time);

  rec.sdktrace::Recordable::AddEvent(name, event_timestamp);

  uint64_t unix_event_time =
      std::chrono::duration_cast<std::chrono::microseconds>(event_time.time_since_epoch()).count();

  json j_span = {
      {"annotations",
       {{{"value", json({{name, json::object()}}).dump()}, {"timestamp", unix_event_time}}}}};
  EXPECT_EQ(rec.span(), j_span);
}

TEST(ZipkinSpanRecordable, AddEventWithAttributes)
{
  zipkin::Recordable rec;

  std::chrono::system_clock::time_point event_time = std::chrono::system_clock::now();
  common::SystemTimestamp event_timestamp(event_time);
  uint64_t unix_event_time =
      std::chrono::duration_cast<std::chrono::microseconds>(event_time.time_since_epoch()).count();

  const int kNumAttributes              = 3;
  std::string keys[kNumAttributes]      = {"attr1", "attr2", "attr3"};
  int values[kNumAttributes]            = {4, 7, 23};
  std::map<std::string, int> attributes = {
      {keys[0], values[0]}, {keys[1], values[1]}, {keys[2], values[2]}};

  rec.AddEvent("Test Event", event_timestamp,
               common::KeyValueIterableView<std::map<std::string, int>>(attributes));

  nlohmann::json j_span = {
      {"annotations",
       {{{"value", json({{"Test Event", {{"attr1", 4}, {"attr2", 7}, {"attr3", 23}}}}).dump()},
         {"timestamp", unix_event_time}}}}};
  EXPECT_EQ(rec.span(), j_span);
}

// Test non-int single types. Int single types are tested using templates (see IntAttributeTest)
TEST(ZipkinSpanRecordable, SetSingleAtrribute)
{
  zipkin::Recordable rec;
  nostd::string_view bool_key = "bool_attr";
  common::AttributeValue bool_val(true);
  rec.SetAttribute(bool_key, bool_val);

  nostd::string_view double_key = "double_attr";
  common::AttributeValue double_val(3.3);
  rec.SetAttribute(double_key, double_val);

  nostd::string_view str_key = "str_attr";
  common::AttributeValue str_val(nostd::string_view("Test"));
  rec.SetAttribute(str_key, str_val);
  nlohmann::json j_span = {
      {"tags", {{"bool_attr", true}, {"double_attr", 3.3}, {"str_attr", "Test"}}}};

  EXPECT_EQ(rec.span(), j_span);
}

// Test non-int array types. Int array types are tested using templates (see IntAttributeTest)
TEST(ZipkinSpanRecordable, SetArrayAtrribute)
{
  zipkin::Recordable rec;
  nlohmann::json j_span = {{"tags",
                            {{"bool_arr_attr", {true, false, true}},
                             {"double_arr_attr", {22.3, 33.4, 44.5}},
                             {"str_arr_attr", {"Hello", "World", "Test"}}}}};
  const int kArraySize  = 3;

  bool bool_arr[kArraySize] = {true, false, true};
  nostd::span<const bool> bool_span(bool_arr);
  rec.SetAttribute("bool_arr_attr", bool_span);

  double double_arr[kArraySize] = {22.3, 33.4, 44.5};
  nostd::span<const double> double_span(double_arr);
  rec.SetAttribute("double_arr_attr", double_span);

  nostd::string_view str_arr[kArraySize] = {"Hello", "World", "Test"};
  nostd::span<const nostd::string_view> str_span(str_arr);
  rec.SetAttribute("str_arr_attr", str_span);

  EXPECT_EQ(rec.span(), j_span);
}

TEST(ZipkinSpanRecordable, SetResource)
{
  zipkin::Recordable rec;
  std::string service_name = "test";
  auto resource = opentelemetry::sdk::resource::Resource::Create({{"service.name", service_name}});
  rec.SetResource(resource);
  EXPECT_EQ(rec.GetServiceName(), service_name);
}

/**
 * AttributeValue can contain different int types, such as int, int64_t,
 * unsigned int, and uint64_t. To avoid writing test cases for each, we can
 * use a template approach to test all int types.
 */
template <typename T>
struct ZipkinIntAttributeTest : public testing::Test
{
  using IntParamType = T;
};

using IntTypes = testing::Types<int, int64_t, unsigned int, uint64_t>;
TYPED_TEST_SUITE(ZipkinIntAttributeTest, IntTypes);

TYPED_TEST(ZipkinIntAttributeTest, SetIntSingleAttribute)
{
  using IntType = typename TestFixture::IntParamType;
  IntType i     = 2;
  common::AttributeValue int_val(i);

  zipkin::Recordable rec;
  rec.SetAttribute("int_attr", int_val);
  nlohmann::json j_span = {{"tags", {{"int_attr", 2}}}};
  EXPECT_EQ(rec.span(), j_span);
}

TYPED_TEST(ZipkinIntAttributeTest, SetIntArrayAttribute)
{
  using IntType = typename TestFixture::IntParamType;

  const int kArraySize        = 3;
  IntType int_arr[kArraySize] = {4, 5, 6};
  nostd::span<const IntType> int_span(int_arr);

  zipkin::Recordable rec;
  rec.SetAttribute("int_arr_attr", int_span);
  nlohmann::json j_span = {{"tags", {{"int_arr_attr", {4, 5, 6}}}}};
  EXPECT_EQ(rec.span(), j_span);
}
