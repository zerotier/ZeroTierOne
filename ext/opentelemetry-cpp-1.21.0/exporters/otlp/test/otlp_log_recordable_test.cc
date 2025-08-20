// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <stdint.h>
#include <chrono>
#include <string>
#include <utility>

#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/exporters/otlp/otlp_log_recordable.h"
#include "opentelemetry/logs/severity.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/unique_ptr.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/logs/readable_log_record.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/trace/span_id.h"
#include "opentelemetry/trace/trace_id.h"
#include "opentelemetry/version.h"

// clang-format off
#include "opentelemetry/exporters/otlp/protobuf_include_prefix.h" // IWYU pragma: keep
// IWYU pragma: no_include "net/proto2/public/repeated_field.h"
#include "opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/logs/v1/logs.pb.h"
#include "opentelemetry/exporters/otlp/protobuf_include_suffix.h" // IWYU pragma: keep
// clang-format on

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{
namespace resource = opentelemetry::sdk::resource;
namespace proto    = opentelemetry::proto;

TEST(OtlpLogRecordable, Basic)
{
  OtlpLogRecordable rec;
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
  common::SystemTimestamp start_timestamp(now);

  uint64_t unix_start =
      std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

  rec.SetTimestamp(start_timestamp);
  rec.SetSeverity(opentelemetry::logs::Severity::kError);
  nostd::string_view name = "Test Log Message";
  rec.SetBody(name);

  uint8_t trace_id_bin[opentelemetry::trace::TraceId::kSize] = {
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
  std::string expected_trace_id_bytes;
  expected_trace_id_bytes.assign(
      reinterpret_cast<char *>(trace_id_bin),
      reinterpret_cast<char *>(trace_id_bin) + opentelemetry::trace::TraceId::kSize);
  rec.SetTraceId(opentelemetry::trace::TraceId{trace_id_bin});
  uint8_t span_id_bin[opentelemetry::trace::SpanId::kSize] = {'7', '6', '5', '4',
                                                              '3', '2', '1', '0'};
  std::string expected_span_id_bytes;
  expected_span_id_bytes.assign(
      reinterpret_cast<char *>(span_id_bin),
      reinterpret_cast<char *>(span_id_bin) + opentelemetry::trace::SpanId::kSize);
  rec.SetSpanId(opentelemetry::trace::SpanId{span_id_bin});

  EXPECT_EQ(rec.log_record().time_unix_nano(), unix_start);
  EXPECT_EQ(rec.log_record().severity_number(), proto::logs::v1::SEVERITY_NUMBER_ERROR);
  EXPECT_EQ(rec.log_record().severity_text(), "ERROR");
  EXPECT_EQ(rec.log_record().body().string_value(), name);
  EXPECT_EQ(rec.log_record().trace_id(), expected_trace_id_bytes);
  EXPECT_EQ(rec.log_record().span_id(), expected_span_id_bytes);
}

TEST(OtlpLogRecordable, GetResource)
{
  OtlpLogRecordable rec;
  const std::string service_name_key = "service.name";
  std::string service_name           = "test-otlp";
  auto resource =
      opentelemetry::sdk::resource::Resource::Create({{service_name_key, service_name}});
  rec.SetResource(resource);

  EXPECT_EQ(&rec.GetResource(), &resource);
}

TEST(OtlpLogRecordable, DefaultResource)
{
  OtlpLogRecordable rec;

  EXPECT_EQ(&rec.GetResource(), &opentelemetry::sdk::logs::ReadableLogRecord::GetDefaultResource());
}

// Test non-int single types. Int single types are tested using templates (see IntAttributeTest)
TEST(OtlpLogRecordable, SetSingleAttribute)
{
  OtlpLogRecordable rec;

  nostd::string_view bool_key = "bool_attr";
  common::AttributeValue bool_val(true);
  rec.SetAttribute(bool_key, bool_val);

  nostd::string_view double_key = "double_attr";
  common::AttributeValue double_val(3.3);
  rec.SetAttribute(double_key, double_val);

  nostd::string_view str_key = "str_attr";
  common::AttributeValue str_val(nostd::string_view("Test"));
  rec.SetAttribute(str_key, str_val);

  int checked_attributes = 0;
  for (auto &attribute : rec.log_record().attributes())
  {
    if (attribute.key() == bool_key)
    {
      ++checked_attributes;
      EXPECT_EQ(attribute.value().bool_value(), nostd::get<bool>(bool_val));
    }
    else if (attribute.key() == double_key)
    {
      ++checked_attributes;
      EXPECT_EQ(attribute.value().double_value(), nostd::get<double>(double_val));
    }
    else if (attribute.key() == str_key)
    {
      ++checked_attributes;
      EXPECT_EQ(attribute.value().string_value(), nostd::get<nostd::string_view>(str_val).data());
    }
  }
  EXPECT_EQ(3, checked_attributes);
}

// Test non-int array types. Int array types are tested using templates (see IntAttributeTest)
TEST(OtlpLogRecordable, SetArrayAttribute)
{
  OtlpLogRecordable rec;

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
    EXPECT_EQ(rec.log_record().attributes(0).value().array_value().values(i).bool_value(),
              bool_span[i]);
    EXPECT_EQ(rec.log_record().attributes(1).value().array_value().values(i).double_value(),
              double_span[i]);
    EXPECT_EQ(rec.log_record().attributes(2).value().array_value().values(i).string_value(),
              str_span[i]);
  }
}

TEST(OtlpLogRecordable, SetInstrumentationScope)
{
  OtlpLogRecordable rec;

  auto inst_lib =
      opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create("test", "v1");
  rec.SetInstrumentationScope(*inst_lib);

  EXPECT_EQ(&rec.GetInstrumentationScope(), inst_lib.get());
}

TEST(OtlpLogRecordable, SetEventName)
{
  OtlpLogRecordable rec;

  nostd::string_view event_name = "Test Event";
  rec.SetEventId(0, event_name);

  EXPECT_EQ(rec.log_record().event_name(), event_name);
}

/**
 * AttributeValue can contain different int types, such as int, int64_t,
 * unsigned int, and uint64_t. To avoid writing test cases for each, we can
 * use a template approach to test all int types.
 */
template <typename T>
struct OtlpLogRecordableIntAttributeTest : public testing::Test
{
  using IntParamType = T;
};

using IntTypes = testing::Types<int, int64_t, unsigned int, uint64_t>;
TYPED_TEST_SUITE(OtlpLogRecordableIntAttributeTest, IntTypes);

TYPED_TEST(OtlpLogRecordableIntAttributeTest, SetIntSingleAttribute)
{
  using IntType = typename TestFixture::IntParamType;
  IntType i     = 2;
  common::AttributeValue int_val(i);

  OtlpLogRecordable rec;

  rec.SetAttribute("int_attr", int_val);

  EXPECT_EQ(rec.log_record().attributes(0).value().int_value(), nostd::get<IntType>(int_val));
}

TYPED_TEST(OtlpLogRecordableIntAttributeTest, SetIntArrayAttribute)
{
  using IntType = typename TestFixture::IntParamType;

  const int kArraySize        = 3;
  IntType int_arr[kArraySize] = {4, 5, 6};
  nostd::span<const IntType> int_span(int_arr);

  OtlpLogRecordable rec;

  rec.SetAttribute("int_arr_attr", int_span);

  for (int i = 0; i < kArraySize; i++)
  {
    EXPECT_EQ(rec.log_record().attributes(0).value().array_value().values(i).int_value(),
              int_span[i]);
  }
}
}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
