/*
 * Copyright The OpenTelemetry Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>
#include <stdint.h>
#include <algorithm>
#include <chrono>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "opentracing/propagation.h"
#include "opentracing/span.h"
#include "opentracing/string_view.h"
#include "opentracing/tracer.h"
#include "opentracing/util.h"
#include "opentracing/value.h"
#include "opentracing/variant/recursive_wrapper.hpp"

#include "opentelemetry/baggage/baggage.h"
#include "opentelemetry/baggage/baggage_context.h"
#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/common/key_value_iterable.h"
#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/context/runtime_context.h"
#include "opentelemetry/nostd/function_ref.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/opentracingshim/shim_utils.h"
#include "opentelemetry/opentracingshim/span_context_shim.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/span_startoptions.h"

#include "shim_mocks.h"

namespace trace_api = opentelemetry::trace;
namespace baggage   = opentelemetry::baggage;
namespace common    = opentelemetry::common;
namespace nostd     = opentelemetry::nostd;
namespace shim      = opentelemetry::opentracingshim;

TEST(ShimUtilsTest, IsBaggageEmpty)
{
  auto none = nostd::shared_ptr<baggage::Baggage>(nullptr);
  ASSERT_TRUE(shim::utils::isBaggageEmpty(none));

  auto empty = nostd::shared_ptr<baggage::Baggage>(new baggage::Baggage({}));
  ASSERT_TRUE(shim::utils::isBaggageEmpty(empty));

  std::map<std::string, std::string> list{{"foo", "bar"}};
  auto non_empty = nostd::shared_ptr<baggage::Baggage>(new baggage::Baggage(list));
  ASSERT_FALSE(shim::utils::isBaggageEmpty(non_empty));
}

TEST(ShimUtilsTest, StringFromValue)
{
  ASSERT_EQ(shim::utils::stringFromValue(true), "true");
  ASSERT_EQ(shim::utils::stringFromValue(false), "false");
  ASSERT_EQ(shim::utils::stringFromValue(1234.567890), "1234.567890");
  ASSERT_EQ(shim::utils::stringFromValue(42l), "42");
  ASSERT_EQ(shim::utils::stringFromValue(55ul), "55");
  ASSERT_EQ(shim::utils::stringFromValue(std::string{"a string"}), "a string");
  ASSERT_EQ(shim::utils::stringFromValue(opentracing::string_view{"a string view"}),
            "a string view");
  ASSERT_EQ(shim::utils::stringFromValue(nullptr), "");
  ASSERT_EQ(shim::utils::stringFromValue("a char ptr"), "a char ptr");

  opentracing::util::recursive_wrapper<opentracing::Values> values{};
  ASSERT_EQ(shim::utils::stringFromValue(values.get()), "");

  opentracing::util::recursive_wrapper<opentracing::Dictionary> dict{};
  ASSERT_EQ(shim::utils::stringFromValue(dict.get()), "");
}

TEST(ShimUtilsTest, AttributeFromValue)
{
  auto value = shim::utils::attributeFromValue(true);
  ASSERT_EQ(value.index(), common::AttributeType::kTypeBool);
  ASSERT_TRUE(nostd::get<bool>(value));

  value = shim::utils::attributeFromValue(false);
  ASSERT_EQ(value.index(), common::AttributeType::kTypeBool);
  ASSERT_FALSE(nostd::get<bool>(value));

  value = shim::utils::attributeFromValue(1234.567890);
  ASSERT_EQ(value.index(), common::AttributeType::kTypeDouble);
  ASSERT_EQ(nostd::get<double>(value), 1234.567890);

  value = shim::utils::attributeFromValue(42l);
  ASSERT_EQ(value.index(), common::AttributeType::kTypeInt64);
  ASSERT_EQ(nostd::get<int64_t>(value), 42l);

  value = shim::utils::attributeFromValue(55ul);
  ASSERT_EQ(value.index(), common::AttributeType::kTypeUInt64);
  ASSERT_EQ(nostd::get<uint64_t>(value), 55ul);

  opentracing::Value str{std::string{"a string"}};
  value = shim::utils::attributeFromValue(str);
  ASSERT_EQ(value.index(), common::AttributeType::kTypeString);
  ASSERT_EQ(nostd::get<nostd::string_view>(value), nostd::string_view{"a string"});

  value = shim::utils::attributeFromValue(opentracing::string_view{"a string view"});
  ASSERT_EQ(value.index(), common::AttributeType::kTypeString);
  ASSERT_EQ(nostd::get<nostd::string_view>(value), nostd::string_view{"a string view"});

  value = shim::utils::attributeFromValue(nullptr);
  ASSERT_EQ(value.index(), common::AttributeType::kTypeString);
  ASSERT_EQ(nostd::get<nostd::string_view>(value), nostd::string_view{});

  value = shim::utils::attributeFromValue("a char ptr");
  ASSERT_EQ(value.index(), common::AttributeType::kTypeCString);
  ASSERT_EQ(nostd::get<const char *>(value), "a char ptr");

  opentracing::util::recursive_wrapper<opentracing::Values> values{};
  value = shim::utils::attributeFromValue(values.get());
  ASSERT_EQ(value.index(), common::AttributeType::kTypeString);
  ASSERT_EQ(nostd::get<nostd::string_view>(value), nostd::string_view{});

  opentracing::util::recursive_wrapper<opentracing::Dictionary> dict{};
  value = shim::utils::attributeFromValue(dict.get());
  ASSERT_EQ(value.index(), common::AttributeType::kTypeString);
  ASSERT_EQ(nostd::get<nostd::string_view>(value), nostd::string_view{});
}

TEST(ShimUtilsTest, MakeOptionsShim_EmptyRefs)
{
  opentracing::StartSpanOptions options;
  options.start_system_timestamp = opentracing::SystemTime::time_point::clock::now();
  options.start_steady_timestamp = opentracing::SteadyTime::time_point::clock::now();

  auto options_shim = shim::utils::makeOptionsShim(options);
  ASSERT_EQ(options_shim.start_system_time,
            common::SystemTimestamp{options.start_system_timestamp});
  ASSERT_EQ(options_shim.start_steady_time,
            common::SteadyTimestamp{options.start_steady_timestamp});
  ASSERT_EQ(nostd::get<trace_api::SpanContext>(options_shim.parent),
            trace_api::SpanContext::GetInvalid());
}

TEST(ShimUtilsTest, MakeOptionsShim_InvalidSpanContext)
{
  opentracing::StartSpanOptions options;
  options.start_system_timestamp = opentracing::SystemTime::time_point::clock::now();
  options.start_steady_timestamp = opentracing::SteadyTime::time_point::clock::now();
  options.references             = {{opentracing::SpanReferenceType::FollowsFromRef, nullptr}};

  auto options_shim = shim::utils::makeOptionsShim(options);
  ASSERT_EQ(options_shim.start_system_time,
            common::SystemTimestamp{options.start_system_timestamp});
  ASSERT_EQ(options_shim.start_steady_time,
            common::SteadyTimestamp{options.start_steady_timestamp});
  ASSERT_EQ(nostd::get<trace_api::SpanContext>(options_shim.parent),
            trace_api::SpanContext::GetInvalid());
}

TEST(ShimUtilsTest, MakeOptionsShim_FirstChildOf)
{
  auto span_context_shim = nostd::shared_ptr<shim::SpanContextShim>(new shim::SpanContextShim(
      trace_api::SpanContext::GetInvalid(), baggage::Baggage::GetDefault()));
  auto span_context      = static_cast<opentracing::SpanContext *>(span_context_shim.get());

  opentracing::StartSpanOptions options;
  options.start_system_timestamp = opentracing::SystemTime::time_point::clock::now();
  options.start_steady_timestamp = opentracing::SteadyTime::time_point::clock::now();
  options.references             = {{opentracing::SpanReferenceType::FollowsFromRef, nullptr},
                                    {opentracing::SpanReferenceType::ChildOfRef, span_context},
                                    {opentracing::SpanReferenceType::ChildOfRef, nullptr}};

  auto options_shim = shim::utils::makeOptionsShim(options);
  ASSERT_EQ(options_shim.start_system_time,
            common::SystemTimestamp{options.start_system_timestamp});
  ASSERT_EQ(options_shim.start_steady_time,
            common::SteadyTimestamp{options.start_steady_timestamp});
  ASSERT_EQ(nostd::get<trace_api::SpanContext>(options_shim.parent), span_context_shim->context());
}

TEST(ShimUtilsTest, MakeOptionsShim_FirstInList)
{
  auto span_context_shim = nostd::shared_ptr<shim::SpanContextShim>(new shim::SpanContextShim(
      trace_api::SpanContext::GetInvalid(), baggage::Baggage::GetDefault()));
  auto span_context      = static_cast<opentracing::SpanContext *>(span_context_shim.get());

  opentracing::StartSpanOptions options;
  options.start_system_timestamp = opentracing::SystemTime::time_point::clock::now();
  options.start_steady_timestamp = opentracing::SteadyTime::time_point::clock::now();
  options.references             = {{opentracing::SpanReferenceType::FollowsFromRef, span_context},
                                    {opentracing::SpanReferenceType::FollowsFromRef, nullptr}};

  auto options_shim = shim::utils::makeOptionsShim(options);
  ASSERT_EQ(options_shim.start_system_time,
            common::SystemTimestamp{options.start_system_timestamp});
  ASSERT_EQ(options_shim.start_steady_time,
            common::SteadyTimestamp{options.start_steady_timestamp});
  ASSERT_EQ(nostd::get<trace_api::SpanContext>(options_shim.parent), span_context_shim->context());
}

TEST(ShimUtilsTest, MakeIterableLinks)
{
  auto span_context_shim1 = nostd::shared_ptr<shim::SpanContextShim>(new shim::SpanContextShim(
      trace_api::SpanContext::GetInvalid(), baggage::Baggage::GetDefault()));
  auto span_context1      = static_cast<opentracing::SpanContext *>(span_context_shim1.get());
  auto span_context_shim2 = nostd::shared_ptr<shim::SpanContextShim>(new shim::SpanContextShim(
      trace_api::SpanContext::GetInvalid(), baggage::Baggage::GetDefault()));
  auto span_context2      = static_cast<opentracing::SpanContext *>(span_context_shim2.get());

  opentracing::StartSpanOptions options;
  auto empty = shim::utils::makeIterableLinks(options);
  ASSERT_EQ(empty.size(), 0);

  options.references = {{opentracing::SpanReferenceType::FollowsFromRef, nullptr},
                        {opentracing::SpanReferenceType::FollowsFromRef, span_context1},
                        {opentracing::SpanReferenceType::ChildOfRef, span_context2}};
  auto full          = shim::utils::makeIterableLinks(options);
  ASSERT_EQ(full.size(), 3);

  std::vector<std::tuple<trace_api::SpanContext, nostd::string_view, common::AttributeValue>> links;
  full.ForEachKeyValue([&links](trace_api::SpanContext ctx, const common::KeyValueIterable &it) {
    it.ForEachKeyValue([&links, &ctx](nostd::string_view key, common::AttributeValue value) {
      links.emplace_back(ctx, key, value);
      return false;
    });
    return true;
  });
  ASSERT_EQ(links.size(), 2);

  trace_api::SpanContext ctx = trace_api::SpanContext::GetInvalid();
  nostd::string_view key;
  common::AttributeValue value;
  std::tie(ctx, key, value) = links[0];
  ASSERT_EQ(ctx, span_context_shim1->context());
  ASSERT_EQ(key, "opentracing.ref_type");
  ASSERT_EQ(nostd::get<nostd::string_view>(value), "follows_from");
  std::tie(ctx, key, value) = links[1];
  ASSERT_EQ(ctx, span_context_shim2->context());
  ASSERT_EQ(key, "opentracing.ref_type");
  ASSERT_EQ(nostd::get<nostd::string_view>(value), "child_of");
}

TEST(ShimUtilsTest, MakeBaggage_EmptyRefs)
{
  auto baggage = baggage::Baggage::GetDefault()->Set("foo", "bar");
  std::string value;
  ASSERT_TRUE(baggage->GetValue("foo", value));
  ASSERT_EQ(value, "bar");

  auto context     = context::RuntimeContext::GetCurrent();
  auto new_context = baggage::SetBaggage(context, baggage);
  auto token       = context::RuntimeContext::Attach(new_context);
  ASSERT_EQ(context::RuntimeContext::GetCurrent(), new_context);

  opentracing::StartSpanOptions options;
  auto new_baggage = shim::utils::makeBaggage(options);
  ASSERT_TRUE(new_baggage->GetValue("foo", value));
  ASSERT_EQ(value, "bar");
}

TEST(ShimUtilsTest, MakeBaggage_NonEmptyRefs)
{
  auto span_context_shim1 = nostd::shared_ptr<shim::SpanContextShim>(new shim::SpanContextShim(
      trace_api::SpanContext::GetInvalid(),
      baggage::Baggage::GetDefault()->Set("test", "foo")->Set("test1", "hello")));
  auto span_context1      = static_cast<opentracing::SpanContext *>(span_context_shim1.get());
  auto span_context_shim2 = nostd::shared_ptr<shim::SpanContextShim>(new shim::SpanContextShim(
      trace_api::SpanContext::GetInvalid(),
      baggage::Baggage::GetDefault()->Set("test", "bar")->Set("test2", "world")));
  auto span_context2      = static_cast<opentracing::SpanContext *>(span_context_shim2.get());

  opentracing::StartSpanOptions options;
  options.references = {{opentracing::SpanReferenceType::FollowsFromRef, span_context1},
                        {opentracing::SpanReferenceType::ChildOfRef, span_context2}};

  auto baggage = shim::utils::makeBaggage(options);
  std::string value;
  ASSERT_TRUE(baggage->GetValue("test", value));
  ASSERT_EQ(value, "foo");
  ASSERT_TRUE(baggage->GetValue("test1", value));
  ASSERT_EQ(value, "hello");
  ASSERT_TRUE(baggage->GetValue("test2", value));
  ASSERT_EQ(value, "world");
}

TEST(ShimUtilsTest, MakeIterableTags)
{
  opentracing::StartSpanOptions options;
  auto empty = shim::utils::makeIterableTags(options);
  ASSERT_EQ(empty.size(), 0);

  options.tags = {{"foo", 42.0}, {"bar", true}, {"baz", "test"}};
  auto full    = shim::utils::makeIterableTags(options);
  ASSERT_EQ(full.size(), 3);

  std::vector<std::pair<nostd::string_view, common::AttributeValue>> attributes;
  full.ForEachKeyValue([&attributes](nostd::string_view key, common::AttributeValue value) {
    attributes.push_back({key, value});
    return true;
  });
  ASSERT_EQ(attributes[0].first, "foo");
  ASSERT_EQ(nostd::get<double>(attributes[0].second), 42.0);
  ASSERT_EQ(attributes[1].first, "bar");
  ASSERT_TRUE(nostd::get<bool>(attributes[1].second));
  ASSERT_EQ(attributes[2].first, "baz");
  ASSERT_STREQ(nostd::get<const char *>(attributes[2].second), "test");
}
