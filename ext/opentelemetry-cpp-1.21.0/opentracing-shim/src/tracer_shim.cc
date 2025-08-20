/*
 * Copyright The OpenTelemetry Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <iosfwd>
#include <new>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "opentracing/expected/expected.hpp"
#include "opentracing/ext/tags.h"
#include "opentracing/propagation.h"
#include "opentracing/span.h"
#include "opentracing/string_view.h"
#include "opentracing/tracer.h"
#include "opentracing/value.h"

#include "opentelemetry/baggage/baggage_context.h"
#include "opentelemetry/context/propagation/global_propagator.h"
#include "opentelemetry/context/propagation/text_map_propagator.h"
#include "opentelemetry/context/runtime_context.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/opentracingshim/propagation.h"
#include "opentelemetry/opentracingshim/shim_utils.h"
#include "opentelemetry/opentracingshim/span_context_shim.h"
#include "opentelemetry/opentracingshim/span_shim.h"
#include "opentelemetry/opentracingshim/tracer_shim.h"
#include "opentelemetry/trace/context.h"
#include "opentelemetry/trace/default_span.h"
#include "opentelemetry/trace/span.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/tracer.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace opentracingshim
{

std::unique_ptr<opentracing::Span> TracerShim::StartSpanWithOptions(
    opentracing::string_view operation_name,
    const opentracing::StartSpanOptions &options) const noexcept
{
  if (is_closed_)
    return nullptr;

  const auto &opts         = utils::makeOptionsShim(options);
  const auto &links        = utils::makeIterableLinks(options);
  const auto &attributes   = utils::makeIterableTags(options);
  const auto &baggage      = utils::makeBaggage(options);
  auto operation_name_view = nostd::string_view{operation_name.data(), operation_name.size()};
  auto span                = tracer_->StartSpan(operation_name_view, attributes, links, opts);
  auto span_shim           = new (std::nothrow) SpanShim(*this, span, baggage);

  // If an initial set of tags is specified and the OpenTracing error tag
  // is included after the OpenTelemetry Span was created.
  const auto &error_entry =
      std::find_if(options.tags.cbegin(), options.tags.cend(),
                   [](const std::pair<std::string, opentracing::v3::Value> &entry) {
                     return entry.first == opentracing::ext::error;
                   });
  // The Shim layer MUST perform the same error handling as described in the Set Tag operation
  if (span_shim && error_entry != options.tags.cend())
  {
    span_shim->handleError(error_entry->second);
  }

  return std::unique_ptr<opentracing::Span>(span_shim);
}

opentracing::expected<void> TracerShim::Inject(const opentracing::SpanContext &,
                                               std::ostream &) const
{
  // Errors MAY be raised if the specified Format is not recognized,
  // depending on the specific OpenTracing Language API.
  return opentracing::make_unexpected(opentracing::invalid_carrier_error);
}

opentracing::expected<void> TracerShim::Inject(const opentracing::SpanContext &sc,
                                               const opentracing::TextMapWriter &writer) const
{
  // TextMap and HttpHeaders formats MUST use their explicitly specified TextMapPropagator,
  // if any, or else use the global TextMapPropagator.
  const auto &propagator =
      propagators_.text_map
          ? propagators_.text_map
          : opentelemetry::context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();

  return injectImpl(sc, writer, propagator);
}

opentracing::expected<void> TracerShim::Inject(const opentracing::SpanContext &sc,
                                               const opentracing::HTTPHeadersWriter &writer) const
{
  // TextMap and HttpHeaders formats MUST use their explicitly specified TextMapPropagator,
  // if any, or else use the global TextMapPropagator.
  const auto &propagator =
      propagators_.http_headers
          ? propagators_.http_headers
          : opentelemetry::context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();

  return injectImpl(sc, writer, propagator);
}

opentracing::expected<std::unique_ptr<opentracing::SpanContext>> TracerShim::Extract(
    std::istream &) const
{
  // Errors MAY be raised if either the Format is not recognized or no value
  // could be extracted, depending on the specific OpenTracing Language API.
  return opentracing::make_unexpected(opentracing::invalid_carrier_error);
}

opentracing::expected<std::unique_ptr<opentracing::SpanContext>> TracerShim::Extract(
    const opentracing::TextMapReader &reader) const
{
  // TextMap and HttpHeaders formats MUST use their explicitly specified TextMapPropagator,
  // if any, or else use the global TextMapPropagator.
  const auto &propagator =
      propagators_.text_map
          ? propagators_.text_map
          : opentelemetry::context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();

  return extractImpl(reader, propagator);
}

opentracing::expected<std::unique_ptr<opentracing::SpanContext>> TracerShim::Extract(
    const opentracing::HTTPHeadersReader &reader) const
{
  // TextMap and HttpHeaders formats MUST use their explicitly specified TextMapPropagator,
  // if any, or else use the global TextMapPropagator.
  const auto &propagator =
      propagators_.http_headers
          ? propagators_.http_headers
          : opentelemetry::context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();

  return extractImpl(reader, propagator);
}

opentracing::expected<void> TracerShim::injectImpl(const opentracing::SpanContext &sc,
                                                   const opentracing::TextMapWriter &writer,
                                                   const PropagatorPtr &propagator) const
{
  // Inject the underlying OpenTelemetry Span and Baggage using either the explicitly registered
  // or the global OpenTelemetry Propagators, as configured at construction time.
  if (auto context_shim = SpanContextShim::extractFrom(&sc))
  {
    auto current_context = opentelemetry::context::RuntimeContext::GetCurrent();

    // Inject dummy span to provide SpanContext information
    auto span_context = opentelemetry::trace::SpanContext(
        context_shim->context().trace_id(), context_shim->context().span_id(),
        context_shim->context().trace_flags(), false);
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> sp{
        new opentelemetry::trace::DefaultSpan(span_context)};
    auto context_with_span = opentelemetry::trace::SetSpan(current_context, sp);

    // Inject any non-empty Baggage
    const auto &context_with_span_baggage =
        opentelemetry::baggage::SetBaggage(context_with_span, context_shim->baggage());

    CarrierWriterShim carrier{writer};
    propagator->Inject(carrier, context_with_span_baggage);
    return opentracing::make_expected();
  }

  return opentracing::make_unexpected(opentracing::invalid_span_context_error);
}

opentracing::expected<std::unique_ptr<opentracing::SpanContext>> TracerShim::extractImpl(
    const opentracing::TextMapReader &reader,
    const PropagatorPtr &propagator) const
{
  // Extract the underlying OpenTelemetry Span and Baggage using either the explicitly registered
  // or the global OpenTelemetry Propagators, as configured at construction time.
  CarrierReaderShim carrier{reader};
  auto current_context = opentelemetry::context::RuntimeContext::GetCurrent();
  auto context         = propagator->Extract(carrier, current_context);
  auto span_context    = opentelemetry::trace::GetSpan(context)->GetContext();
  auto baggage         = opentelemetry::baggage::GetBaggage(context);

  // The operation MUST return a `SpanContext` Shim instance with the extracted values if any of
  // these conditions are met:
  // * `SpanContext` is valid.
  // * `SpanContext` is sampled.
  // * `SpanContext` contains non-empty extracted `Baggage`.
  // Otherwise, the operation MUST return null or empty value.
  SpanContextShim *context_shim =
      (!span_context.IsValid() && !span_context.IsSampled() && utils::isBaggageEmpty(baggage))
          ? nullptr
          : new (std::nothrow) SpanContextShim(span_context, baggage);

  // Note: Invalid but sampled `SpanContext` instances are returned as a way to support
  // jaeger-debug-id headers (https://github.com/jaegertracing/jaeger-client-java#via-http-headers)
  // which are used to force propagation of debug information.
  return opentracing::make_expected(std::unique_ptr<opentracing::SpanContext>(context_shim));
}

}  // namespace opentracingshim
OPENTELEMETRY_END_NAMESPACE
