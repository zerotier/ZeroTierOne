// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <iostream>
#include <new>
#include <string>
#include <utility>

#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/context/context_value.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/span_metadata.h"
#include "tracer.h"

namespace nostd   = opentelemetry::nostd;
namespace common  = opentelemetry::common;
namespace trace   = opentelemetry::trace;
namespace context = opentelemetry::context;

namespace
{
class Span final : public trace::Span
{
public:
  Span(std::shared_ptr<Tracer> &&tracer,
       nostd::string_view name,
       const common::KeyValueIterable & /*attributes*/,
       const trace::SpanContextKeyValueIterable & /*links*/,
       const trace::StartSpanOptions & /*options*/) noexcept
      : tracer_{std::move(tracer)}, name_{name}, span_context_{trace::SpanContext::GetInvalid()}
  {
    std::cout << "StartSpan: " << name << "\n";
  }

  ~Span() override { std::cout << "~Span\n"; }

  // opentelemetry::trace::Span
  void SetAttribute(nostd::string_view /*name*/,
                    const common::AttributeValue & /*value*/) noexcept override
  {}

  void AddEvent(nostd::string_view /*name*/) noexcept override {}

  void AddEvent(nostd::string_view /*name*/,
                common::SystemTimestamp /*timestamp*/) noexcept override
  {}

  void AddEvent(nostd::string_view /*name*/,
                common::SystemTimestamp /*timestamp*/,
                const common::KeyValueIterable & /*attributes*/) noexcept override
  {}

  void AddEvent(nostd::string_view /*name*/,
                const common::KeyValueIterable & /*attributes*/) noexcept override
  {}

#if OPENTELEMETRY_ABI_VERSION_NO >= 2
  void AddLink(const trace::SpanContext & /* target */,
               const common::KeyValueIterable & /* attrs */) noexcept override
  {}

  void AddLinks(const trace::SpanContextKeyValueIterable & /* links */) noexcept override {}
#endif

  void SetStatus(trace::StatusCode /*code*/, nostd::string_view /*description*/) noexcept override
  {}

  void UpdateName(nostd::string_view /*name*/) noexcept override {}

  void End(const trace::EndSpanOptions & /*options*/) noexcept override {}

  bool IsRecording() const noexcept override { return true; }

  trace::SpanContext GetContext() const noexcept override { return span_context_; }

private:
  std::shared_ptr<Tracer> tracer_;
  std::string name_;
  trace::SpanContext span_context_;
};
}  // namespace

Tracer::Tracer(nostd::string_view /*output*/)
{
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
  UpdateEnabled(true);
#endif
}

nostd::shared_ptr<trace::Span> Tracer::StartSpan(nostd::string_view name,
                                                 const common::KeyValueIterable &attributes,
                                                 const trace::SpanContextKeyValueIterable &links,
                                                 const trace::StartSpanOptions &options) noexcept
{
  return nostd::shared_ptr<trace::Span>{
      new (std::nothrow) Span{this->shared_from_this(), name, attributes, links, options}};
}
