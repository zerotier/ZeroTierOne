// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0
#include "opentelemetry/sdk/trace/sampler.h"
#include "opentelemetry/trace/span.h"

#pragma once
OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace etw
{

class TailSampler
{
public:
  // convert to etw span if required for getters on span.
  // auto etw_span = static_cast<const opentelemetry::exporter::etw::Span*>(&span);
  // Decision based on
  //      Span::GetStatus()
  //      Span::GetProperties()
  //      Span::GetContext()
  virtual opentelemetry::sdk::trace::SamplingResult ShouldSample(
      const opentelemetry::trace::Span &span) noexcept = 0;
  virtual ~TailSampler()                               = default;
};

class AlwaysOnTailSampler : public TailSampler
{
public:
  opentelemetry::sdk::trace::SamplingResult ShouldSample(
      const opentelemetry::trace::Span &span) noexcept override
  {
    UNREFERENCED_PARAMETER(span);
    return {opentelemetry::sdk::trace::Decision::RECORD_AND_SAMPLE};
  }
};

}  // namespace etw
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
