// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <string>

#include "opentelemetry/context/context_value.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/trace/noop.h"
#include "opentelemetry/trace/span.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/tracer.h"

namespace trace_api = opentelemetry::trace;
namespace nostd     = opentelemetry::nostd;
namespace context   = opentelemetry::context;

TEST(TracerTest, GetCurrentSpan)
{
  std::unique_ptr<trace_api::Tracer> tracer(new trace_api::NoopTracer());
  nostd::shared_ptr<trace_api::Span> span_first(new trace_api::NoopSpan(nullptr));
  nostd::shared_ptr<trace_api::Span> span_second(new trace_api::NoopSpan(nullptr));

  auto current = tracer->GetCurrentSpan();
  ASSERT_FALSE(current->GetContext().IsValid());

  {
    auto scope_first = tracer->WithActiveSpan(span_first);
    current          = tracer->GetCurrentSpan();
    ASSERT_EQ(current, span_first);

    {
      auto scope_second = tracer->WithActiveSpan(span_second);
      current           = tracer->GetCurrentSpan();
      ASSERT_EQ(current, span_second);
    }
    current = tracer->GetCurrentSpan();
    ASSERT_EQ(current, span_first);
  }

  current = tracer->GetCurrentSpan();
  ASSERT_FALSE(current->GetContext().IsValid());
}
