/*
 * Copyright The OpenTelemetry Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "opentracing/span.h"
#include "opentracing/string_view.h"
#include "opentracing/tracer.h"
#include "opentracing/util.h"
#include "opentracing/value.h"

#include "opentelemetry/common/spin_lock_mutex.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/opentracingshim/span_context_shim.h"
#include "opentelemetry/opentracingshim/tracer_shim.h"
#include "opentelemetry/trace/span.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace opentracingshim
{

using SpanPtr    = nostd::shared_ptr<opentelemetry::trace::Span>;
using EventEntry = std::pair<opentracing::string_view, opentracing::Value>;

class SpanShim : public opentracing::Span
{
public:
  explicit SpanShim(const TracerShim &tracer, const SpanPtr &span, const BaggagePtr &baggage)
      : tracer_(tracer), span_(span), context_(span->GetContext(), baggage)
  {}

  void handleError(const opentracing::Value &value) noexcept;

  void FinishWithOptions(
      const opentracing::FinishSpanOptions &finish_span_options) noexcept override;
  void SetOperationName(opentracing::string_view name) noexcept override;
  void SetTag(opentracing::string_view key, const opentracing::Value &value) noexcept override;
  void SetBaggageItem(opentracing::string_view restricted_key,
                      opentracing::string_view value) noexcept override;
  std::string BaggageItem(opentracing::string_view restricted_key) const noexcept override;
  void Log(std::initializer_list<EventEntry> fields) noexcept override;
  void Log(opentracing::SystemTime timestamp,
           std::initializer_list<EventEntry> fields) noexcept override;
  void Log(opentracing::SystemTime timestamp,
           const std::vector<EventEntry> &fields) noexcept override;
  inline const opentracing::SpanContext &context() const noexcept override { return context_; }
  inline const opentracing::Tracer &tracer() const noexcept override { return tracer_; }

private:
  void logImpl(nostd::span<const EventEntry> fields,
               const opentracing::SystemTime *const timestamp) noexcept;

  const TracerShim &tracer_;
  SpanPtr span_;
  SpanContextShim context_;
  mutable opentelemetry::common::SpinLockMutex context_lock_;
};

}  // namespace opentracingshim
OPENTELEMETRY_END_NAMESPACE
