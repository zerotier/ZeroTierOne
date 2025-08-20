/*
 * Copyright The OpenTelemetry Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>
#include <stdint.h>
#include <iostream>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#include "opentracing/expected/expected.hpp"
#include "opentracing/noop.h"
#include "opentracing/propagation.h"
#include "opentracing/span.h"
#include "opentracing/tracer.h"
#include "opentracing/util.h"
#include "opentracing/value.h"

#include "opentelemetry/baggage/baggage.h"
#include "opentelemetry/common/key_value_iterable.h"
#include "opentelemetry/context/propagation/text_map_propagator.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/opentracingshim/shim_utils.h"
#include "opentelemetry/opentracingshim/span_context_shim.h"
#include "opentelemetry/opentracingshim/tracer_shim.h"
#include "opentelemetry/trace/default_span.h"
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/trace/span.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/span_context_kv_iterable.h"
#include "opentelemetry/trace/span_id.h"
#include "opentelemetry/trace/span_metadata.h"
#include "opentelemetry/trace/span_startoptions.h"
#include "opentelemetry/trace/trace_flags.h"
#include "opentelemetry/trace/trace_id.h"
#include "opentelemetry/trace/tracer.h"
#include "opentelemetry/trace/tracer_provider.h"

#include "shim_mocks.h"

namespace trace_api = opentelemetry::trace;
namespace nostd     = opentelemetry::nostd;
namespace context   = opentelemetry::context;
namespace baggage   = opentelemetry::baggage;
namespace shim      = opentelemetry::opentracingshim;

class TracerShimTest : public testing::Test
{
public:
  std::shared_ptr<opentracing::Tracer> tracer_shim;
  MockPropagator *text_map_format;
  MockPropagator *http_headers_format;

protected:
  virtual void SetUp() override
  {
    using context::propagation::TextMapPropagator;

    text_map_format     = new MockPropagator();
    http_headers_format = new MockPropagator();

    tracer_shim = shim::TracerShim::createTracerShim(
        trace_api::Provider::GetTracerProvider(),
        {.text_map     = nostd::shared_ptr<TextMapPropagator>(text_map_format),
         .http_headers = nostd::shared_ptr<TextMapPropagator>(http_headers_format)});
  }

  virtual void TearDown() override { tracer_shim.reset(); }
};

TEST_F(TracerShimTest, TracerName)
{
  auto mock_provider_ptr = new MockTracerProvider();
  nostd::shared_ptr<trace_api::TracerProvider> provider(mock_provider_ptr);
  ASSERT_NE(shim::TracerShim::createTracerShim(provider), nullptr);
  ASSERT_EQ(mock_provider_ptr->library_name_, "opentracing-shim");
}

TEST_F(TracerShimTest, SpanReferenceToCreatingTracer)
{
  auto span_shim = tracer_shim->StartSpan("a");
  ASSERT_NE(span_shim, nullptr);
  ASSERT_EQ(&span_shim->tracer(), tracer_shim.get());
}

TEST_F(TracerShimTest, SpanParentChildRelationship)
{
  auto span_shim1 = tracer_shim->StartSpan("a");
  auto span_shim2 = tracer_shim->StartSpan("b", {opentracing::ChildOf(&span_shim1->context())});
  ASSERT_NE(span_shim1, nullptr);
  ASSERT_NE(span_shim2, nullptr);
  ASSERT_NE(span_shim1, span_shim2);
  ASSERT_EQ(span_shim1->context().ToSpanID(), span_shim2->context().ToSpanID());
  ASSERT_EQ(span_shim1->context().ToTraceID(), span_shim2->context().ToTraceID());

  auto span_context_shim1 = static_cast<const shim::SpanContextShim *>(&span_shim1->context());
  auto span_context_shim2 = static_cast<const shim::SpanContextShim *>(&span_shim2->context());
  ASSERT_TRUE(span_context_shim1 != nullptr);
  ASSERT_TRUE(span_context_shim2 != nullptr);
  ASSERT_EQ(span_context_shim1->context(), span_context_shim2->context());
}

TEST_F(TracerShimTest, TracerGloballyRegistered)
{
  ASSERT_FALSE(opentracing::Tracer::IsGlobalTracerRegistered());
  ASSERT_NE(opentracing::Tracer::InitGlobal(tracer_shim), nullptr);
  ASSERT_TRUE(opentracing::Tracer::IsGlobalTracerRegistered());
}

TEST_F(TracerShimTest, Close)
{
  tracer_shim->Close();
  auto span_shim = tracer_shim->StartSpan("a");
  ASSERT_TRUE(span_shim == nullptr);
}

TEST_F(TracerShimTest, SpanHandleErrorTagAtCreation)
{
  auto mock_provider_ptr = new MockTracerProvider();
  nostd::shared_ptr<trace_api::TracerProvider> provider(mock_provider_ptr);
  auto tracer_shim = shim::TracerShim::createTracerShim(provider);
  auto span_shim   = tracer_shim->StartSpanWithOptions("test", {});
  ASSERT_TRUE(span_shim != nullptr);
  ASSERT_TRUE(mock_provider_ptr->tracer_ != nullptr);
  ASSERT_TRUE(mock_provider_ptr->tracer_->span_ != nullptr);
  ASSERT_EQ(mock_provider_ptr->tracer_->span_->name_, "test");
  ASSERT_EQ(mock_provider_ptr->tracer_->span_->status_.first, trace_api::StatusCode::kUnset);

  auto span_shim1 = tracer_shim->StartSpanWithOptions("test1", {.tags = {{"event", "normal"}}});
  ASSERT_TRUE(span_shim1 != nullptr);
  ASSERT_EQ(mock_provider_ptr->tracer_->span_->name_, "test1");
  ASSERT_EQ(mock_provider_ptr->tracer_->span_->status_.first, trace_api::StatusCode::kUnset);

  auto span_shim2 = tracer_shim->StartSpanWithOptions("test2", {.tags = {{"error", true}}});
  ASSERT_TRUE(span_shim2 != nullptr);
  ASSERT_EQ(mock_provider_ptr->tracer_->span_->name_, "test2");
  ASSERT_EQ(mock_provider_ptr->tracer_->span_->status_.first, trace_api::StatusCode::kError);

  auto span_shim3 = tracer_shim->StartSpanWithOptions("test3", {.tags = {{"error", "false"}}});
  ASSERT_TRUE(span_shim3 != nullptr);
  ASSERT_EQ(mock_provider_ptr->tracer_->span_->name_, "test3");
  ASSERT_EQ(mock_provider_ptr->tracer_->span_->status_.first, trace_api::StatusCode::kOk);
}

TEST_F(TracerShimTest, InjectInvalidCarrier)
{
  auto span_shim = tracer_shim->StartSpan("a");
  auto result    = tracer_shim->Inject(span_shim->context(), std::cout);
  ASSERT_TRUE(opentracing::are_errors_equal(result.error(), opentracing::invalid_carrier_error));
}

TEST_F(TracerShimTest, InjectNullContext)
{
  std::unordered_map<std::string, std::string> text_map;
  auto noop_tracer = opentracing::MakeNoopTracer();
  auto span        = noop_tracer->StartSpan("a");
  auto result      = tracer_shim->Inject(span->context(), TextMapCarrier{text_map});
  ASSERT_TRUE(
      opentracing::are_errors_equal(result.error(), opentracing::invalid_span_context_error));
  ASSERT_TRUE(text_map.empty());
}

TEST_F(TracerShimTest, InjectTextMap)
{
  ASSERT_FALSE(text_map_format->is_injected);
  ASSERT_FALSE(http_headers_format->is_injected);

  std::unordered_map<std::string, std::string> text_map;
  auto span_shim = tracer_shim->StartSpan("a");
  tracer_shim->Inject(span_shim->context(), TextMapCarrier{text_map});
  ASSERT_TRUE(text_map_format->is_injected);
  ASSERT_FALSE(http_headers_format->is_injected);
}

TEST_F(TracerShimTest, InjectHttpsHeaders)
{
  ASSERT_FALSE(text_map_format->is_injected);
  ASSERT_FALSE(http_headers_format->is_injected);

  std::unordered_map<std::string, std::string> text_map;
  auto span_shim = tracer_shim->StartSpan("a");
  tracer_shim->Inject(span_shim->context(), HTTPHeadersCarrier{text_map});
  ASSERT_FALSE(text_map_format->is_injected);
  ASSERT_TRUE(http_headers_format->is_injected);
}

TEST_F(TracerShimTest, ExtractInvalidCarrier)
{
  auto result = tracer_shim->Extract(std::cin);
  ASSERT_TRUE(opentracing::are_errors_equal(result.error(), opentracing::invalid_carrier_error));
}

TEST_F(TracerShimTest, ExtractNullContext)
{
  std::unordered_map<std::string, std::string> text_map;
  auto result = tracer_shim->Extract(TextMapCarrier{text_map});
  ASSERT_EQ(result.value(), nullptr);
}

TEST_F(TracerShimTest, ExtractTextMap)
{
  ASSERT_FALSE(text_map_format->is_extracted);
  ASSERT_FALSE(http_headers_format->is_extracted);

  std::unordered_map<std::string, std::string> text_map;
  auto result = tracer_shim->Extract(TextMapCarrier{text_map});
  ASSERT_EQ(result.value(), nullptr);
  ASSERT_TRUE(text_map_format->is_extracted);
  ASSERT_FALSE(http_headers_format->is_extracted);
}

TEST_F(TracerShimTest, ExtractHttpsHeaders)
{
  ASSERT_FALSE(text_map_format->is_extracted);
  ASSERT_FALSE(http_headers_format->is_extracted);

  std::unordered_map<std::string, std::string> text_map;
  auto result = tracer_shim->Extract(HTTPHeadersCarrier{text_map});
  ASSERT_EQ(result.value(), nullptr);
  ASSERT_FALSE(text_map_format->is_extracted);
  ASSERT_TRUE(http_headers_format->is_extracted);
}

TEST_F(TracerShimTest, ExtractOnlyBaggage)
{
  std::unordered_map<std::string, std::string> text_map;
  auto span_shim = tracer_shim->StartSpan("a");
  span_shim->SetBaggageItem("foo", "bar");
  ASSERT_EQ(span_shim->BaggageItem("foo"), "bar");

  tracer_shim->Inject(span_shim->context(), TextMapCarrier{text_map});
  auto span_context = tracer_shim->Extract(TextMapCarrier{text_map});
  ASSERT_TRUE(span_context.value() != nullptr);

  auto span_context_shim = static_cast<shim::SpanContextShim *>(span_context.value().get());
  ASSERT_TRUE(span_context_shim != nullptr);
  ASSERT_FALSE(span_context_shim->context().IsValid());
  ASSERT_FALSE(span_context_shim->context().IsSampled());
  ASSERT_FALSE(shim::utils::isBaggageEmpty(span_context_shim->baggage()));

  std::string value;
  ASSERT_TRUE(span_context_shim->BaggageItem("foo", value));
  ASSERT_EQ(value, "bar");
}

class TracerWithSpanContext : public trace_api::Tracer
{
public:
  nostd::shared_ptr<trace_api::Span> StartSpan(
      nostd::string_view name,
      const common::KeyValueIterable & /* attributes */,
      const trace_api::SpanContextKeyValueIterable & /* links */,
      const trace_api::StartSpanOptions & /* options */) noexcept override
  {
    constexpr uint8_t trace_id_buf[] = {1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8};
    trace_api::TraceId trace_id(trace_id_buf);

    constexpr uint8_t span_id_buf[] = {1, 2, 3, 4, 5, 6, 7, 8};
    trace_api::SpanId span_id(span_id_buf);

    auto span_context = trace_api::SpanContext(trace_id, span_id, GetTraceFlags(), false);
    nostd::shared_ptr<trace_api::Span> result(new trace_api::DefaultSpan(span_context));

    return result;
  }

  void ForceFlushWithMicroseconds(uint64_t /* timeout */) noexcept override {}

  void CloseWithMicroseconds(uint64_t /* timeout */) noexcept override {}

  static trace_api::TraceFlags GetTraceFlags()
  {
    return trace_api::TraceFlags(trace_api::TraceFlags::kIsSampled);
  }
};

class TracerWithSpanContextProvider : public trace_api::TracerProvider
{
public:
  static nostd::shared_ptr<trace_api::TracerProvider> Create()
  {
    nostd::shared_ptr<trace_api::TracerProvider> result(new TracerWithSpanContextProvider());
    return result;
  }

#if OPENTELEMETRY_ABI_VERSION_NO >= 2
  nostd::shared_ptr<trace_api::Tracer> GetTracer(
      nostd::string_view /* name */,
      nostd::string_view /* version */,
      nostd::string_view /* schema_url */,
      const common::KeyValueIterable * /* attributes */) noexcept override
  {
    nostd::shared_ptr<trace_api::Tracer> result(new TracerWithSpanContext());
    return result;
  }
#else
  nostd::shared_ptr<trace_api::Tracer> GetTracer(
      nostd::string_view /* name */,
      nostd::string_view /* version */,
      nostd::string_view /* schema_url */) noexcept override
  {
    nostd::shared_ptr<trace_api::Tracer> result(new TracerWithSpanContext());
    return result;
  }
#endif
};

TEST_F(TracerShimTest, InjectSpanKey)
{
  using context::propagation::TextMapPropagator;

  auto local_text_map_format     = new MockPropagator();
  auto local_http_headers_format = new MockPropagator();

  ASSERT_FALSE(local_text_map_format->is_injected);
  ASSERT_FALSE(local_http_headers_format->is_injected);

  nostd::shared_ptr<trace_api::TracerProvider> tracer_provider =
      TracerWithSpanContextProvider::Create();
  auto local_tracer_shim = shim::TracerShim::createTracerShim(
      tracer_provider,
      {.text_map     = nostd::shared_ptr<TextMapPropagator>(local_text_map_format),
       .http_headers = nostd::shared_ptr<TextMapPropagator>(local_http_headers_format)});

  std::unordered_map<std::string, std::string> text_map;
  auto span_shim = local_tracer_shim->StartSpan("a");
  local_tracer_shim->Inject(span_shim->context(), TextMapCarrier{text_map});

  ASSERT_TRUE(local_text_map_format->is_injected);
  ASSERT_FALSE(local_http_headers_format->is_injected);

  ASSERT_EQ(span_shim->context().ToTraceID(), text_map[MockPropagator::kTraceIdKey]);
  ASSERT_EQ(span_shim->context().ToSpanID(), text_map[MockPropagator::kSpanIdKey]);

  char flag_buffer[2];
  TracerWithSpanContext::GetTraceFlags().ToLowerBase16(flag_buffer);
  ASSERT_EQ(std::string(flag_buffer, 2), text_map[MockPropagator::kTraceFlagsKey]);
}
