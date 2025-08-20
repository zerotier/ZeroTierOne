// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <stdint.h>
#include <algorithm>
#include <chrono>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/common/key_value_iterable_view.h"
#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/exporters/otlp/otlp_recordable.h"
#include "opentelemetry/exporters/otlp/otlp_recordable_utils.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/sdk/trace/recordable.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/span_id.h"
#include "opentelemetry/trace/span_metadata.h"
#include "opentelemetry/trace/trace_flags.h"
#include "opentelemetry/trace/trace_id.h"
#include "opentelemetry/trace/trace_state.h"
#include "opentelemetry/version.h"

#if defined(__GNUC__)
// GCC raises -Wsuggest-override warnings on GTest,
// in code related to TYPED_TEST() .
#  pragma GCC diagnostic ignored "-Wsuggest-override"
#endif

// clang-format off
#include "opentelemetry/exporters/otlp/protobuf_include_prefix.h" // IWYU pragma: keep
// IWYU pragma: no_include "net/proto2/public/repeated_field.h"
#include "opentelemetry/proto/collector/trace/v1/trace_service.pb.h"
#include "opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/resource/v1/resource.pb.h"
#include "opentelemetry/proto/trace/v1/trace.pb.h"
#include "opentelemetry/exporters/otlp/protobuf_include_suffix.h" // IWYU pragma: keep
// clang-format on

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{
namespace trace_api = opentelemetry::trace;
namespace trace_sdk = opentelemetry::sdk::trace;
namespace resource  = opentelemetry::sdk::resource;
namespace proto     = opentelemetry::proto;

namespace trace_sdk_2 = opentelemetry::sdk::trace;

TEST(OtlpRecordable, SetIdentity)
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

  OtlpRecordable rec;

  rec.SetIdentity(span_context, parent_span_id);

  EXPECT_EQ(rec.span().trace_id(), std::string(reinterpret_cast<const char *>(trace_id.Id().data()),
                                               trace::TraceId::kSize));
  EXPECT_EQ(rec.span().span_id(),
            std::string(reinterpret_cast<const char *>(span_id.Id().data()), trace::SpanId::kSize));
  EXPECT_EQ(rec.span().trace_state(), "key1=value");
  EXPECT_EQ(rec.span().parent_span_id(),
            std::string(reinterpret_cast<const char *>(parent_span_id.Id().data()),
                        trace::SpanId::kSize));

  OtlpRecordable rec_invalid_parent;

  constexpr uint8_t invalid_parent_span_id_buf[] = {0, 0, 0, 0, 0, 0, 0, 0};
  trace_api::SpanId invalid_parent_span_id{invalid_parent_span_id_buf};
  rec_invalid_parent.SetIdentity(span_context, invalid_parent_span_id);

  EXPECT_EQ(rec_invalid_parent.span().parent_span_id(), std::string{});
}

TEST(OtlpRecordable, SetSpanKind)
{
  OtlpRecordable rec;
  trace_api::SpanKind span_kind = trace_api::SpanKind::kServer;
  rec.SetSpanKind(span_kind);
  EXPECT_EQ(rec.span().kind(), proto::trace::v1::Span_SpanKind::Span_SpanKind_SPAN_KIND_SERVER);
}

TEST(OtlpRecordable, SetInstrumentationScope)
{
  OtlpRecordable rec;
  auto inst_lib = trace_sdk::InstrumentationScope::Create("test", "v1");
  rec.SetInstrumentationScope(*inst_lib);
  auto proto_instr_libr = rec.GetProtoInstrumentationScope();
  EXPECT_EQ(proto_instr_libr.name(), inst_lib->GetName());
  EXPECT_EQ(proto_instr_libr.version(), inst_lib->GetVersion());
}

TEST(OtlpRecordable, SetInstrumentationLibraryWithSchemaURL)
{
  OtlpRecordable rec;
  const std::string expected_schema_url{"https://opentelemetry.io/schemas/1.11.0"};
  auto inst_lib = trace_sdk::InstrumentationScope::Create("test", "v1", expected_schema_url);
  rec.SetInstrumentationScope(*inst_lib);
  EXPECT_EQ(expected_schema_url, rec.GetInstrumentationLibrarySchemaURL());
}

TEST(OtlpRecordable, SetInstrumentationScopeWithAttributes)
{
  exporter::otlp::OtlpRecordable rec;

  auto inst_lib = trace_sdk::InstrumentationScope::Create(
      "test_scope_name", "test_version", "test_schema_url", {{"test_key", "test_value"}});

  ASSERT_EQ(inst_lib->GetAttributes().size(), 1);

  rec.SetInstrumentationScope(*inst_lib);

  const auto proto_instr_libr = rec.GetProtoInstrumentationScope();
  EXPECT_EQ("test_scope_name", proto_instr_libr.name());
  EXPECT_EQ("test_version", proto_instr_libr.version());

  ASSERT_EQ(proto_instr_libr.attributes_size(), 1);
  const auto &proto_attributes = proto_instr_libr.attributes(0);
  // Requires protoc 3.15.0
  // ASSERT_TRUE(proto_attributes.value().has_string_value());
  EXPECT_EQ("test_key", proto_attributes.key());
  EXPECT_EQ("test_value", proto_attributes.value().string_value());
}

TEST(OtlpRecordable, SetStartTime)
{
  OtlpRecordable rec;
  std::chrono::system_clock::time_point start_time = std::chrono::system_clock::now();
  common::SystemTimestamp start_timestamp(start_time);

  uint64_t unix_start =
      std::chrono::duration_cast<std::chrono::nanoseconds>(start_time.time_since_epoch()).count();

  rec.SetStartTime(start_timestamp);
  EXPECT_EQ(rec.span().start_time_unix_nano(), unix_start);
}

TEST(OtlpRecordable, SetDuration)
{
  OtlpRecordable rec;
  // Start time is 0
  common::SystemTimestamp start_timestamp;

  std::chrono::nanoseconds duration(10);
  uint64_t unix_end = duration.count();

  rec.SetStartTime(start_timestamp);
  rec.SetDuration(duration);

  EXPECT_EQ(rec.span().end_time_unix_nano(), unix_end);
}

TEST(OtlpRecordable, SetStatus)
{
  OtlpRecordable rec1;
  trace::StatusCode code_error(trace::StatusCode::kError);
  nostd::string_view description = "For test";
  rec1.SetStatus(code_error, description);

  EXPECT_EQ(rec1.span().status().code(), proto::trace::v1::Status_StatusCode(code_error));
  EXPECT_EQ(rec1.span().status().message(), description);

  OtlpRecordable rec2;
  trace::StatusCode code_ok(trace::StatusCode::kOk);
  rec2.SetStatus(code_ok, description);
  EXPECT_EQ(rec2.span().status().code(), proto::trace::v1::Status_StatusCode(code_ok));
  EXPECT_EQ(rec2.span().status().message(), "");
}

TEST(OtlpRecordable, AddEventDefault)
{
  OtlpRecordable rec;
  nostd::string_view name = "Test Event";

  std::chrono::system_clock::time_point event_time = std::chrono::system_clock::now();
  common::SystemTimestamp event_timestamp(event_time);

  rec.sdk::trace::Recordable::AddEvent(name, event_timestamp);

  uint64_t unix_event_time =
      std::chrono::duration_cast<std::chrono::nanoseconds>(event_time.time_since_epoch()).count();

  EXPECT_EQ(rec.span().events(0).name(), name);
  EXPECT_EQ(rec.span().events(0).time_unix_nano(), unix_event_time);
  EXPECT_EQ(rec.span().events(0).attributes().size(), 0);
}

TEST(OtlpRecordable, AddEventWithAttributes)
{
  OtlpRecordable rec;
  const int kNumAttributes              = 3;
  std::string keys[kNumAttributes]      = {"attr1", "attr2", "attr3"};
  int values[kNumAttributes]            = {4, 7, 23};
  std::map<std::string, int> attributes = {
      {keys[0], values[0]}, {keys[1], values[1]}, {keys[2], values[2]}};

  rec.AddEvent("Test Event", std::chrono::system_clock::now(),
               common::KeyValueIterableView<std::map<std::string, int>>(attributes));

  for (int i = 0; i < kNumAttributes; i++)
  {
    EXPECT_EQ(rec.span().events(0).attributes(i).key(), keys[i]);
    EXPECT_EQ(rec.span().events(0).attributes(i).value().int_value(), values[i]);
  }
}

TEST(OtlpRecordable, AddLink)
{
  OtlpRecordable rec;
  const int kNumAttributes              = 3;
  std::string keys[kNumAttributes]      = {"attr1", "attr2", "attr3"};
  int values[kNumAttributes]            = {5, 12, 40};
  std::map<std::string, int> attributes = {
      {keys[0], values[0]}, {keys[1], values[1]}, {keys[2], values[2]}};

  auto trace_id = rec.span().trace_id();
  auto span_id  = rec.span().span_id();

  trace::TraceFlags flags;
  std::string trace_state_header = "k1=v1,k2=v2";
  auto ts                        = trace::TraceState::FromHeader(trace_state_header);

  rec.AddLink(trace::SpanContext(trace::TraceId(), trace::SpanId(), flags, false, ts),
              common::KeyValueIterableView<std::map<std::string, int>>(attributes));

  EXPECT_EQ(rec.span().trace_id(), trace_id);
  EXPECT_EQ(rec.span().span_id(), span_id);
  EXPECT_EQ(rec.span().links(0).trace_state(), trace_state_header);
  for (int i = 0; i < kNumAttributes; i++)
  {
    EXPECT_EQ(rec.span().links(0).attributes(i).key(), keys[i]);
    EXPECT_EQ(rec.span().links(0).attributes(i).value().int_value(), values[i]);
  }
}

TEST(OtlpRecordable, SetResource)
{
  OtlpRecordable rec;
  const std::string service_name_key = "service.name";
  std::string service_name           = "test-otlp";
  auto resource = resource::Resource::Create({{service_name_key, service_name}});
  rec.SetResource(resource);

  auto proto_resource     = rec.ProtoResource();
  bool found_service_name = false;
  for (int i = 0; i < proto_resource.attributes_size(); i++)
  {
    const auto &attr = proto_resource.attributes(static_cast<int>(i));
    if (attr.key() == service_name_key && attr.value().string_value() == service_name)
    {
      found_service_name = true;
    }
  }
  EXPECT_TRUE(found_service_name);
}

TEST(OtlpRecordable, SetResourceWithSchemaURL)
{
  OtlpRecordable rec;
  const std::string service_name_key    = "service.name";
  const std::string service_name        = "test-otlp";
  const std::string expected_schema_url = "https://opentelemetry.io/schemas/1.11.0";
  auto resource =
      resource::Resource::Create({{service_name_key, service_name}}, expected_schema_url);
  rec.SetResource(resource);

  EXPECT_EQ(expected_schema_url, rec.GetResourceSchemaURL());
}

// Test non-int single types. Int single types are tested using templates (see IntAttributeTest)
TEST(OtlpRecordable, SetSingleAttribute)
{
  OtlpRecordable rec;
  nostd::string_view bool_key = "bool_attr";
  common::AttributeValue bool_val(true);
  rec.SetAttribute(bool_key, bool_val);

  nostd::string_view double_key = "double_attr";
  common::AttributeValue double_val(3.3);
  rec.SetAttribute(double_key, double_val);

  nostd::string_view str_key = "str_attr";
  common::AttributeValue str_val(nostd::string_view("Test"));
  rec.SetAttribute(str_key, str_val);

  EXPECT_EQ(rec.span().attributes(0).key(), bool_key);
  EXPECT_EQ(rec.span().attributes(0).value().bool_value(), nostd::get<bool>(bool_val));

  EXPECT_EQ(rec.span().attributes(1).key(), double_key);
  EXPECT_EQ(rec.span().attributes(1).value().double_value(), nostd::get<double>(double_val));

  EXPECT_EQ(rec.span().attributes(2).key(), str_key);
  EXPECT_EQ(rec.span().attributes(2).value().string_value(),
            nostd::get<nostd::string_view>(str_val).data());
}

// Test non-int array types. Int array types are tested using templates (see IntAttributeTest)
TEST(OtlpRecordable, SetArrayAttribute)
{
  OtlpRecordable rec;
  const int kArraySize = 3;

  bool bool_arr[kArraySize] = {true, false, true};
  nostd::span<const bool> bool_span(bool_arr);
  rec.SetAttribute("bool_arr_attr", bool_span);

  double double_arr[kArraySize] = {22.3, 33.4, 44.5};
  nostd::span<const double> double_span(double_arr);
  rec.SetAttribute("double_arr_attr", double_span);

  nostd::string_view str_arr[kArraySize] = {"Hello", "World", "Test"};
  nostd::span<const nostd::string_view> str_span(str_arr);
  rec.SetAttribute("str_arr_attr", str_span);

  for (int i = 0; i < kArraySize; i++)
  {
    EXPECT_EQ(rec.span().attributes(0).value().array_value().values(i).bool_value(), bool_span[i]);
    EXPECT_EQ(rec.span().attributes(1).value().array_value().values(i).double_value(),
              double_span[i]);
    EXPECT_EQ(rec.span().attributes(2).value().array_value().values(i).string_value(), str_span[i]);
  }
}

// Test otlp resource populate request util
TEST(OtlpRecordable, PopulateRequest)
{
  auto rec1      = std::unique_ptr<sdk::trace::Recordable>(new OtlpRecordable);
  auto resource1 = resource::Resource::Create({{"service.name", "one"}});
  rec1->SetResource(resource1);
  auto inst_lib1 = trace_sdk::InstrumentationScope::Create("one", "1", "scope_schema",
                                                           {{"scope_key", "scope_value"}});
  rec1->SetInstrumentationScope(*inst_lib1);

  auto rec2      = std::unique_ptr<sdk::trace::Recordable>(new OtlpRecordable);
  auto resource2 = resource::Resource::Create({{"service.name", "two"}});
  rec2->SetResource(resource2);
  auto inst_lib2 = trace_sdk::InstrumentationScope::Create("two", "2");
  rec2->SetInstrumentationScope(*inst_lib2);

  // This has the same resource as rec2, but a different scope
  auto rec3 = std::unique_ptr<sdk::trace::Recordable>(new OtlpRecordable);
  rec3->SetResource(resource2);
  auto inst_lib3 = trace_sdk::InstrumentationScope::Create("three", "3");
  rec3->SetInstrumentationScope(*inst_lib3);

  proto::collector::trace::v1::ExportTraceServiceRequest req;
  std::vector<std::unique_ptr<sdk::trace::Recordable>> spans;
  spans.push_back(std::move(rec1));
  spans.push_back(std::move(rec2));
  spans.push_back(std::move(rec3));
  const nostd::span<std::unique_ptr<sdk::trace::Recordable>, 3> spans_span(spans.data(), 3);
  OtlpRecordableUtils::PopulateRequest(spans_span, &req);

  EXPECT_EQ(req.resource_spans().size(), 2);
  for (const auto &resource_spans : req.resource_spans())
  {
    ASSERT_GT(resource_spans.resource().attributes_size(), 0);
    const auto service_name     = resource_spans.resource().attributes(0).value().string_value();
    const auto scope_spans_size = resource_spans.scope_spans().size();
    if (service_name == "one")
    {
      ASSERT_GT(resource_spans.scope_spans_size(), 0);
      const auto &scope_one = resource_spans.scope_spans(0).scope();

      EXPECT_EQ(scope_spans_size, 1);
      EXPECT_EQ(scope_one.name(), "one");
      EXPECT_EQ(scope_one.version(), "1");

      ASSERT_EQ(scope_one.attributes_size(), 1);
      const auto &scope_attribute = scope_one.attributes(0);

      EXPECT_EQ(scope_attribute.key(), "scope_key");
      EXPECT_EQ(scope_attribute.value().string_value(), "scope_value");
    }
    if (service_name == "two")
    {
      EXPECT_EQ(scope_spans_size, 2);
    }
  }
}

// Test otlp resource populate request util with missing data
TEST(OtlpRecordable, PopulateRequestMissing)
{
  // Missing scope
  auto rec1      = std::unique_ptr<sdk::trace::Recordable>(new OtlpRecordable);
  auto resource1 = resource::Resource::Create({{"service.name", "one"}});
  rec1->SetResource(resource1);

  // Missing resource
  auto rec2      = std::unique_ptr<sdk::trace::Recordable>(new OtlpRecordable);
  auto inst_lib2 = trace_sdk::InstrumentationScope::Create("two", "2");
  rec2->SetInstrumentationScope(*inst_lib2);

  proto::collector::trace::v1::ExportTraceServiceRequest req;
  std::vector<std::unique_ptr<sdk::trace::Recordable>> spans;
  spans.push_back(std::move(rec1));
  spans.push_back(std::move(rec2));
  const nostd::span<std::unique_ptr<sdk::trace::Recordable>, 2> spans_span(spans.data(), 2);
  OtlpRecordableUtils::PopulateRequest(spans_span, &req);

  EXPECT_EQ(req.resource_spans().size(), 2);
  for (const auto &resource_spans : req.resource_spans())
  {
    // Both should have scope spans
    EXPECT_EQ(resource_spans.scope_spans().size(), 1);
    auto scope = resource_spans.scope_spans(0).scope();
    // Select the one with missing scope
    if (scope.name() == "")
    {
      // Version is also empty
      EXPECT_EQ(scope.version(), "");
    }
    else
    {
      // The other has a name and version
      EXPECT_EQ(scope.name(), "two");
      EXPECT_EQ(scope.version(), "2");
    }
  }
}

template <typename T>
struct EmptyArrayAttributeTest : public testing::Test
{
  using ElementType = T;
};

using ArrayElementTypes =
    testing::Types<bool, double, nostd::string_view, uint8_t, int, int64_t, unsigned int, uint64_t>;
TYPED_TEST_SUITE(EmptyArrayAttributeTest, ArrayElementTypes);

// Test empty arrays.
TYPED_TEST(EmptyArrayAttributeTest, SetEmptyArrayAttribute)
{
  using ArrayElementType = typename TestFixture::ElementType;
  OtlpRecordable rec;

  nostd::span<const ArrayElementType> span = {};
  rec.SetAttribute("empty_arr_attr", span);

  EXPECT_TRUE(rec.span().attributes(0).value().has_array_value());
  EXPECT_TRUE(rec.span().attributes(0).value().array_value().values().empty());
}

/**
 * AttributeValue can contain different int types, such as int, int64_t,
 * unsigned int, and uint64_t. To avoid writing test cases for each, we can
 * use a template approach to test all int types.
 */
template <typename T>
struct IntAttributeTest : public testing::Test
{
  using IntParamType = T;
};

using IntTypes = testing::Types<int, int64_t, unsigned int, uint64_t>;
TYPED_TEST_SUITE(IntAttributeTest, IntTypes);

TYPED_TEST(IntAttributeTest, SetIntSingleAttribute)
{
  using IntType = typename TestFixture::IntParamType;
  IntType i     = 2;
  common::AttributeValue int_val(i);

  OtlpRecordable rec;
  rec.SetAttribute("int_attr", int_val);
  EXPECT_EQ(rec.span().attributes(0).value().int_value(), nostd::get<IntType>(int_val));
}

TYPED_TEST(IntAttributeTest, SetIntArrayAttribute)
{
  using IntType = typename TestFixture::IntParamType;

  const int kArraySize        = 3;
  IntType int_arr[kArraySize] = {4, 5, 6};
  nostd::span<const IntType> int_span(int_arr);

  OtlpRecordable rec;
  rec.SetAttribute("int_arr_attr", int_span);

  for (int i = 0; i < kArraySize; i++)
  {
    EXPECT_EQ(rec.span().attributes(0).value().array_value().values(i).int_value(), int_span[i]);
  }
}
}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
