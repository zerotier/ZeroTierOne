// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/logs/logger.h"
#include "opentelemetry/logs/logger_provider.h"
#include "opentelemetry/logs/provider.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/trace/scope.h"
#include "opentelemetry/trace/span.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/tracer.h"
#include "opentelemetry/trace/tracer_provider.h"

namespace logs  = opentelemetry::logs;
namespace trace = opentelemetry::trace;

namespace
{
opentelemetry::nostd::shared_ptr<trace::Tracer> get_tracer()
{
  auto provider = trace::Provider::GetTracerProvider();
  return provider->GetTracer("foo_library");
}

opentelemetry::nostd::shared_ptr<logs::Logger> get_logger()
{
  auto provider = logs::Provider::GetLoggerProvider();
  return provider->GetLogger("foo_library_logger", "foo_library");
}
}  // namespace

void foo_library()
{
  auto span        = get_tracer()->StartSpan("span 1");
  auto scoped_span = trace::Scope(get_tracer()->StartSpan("foo_library"));
  auto ctx         = span->GetContext();
  auto logger      = get_logger();

  logger->Debug("body", ctx.trace_id(), ctx.span_id(), ctx.trace_flags());
}
