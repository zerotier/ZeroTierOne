// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "opentelemetry/common/key_value_iterable_view.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/sdk/trace/sampler.h"
#include "opentelemetry/sdk/trace/samplers/always_off.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/span_context_kv_iterable_view.h"
#include "opentelemetry/trace/span_metadata.h"
#include "opentelemetry/trace/trace_id.h"
#include "opentelemetry/trace/trace_state.h"

using opentelemetry::sdk::trace::AlwaysOffSampler;
using opentelemetry::sdk::trace::Decision;
using opentelemetry::trace::SpanContext;
namespace trace_api = opentelemetry::trace;

TEST(AlwaysOffSampler, ShouldSample)
{
  AlwaysOffSampler sampler;

  trace_api::TraceId trace_id;
  trace_api::SpanKind span_kind = trace_api::SpanKind::kInternal;

  using M = std::map<std::string, int>;
  M m1    = {{}};

  using L = std::vector<std::pair<SpanContext, std::map<std::string, std::string>>>;
  L l1    = {{SpanContext(false, false), {}}, {SpanContext(false, false), {}}};

  opentelemetry::common::KeyValueIterableView<M> view{m1};
  trace_api::SpanContextKeyValueIterableView<L> links{l1};

  auto sampling_result =
      sampler.ShouldSample(SpanContext::GetInvalid(), trace_id, "", span_kind, view, links);

  ASSERT_EQ(Decision::DROP, sampling_result.decision);
  ASSERT_EQ(nullptr, sampling_result.attributes);
  ASSERT_EQ("", sampling_result.trace_state->ToHeader());
}

TEST(AlwaysOffSampler, GetDescription)
{
  AlwaysOffSampler sampler;

  ASSERT_EQ("AlwaysOffSampler", sampler.GetDescription());
}
