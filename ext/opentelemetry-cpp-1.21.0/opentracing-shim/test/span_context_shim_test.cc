/*
 * Copyright The OpenTelemetry Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>
#include <algorithm>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "opentracing/noop.h"
#include "opentracing/span.h"
#include "opentracing/tracer.h"
#include "opentracing/value.h"

#include "opentelemetry/baggage/baggage.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/unique_ptr.h"
#include "opentelemetry/opentracingshim/span_context_shim.h"
#include "opentelemetry/trace/span_context.h"

namespace trace_api = opentelemetry::trace;
namespace baggage   = opentelemetry::baggage;
namespace nostd     = opentelemetry::nostd;
namespace shim      = opentelemetry::opentracingshim;

class SpanContextShimTest : public testing::Test
{
public:
  nostd::unique_ptr<shim::SpanContextShim> span_context_shim;

protected:
  virtual void SetUp() override
  {
    auto span_context = trace_api::SpanContext::GetInvalid();
    auto baggage      = baggage::Baggage::GetDefault()->Set("foo", "bar");
    span_context_shim =
        nostd::unique_ptr<shim::SpanContextShim>(new shim::SpanContextShim(span_context, baggage));
  }

  virtual void TearDown() override { span_context_shim.reset(); }
};

TEST_F(SpanContextShimTest, ExtractFrom)
{
  ASSERT_TRUE(shim::SpanContextShim::extractFrom(nullptr) == nullptr);

  auto tracer = opentracing::MakeNoopTracer();
  auto span   = tracer->StartSpanWithOptions("operation", {});
  ASSERT_TRUE(shim::SpanContextShim::extractFrom(&span->context()) == nullptr);

  auto span_context_shim = nostd::shared_ptr<shim::SpanContextShim>(new shim::SpanContextShim(
      trace_api::SpanContext::GetInvalid(), baggage::Baggage::GetDefault()));
  ASSERT_TRUE(shim::SpanContextShim::extractFrom(span_context_shim.get()) != nullptr);
}

TEST_F(SpanContextShimTest, BaggageItem)
{
  std::string value;
  ASSERT_TRUE(span_context_shim->BaggageItem("foo", value));
  ASSERT_EQ(value, "bar");
  ASSERT_FALSE(span_context_shim->BaggageItem("", value));
}

TEST_F(SpanContextShimTest, NewWithKeyValue)
{
  auto new_span_context_shim = span_context_shim->newWithKeyValue("test", "this");
  ASSERT_NE(span_context_shim.get(), &new_span_context_shim);
  ASSERT_EQ(span_context_shim->context(), new_span_context_shim.context());
  ASSERT_EQ(span_context_shim->context().IsValid(), new_span_context_shim.context().IsValid());
  ASSERT_EQ(span_context_shim->context().IsRemote(), new_span_context_shim.context().IsRemote());

  std::string value;
  ASSERT_TRUE(new_span_context_shim.BaggageItem("foo", value));
  ASSERT_EQ(value, "bar");
  ASSERT_TRUE(new_span_context_shim.BaggageItem("test", value));
  ASSERT_EQ(value, "this");
}

TEST_F(SpanContextShimTest, ForeachBaggageItem)
{
  std::initializer_list<std::pair<std::string, std::string>> list{
      {"foo", "bar"}, {"bar", "baz"}, {"baz", "foo"}};
  nostd::shared_ptr<baggage::Baggage> baggage(new baggage::Baggage(list));
  shim::SpanContextShim new_span_context_shim(span_context_shim->context(), baggage);

  std::vector<std::string> concatenated;
  new_span_context_shim.ForeachBaggageItem(
      [&concatenated](const std::string &key, const std::string &value) {
        concatenated.emplace_back(key + ":" + value);
        return true;
      });

  ASSERT_EQ(concatenated.size(), 3);
  ASSERT_EQ(concatenated[0], "foo:bar");
  ASSERT_EQ(concatenated[1], "bar:baz");
  ASSERT_EQ(concatenated[2], "baz:foo");
}

TEST_F(SpanContextShimTest, Clone)
{
  auto new_span_context      = span_context_shim->Clone();
  auto new_span_context_shim = static_cast<shim::SpanContextShim *>(new_span_context.get());
  ASSERT_TRUE(new_span_context_shim != nullptr);
  ASSERT_NE(span_context_shim.get(), new_span_context_shim);
  ASSERT_EQ(span_context_shim->context(), new_span_context_shim->context());
  ASSERT_EQ(span_context_shim->context().IsValid(), new_span_context_shim->context().IsValid());
  ASSERT_EQ(span_context_shim->context().IsRemote(), new_span_context_shim->context().IsRemote());

  std::string value;
  std::string new_value;
  ASSERT_TRUE(span_context_shim->BaggageItem("foo", value));
  ASSERT_TRUE(new_span_context_shim->BaggageItem("foo", new_value));
  ASSERT_EQ(value, new_value);
}
