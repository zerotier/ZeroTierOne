/*
 * Copyright The OpenTelemetry Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <functional>
#include <memory>
#include <new>
#include <string>

#include "opentracing/span.h"

#include "opentelemetry/baggage/baggage.h"
#include "opentelemetry/nostd/function_ref.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/opentracingshim/span_context_shim.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/span_id.h"
#include "opentelemetry/trace/trace_id.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace opentracingshim
{

SpanContextShim SpanContextShim::newWithKeyValue(nostd::string_view key,
                                                 nostd::string_view value) const noexcept
{
  return SpanContextShim{context_, baggage_->Set(key, value)};
}

bool SpanContextShim::BaggageItem(nostd::string_view key, std::string &value) const noexcept
{
  return baggage_->GetValue(key, value);
}

void SpanContextShim::ForeachBaggageItem(VisitBaggageItem f) const
{
  baggage_->GetAllEntries([&f](nostd::string_view key, nostd::string_view value) {
    return f(std::string{key.data(), key.size()}, std::string{value.data(), value.size()});
  });
}

std::unique_ptr<opentracing::SpanContext> SpanContextShim::Clone() const noexcept
{
  return std::unique_ptr<SpanContextShim>(new (std::nothrow) SpanContextShim(context_, baggage_));
}

std::string SpanContextShim::ToTraceID() const noexcept
{
  return toHexString(context_.trace_id());
}

std::string SpanContextShim::ToSpanID() const noexcept
{
  return toHexString(context_.span_id());
}

}  // namespace opentracingshim
OPENTELEMETRY_END_NAMESPACE
