/*
 * Copyright The OpenTelemetry Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <tuple>
#include <unordered_map>

#include "opentracing/propagation.h"

#include "opentelemetry/baggage/baggage_context.h"
#include "opentelemetry/context/propagation/text_map_propagator.h"
#include "opentelemetry/trace/span.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/span_metadata.h"
#include "opentelemetry/trace/tracer.h"
#include "opentelemetry/trace/tracer_provider.h"

namespace trace_api = opentelemetry::trace;
namespace baggage   = opentelemetry::baggage;
namespace common    = opentelemetry::common;
namespace context   = opentelemetry::context;
namespace nostd     = opentelemetry::nostd;

struct MockSpan final : public trace_api::Span
{
  void SetAttribute(nostd::string_view key, const common::AttributeValue &value) noexcept override
  {
    attribute_ = {key.data(), value};
  }

  void AddEvent(nostd::string_view name,
                common::SystemTimestamp timestamp,
                const common::KeyValueIterable &attributes) noexcept override
  {
    std::unordered_map<std::string, common::AttributeValue> attribute_map;
    attribute_map.reserve(attributes.size());
    attributes.ForEachKeyValue(
        [&attribute_map](nostd::string_view key, const common::AttributeValue &value) {
          attribute_map.emplace(key.data(), value);
          return true;
        });
    event_ = {name.data(), timestamp, attribute_map};
  }

  void AddEvent(nostd::string_view name,
                const common::KeyValueIterable &attributes) noexcept override
  {
    AddEvent(name, {}, attributes);
  }

  void AddEvent(nostd::string_view, common::SystemTimestamp) noexcept override {}

  void AddEvent(nostd::string_view) noexcept override {}

  void SetStatus(trace_api::StatusCode code, nostd::string_view description) noexcept override
  {
    status_ = {code, description.data()};
  }

  void UpdateName(nostd::string_view name) noexcept override { name_ = name.data(); }

  void End(const trace_api::EndSpanOptions &options) noexcept override { options_ = options; }

  bool IsRecording() const noexcept override { return false; }

  trace_api::SpanContext GetContext() const noexcept override
  {
    return trace_api::SpanContext(false, false);
  }

  std::pair<std::string, common::AttributeValue> attribute_;
  std::tuple<std::string,
             common::SystemTimestamp,
             std::unordered_map<std::string, common::AttributeValue>>
      event_;
  std::pair<trace_api::StatusCode, std::string> status_;
  std::string name_;
  trace_api::EndSpanOptions options_;
};

struct MockTracer final : public trace_api::Tracer
{
  nostd::shared_ptr<trace_api::Span> StartSpan(
      nostd::string_view name,
      const common::KeyValueIterable &,
      const trace_api::SpanContextKeyValueIterable &,
      const trace_api::StartSpanOptions &) noexcept override
  {
    span_        = new MockSpan();
    span_->name_ = std::string{name};
    return nostd::shared_ptr<trace_api::Span>(span_);
  }

  void ForceFlushWithMicroseconds(uint64_t) noexcept override {}

  void CloseWithMicroseconds(uint64_t) noexcept override {}

  MockSpan *span_;
};

struct MockTracerProvider final : public trace_api::TracerProvider
{
  nostd::shared_ptr<trace_api::Tracer> GetTracer(nostd::string_view library_name,
                                                 nostd::string_view,
                                                 nostd::string_view) noexcept override
  {
    library_name_ = std::string{library_name};
    tracer_       = new MockTracer();
    return nostd::shared_ptr<trace_api::Tracer>(tracer_);
  }

  std::string library_name_;
  MockTracer *tracer_;
};

struct MockPropagator : public context::propagation::TextMapPropagator
{
  static constexpr const char *kTraceIdKey    = "trace_id";
  static constexpr const char *kSpanIdKey     = "span_id";
  static constexpr const char *kTraceFlagsKey = "trace_flags_id";

  template <class T, int N>
  static inline std::string ToLowerBase16(const T &id)
  {
    char buf[N] = {0};
    id.ToLowerBase16(buf);
    return std::string(buf, sizeof(buf));
  }

  // Returns the context that is stored in the carrier with the TextMapCarrier as extractor.
  context::Context Extract(const context::propagation::TextMapCarrier &carrier,
                           context::Context &context) noexcept override
  {
    std::vector<std::pair<std::string, std::string>> kvs;
    carrier.Keys([&carrier, &kvs](nostd::string_view k) {
      kvs.emplace_back(k, carrier.Get(k));
      return true;
    });
    is_extracted = true;
    return baggage::SetBaggage(context,
                               nostd::shared_ptr<baggage::Baggage>(new baggage::Baggage(kvs)));
  }

  // Sets the context for carrier with self defined rules.
  void Inject(context::propagation::TextMapCarrier &carrier,
              const context::Context &context) noexcept override
  {
    auto baggage = baggage::GetBaggage(context);
    baggage->GetAllEntries([&carrier](nostd::string_view k, nostd::string_view v) {
      carrier.Set(k, v);
      return true;
    });

    auto span_key_value = context.GetValue(trace_api::kSpanKey);
    if (nostd::holds_alternative<nostd::shared_ptr<trace_api::Span>>(span_key_value))
    {
      auto span = nostd::get<nostd::shared_ptr<trace_api::Span>>(span_key_value);
      if (span)
      {
        // Store span context information in TextMapCarrier to allow verifying propagation
        auto span_context = span->GetContext();
        carrier.Set(kTraceIdKey, ToLowerBase16<trace_api::TraceId, 2 * trace_api::TraceId::kSize>(
                                     span_context.trace_id()));
        carrier.Set(kSpanIdKey, ToLowerBase16<trace_api::SpanId, 2 * trace_api::SpanId::kSize>(
                                    span_context.span_id()));
        carrier.Set(kTraceFlagsKey,
                    ToLowerBase16<trace_api::TraceFlags, 2>(span_context.trace_flags()));
      }
    }

    is_injected = true;
  }

  // Gets the fields set in the carrier by the `inject` method
  bool Fields(nostd::function_ref<bool(nostd::string_view)>) const noexcept override
  {
    return true;
  }

  bool is_extracted = false;
  bool is_injected  = false;
};

struct TextMapCarrier : opentracing::TextMapReader, opentracing::TextMapWriter
{
  TextMapCarrier(std::unordered_map<std::string, std::string> &text_map_) : text_map(text_map_) {}

  opentracing::expected<void> Set(opentracing::string_view key,
                                  opentracing::string_view value) const override
  {
    text_map[key] = value;
    return {};
  }

  opentracing::expected<opentracing::string_view> LookupKey(
      opentracing::string_view key) const override
  {
    if (!supports_lookup)
    {
      return opentracing::make_unexpected(opentracing::lookup_key_not_supported_error);
    }
    auto iter = text_map.find(key);
    if (iter != text_map.end())
    {
      return opentracing::string_view{iter->second};
    }
    else
    {
      return opentracing::make_unexpected(opentracing::key_not_found_error);
    }
  }

  opentracing::expected<void> ForeachKey(
      std::function<opentracing::expected<void>(opentracing::string_view key,
                                                opentracing::string_view value)> f) const override
  {
    ++foreach_key_call_count;
    for (const auto &key_value : text_map)
    {
      auto result = f(key_value.first, key_value.second);
      if (!result)
        return result;
    }
    return {};
  }

  bool supports_lookup               = false;
  mutable int foreach_key_call_count = 0;
  std::unordered_map<std::string, std::string> &text_map;
};

struct HTTPHeadersCarrier : opentracing::HTTPHeadersReader, opentracing::HTTPHeadersWriter
{
  HTTPHeadersCarrier(std::unordered_map<std::string, std::string> &text_map_) : text_map(text_map_)
  {}

  opentracing::expected<void> Set(opentracing::string_view key,
                                  opentracing::string_view value) const override
  {
    text_map[key] = value;
    return {};
  }

  opentracing::expected<void> ForeachKey(
      std::function<opentracing::expected<void>(opentracing::string_view key,
                                                opentracing::string_view value)> f) const override
  {
    for (const auto &key_value : text_map)
    {
      auto result = f(key_value.first, key_value.second);
      if (!result)
        return result;
    }
    return {};
  }

  std::unordered_map<std::string, std::string> &text_map;
};
