// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <stdint.h>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "opentelemetry/common/key_value_iterable_view.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/sdk/trace/sampler.h"
#include "opentelemetry/sdk/trace/samplers/always_on.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/span_context_kv_iterable_view.h"
#include "opentelemetry/trace/span_metadata.h"
#include "opentelemetry/trace/trace_id.h"
#include "opentelemetry/trace/trace_state.h"

using namespace opentelemetry::sdk::trace;
using namespace opentelemetry::nostd;
using opentelemetry::trace::SpanContext;
namespace trace_api = opentelemetry::trace;

TEST(AlwaysOnSampler, ShouldSample)
{
  AlwaysOnSampler sampler;

  // A buffer of trace_id with no specific meaning
  constexpr uint8_t buf[] = {0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7};

  trace_api::TraceId trace_id_invalid;
  trace_api::TraceId trace_id_valid(buf);
  std::map<std::string, int> key_value_container = {{"key", 0}};

  using L = std::vector<std::pair<trace_api::SpanContext, std::map<std::string, std::string>>>;
  L l1 = {{trace_api::SpanContext(false, false), {}}, {trace_api::SpanContext(false, false), {}}};

  opentelemetry::trace::SpanContextKeyValueIterableView<L> links{l1};

  // Test with invalid (empty) trace id and empty parent context
  auto sampling_result = sampler.ShouldSample(
      SpanContext::GetInvalid(), trace_id_invalid, "invalid trace id test",
      trace_api::SpanKind::kServer,
      opentelemetry::common::KeyValueIterableView<std::map<std::string, int>>(key_value_container),
      links);

  ASSERT_EQ(Decision::RECORD_AND_SAMPLE, sampling_result.decision);
  ASSERT_EQ(nullptr, sampling_result.attributes);
  ASSERT_EQ("", sampling_result.trace_state->ToHeader());

  // Test with a valid trace id and empty parent context
  sampling_result = sampler.ShouldSample(
      SpanContext::GetInvalid(), trace_id_valid, "valid trace id test",
      trace_api::SpanKind::kServer,
      opentelemetry::common::KeyValueIterableView<std::map<std::string, int>>(key_value_container),
      links);

  ASSERT_EQ(Decision::RECORD_AND_SAMPLE, sampling_result.decision);
  ASSERT_EQ(nullptr, sampling_result.attributes);
  ASSERT_EQ("", sampling_result.trace_state->ToHeader());
}

TEST(AlwaysOnSampler, GetDescription)
{
  AlwaysOnSampler sampler;

  ASSERT_EQ("AlwaysOnSampler", sampler.GetDescription());
}
