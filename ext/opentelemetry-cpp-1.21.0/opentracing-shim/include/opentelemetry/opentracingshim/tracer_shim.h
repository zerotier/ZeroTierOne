/*
 * Copyright The OpenTelemetry Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <iosfwd>
#include <new>

#include "opentracing/expected/expected.hpp"
#include "opentracing/propagation.h"
#include "opentracing/span.h"
#include "opentracing/string_view.h"
#include "opentracing/tracer.h"

#include "opentelemetry/context/propagation/text_map_propagator.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/trace/tracer.h"
#include "opentelemetry/trace/tracer_provider.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace opentracingshim
{

using TracerPtr         = nostd::shared_ptr<opentelemetry::trace::Tracer>;
using TracerProviderPtr = nostd::shared_ptr<opentelemetry::trace::TracerProvider>;
using PropagatorPtr     = nostd::shared_ptr<opentelemetry::context::propagation::TextMapPropagator>;

struct OpenTracingPropagators
{
  PropagatorPtr text_map;
  PropagatorPtr http_headers;
};

class TracerShim : public opentracing::Tracer
{
public:
  static inline std::shared_ptr<opentracing::Tracer> createTracerShim(
      const TracerProviderPtr &provider = opentelemetry::trace::Provider::GetTracerProvider(),
      const OpenTracingPropagators &propagators = {}) noexcept
  {
    // This operation MUST accept the following parameters:
    // - An OpenTelemetry TracerProvider. This operation MUST use this TracerProvider to obtain a
    //   Tracer with the name opentracing-shim along with the current shim library version.
    // - OpenTelemetry Propagators to be used to perform injection and extraction for the
    //   OpenTracing TextMap and HTTPHeaders formats. If not specified, no Propagator values will
    //   be stored in the Shim, and the global OpenTelemetry TextMap propagator will be used for
    //   both OpenTracing TextMap and HTTPHeaders formats.
    return std::shared_ptr<opentracing::Tracer>(
        new (std::nothrow) TracerShim(provider->GetTracer("opentracing-shim"), propagators));
  }

  std::unique_ptr<opentracing::Span> StartSpanWithOptions(
      opentracing::string_view operation_name,
      const opentracing::StartSpanOptions &options) const noexcept override;
  opentracing::expected<void> Inject(const opentracing::SpanContext &sc,
                                     std::ostream &writer) const override;
  opentracing::expected<void> Inject(const opentracing::SpanContext &sc,
                                     const opentracing::TextMapWriter &writer) const override;
  opentracing::expected<void> Inject(const opentracing::SpanContext &sc,
                                     const opentracing::HTTPHeadersWriter &writer) const override;
  opentracing::expected<std::unique_ptr<opentracing::SpanContext>> Extract(
      std::istream &reader) const override;
  opentracing::expected<std::unique_ptr<opentracing::SpanContext>> Extract(
      const opentracing::TextMapReader &reader) const override;
  opentracing::expected<std::unique_ptr<opentracing::SpanContext>> Extract(
      const opentracing::HTTPHeadersReader &reader) const override;
  inline void Close() noexcept override { is_closed_ = true; }

private:
  explicit TracerShim(const TracerPtr &tracer, const OpenTracingPropagators &propagators)
      : tracer_(tracer), propagators_(propagators)
  {}

  opentracing::expected<void> injectImpl(const opentracing::SpanContext &sc,
                                         const opentracing::TextMapWriter &writer,
                                         const PropagatorPtr &propagator) const;
  opentracing::expected<std::unique_ptr<opentracing::SpanContext>> extractImpl(
      const opentracing::TextMapReader &reader,
      const PropagatorPtr &propagator) const;

  TracerPtr tracer_;
  OpenTracingPropagators propagators_;
  bool is_closed_ = false;
};

}  // namespace opentracingshim
OPENTELEMETRY_END_NAMESPACE
