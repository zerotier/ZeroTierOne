/*
 * Copyright The OpenTelemetry Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <functional>
#include <string>

#include "opentracing/span.h"

#include "opentelemetry/baggage/baggage.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace opentracingshim
{

using BaggagePtr = nostd::shared_ptr<opentelemetry::baggage::Baggage>;

class SpanContextShim final : public opentracing::SpanContext
{
public:
  explicit SpanContextShim(const opentelemetry::trace::SpanContext &context,
                           const BaggagePtr &baggage)
      : context_(context), baggage_(baggage)
  {}

  inline const opentelemetry::trace::SpanContext &context() const { return context_; }
  inline const BaggagePtr &baggage() const { return baggage_; }
  SpanContextShim newWithKeyValue(nostd::string_view key, nostd::string_view value) const noexcept;
  bool BaggageItem(nostd::string_view key, std::string &value) const noexcept;

  using VisitBaggageItem = std::function<bool(const std::string &key, const std::string &value)>;
  void ForeachBaggageItem(VisitBaggageItem f) const override;
  std::unique_ptr<opentracing::SpanContext> Clone() const noexcept override;
  std::string ToTraceID() const noexcept override;
  std::string ToSpanID() const noexcept override;

  static inline const SpanContextShim *extractFrom(const opentracing::SpanContext *span_context)
  {
#ifndef OPENTELEMETRY_RTTI_ENABLED
    auto result = static_cast<const SpanContextShim *>(span_context);
    return result && result->provides_context_and_baggage_ == kUniqueTag ? result : nullptr;
#else
    return dynamic_cast<const SpanContextShim *>(span_context);
#endif
  }

private:
  template <typename T>
  static std::string toHexString(const T &id_item)
  {
    char buf[T::kSize * 2];
    id_item.ToLowerBase16(buf);
    return std::string(buf, sizeof(buf));
  }

  opentelemetry::trace::SpanContext context_;
  BaggagePtr baggage_;

#ifndef OPENTELEMETRY_RTTI_ENABLED
  static constexpr uint64_t kUniqueTag   = 0x07e1ca578e57a71c;
  uint64_t provides_context_and_baggage_ = kUniqueTag;
#endif
};

}  // namespace opentracingshim
OPENTELEMETRY_END_NAMESPACE
