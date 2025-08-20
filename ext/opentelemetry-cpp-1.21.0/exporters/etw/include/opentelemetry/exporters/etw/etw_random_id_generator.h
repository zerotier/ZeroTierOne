// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "opentelemetry/sdk/trace/id_generator.h"
#include "opentelemetry/version.h"

#ifdef _WIN32
#  include "Windows.h"
#endif

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace trace
{

class ETWRandomIdGenerator : public IdGenerator
{
public:
  ETWRandomIdGenerator() : IdGenerator(false) {}

  opentelemetry::trace::SpanId GenerateSpanId() noexcept override
  {
    GUID span_id;
    // Generate random GUID
    CoCreateGuid(&span_id);
    const auto *spanIdPtr = reinterpret_cast<const uint8_t *>(std::addressof(span_id));

    // Populate SpanId with that GUID
    nostd::span<const uint8_t, opentelemetry::trace::SpanId::kSize> spanIdBytes(
        spanIdPtr, spanIdPtr + opentelemetry::trace::SpanId::kSize);
    return opentelemetry::trace::SpanId(spanIdBytes);
  }

  opentelemetry::trace::TraceId GenerateTraceId() noexcept override
  {
    GUID trace_id;
    CoCreateGuid(&trace_id);
    // Populate TraceId of the Tracer with the above GUID
    const auto *traceIdPtr = reinterpret_cast<const uint8_t *>(std::addressof(trace_id));
    nostd::span<const uint8_t, opentelemetry::trace::TraceId::kSize> traceIdBytes(
        traceIdPtr, traceIdPtr + opentelemetry::trace::TraceId::kSize);
    return opentelemetry::trace::TraceId(traceIdBytes);
  }
};
}  // namespace trace
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
