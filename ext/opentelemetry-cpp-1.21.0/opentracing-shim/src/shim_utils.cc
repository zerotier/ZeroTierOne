/*
 * Copyright The OpenTelemetry Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "opentracing/propagation.h"
#include "opentracing/span.h"
#include "opentracing/tracer.h"

#include "opentelemetry/baggage/baggage.h"
#include "opentelemetry/baggage/baggage_context.h"
#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/context/runtime_context.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/opentracingshim/shim_utils.h"
#include "opentelemetry/opentracingshim/span_context_shim.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/span_startoptions.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace opentracingshim
{
namespace utils
{

opentelemetry::trace::StartSpanOptions makeOptionsShim(
    const opentracing::StartSpanOptions &options) noexcept
{
  using opentracing::SpanReferenceType;
  // If an explicit start timestamp is specified, a conversion MUST
  // be done to match the OpenTracing and OpenTelemetry units.
  opentelemetry::trace::StartSpanOptions options_shim;
  options_shim.start_system_time =
      opentelemetry::common::SystemTimestamp{options.start_system_timestamp};
  options_shim.start_steady_time =
      opentelemetry::common::SteadyTimestamp{options.start_steady_timestamp};

  const auto &refs = options.references;
  // If a list of Span references is specified...
  if (!refs.empty())
  {
    const auto &first_child_of = std::find_if(
        refs.cbegin(), refs.cend(),
        [](const std::pair<SpanReferenceType, const opentracing::SpanContext *> &entry) {
          return entry.first == SpanReferenceType::ChildOfRef;
        });
    // The first SpanContext with Child Of type in the entire list is used as parent,
    // else the first SpanContext is used as parent
    auto context = (first_child_of != refs.cend()) ? first_child_of->second : refs.cbegin()->second;

    if (auto context_shim = SpanContextShim::extractFrom(context))
    {
      options_shim.parent = context_shim->context();
    }
  }

  return options_shim;
}

LinksIterable makeIterableLinks(const opentracing::StartSpanOptions &options) noexcept
{
  return LinksIterable(options.references);
}

TagsIterable makeIterableTags(const opentracing::StartSpanOptions &options) noexcept
{
  return TagsIterable(options.tags);
}

nostd::shared_ptr<opentelemetry::baggage::Baggage> makeBaggage(
    const opentracing::StartSpanOptions &options) noexcept
{
  using namespace opentelemetry::baggage;

  std::unordered_map<std::string, std::string> baggage_items;
  // If a list of Span references is specified...
  for (const auto &entry : options.references)
  {
    // The union of their Baggage values MUST be used
    // as the initial Baggage of the newly created Span.
    entry.second->ForeachBaggageItem(
        [&baggage_items](const std::string &key, const std::string &value) {
          // It is unspecified which Baggage value is used in the case of repeated keys.
          return baggage_items.emplace(key, value).second;
        });
  }
  // If no such list of references is specified, the current Baggage
  // MUST be used as the initial value of the newly created Span.
  return baggage_items.empty() ? GetBaggage(opentelemetry::context::RuntimeContext::GetCurrent())
                               : nostd::shared_ptr<Baggage>(new Baggage(baggage_items));
}

}  // namespace utils
}  // namespace opentracingshim
OPENTELEMETRY_END_NAMESPACE
