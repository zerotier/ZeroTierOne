/*
 * Copyright The OpenTelemetry Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "opentracing/propagation.h"
#include "opentracing/span.h"
#include "opentracing/string_view.h"
#include "opentracing/tracer.h"
#include "opentracing/value.h"
#include "opentracing/variant/recursive_wrapper.hpp"

#include "opentelemetry/baggage/baggage.h"
#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/common/key_value_iterable.h"
#include "opentelemetry/common/key_value_iterable_view.h"
#include "opentelemetry/nostd/function_ref.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/opentracingshim/span_context_shim.h"
#include "opentelemetry/semconv/incubating/opentracing_attributes.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/span_context_kv_iterable.h"
#include "opentelemetry/trace/span_startoptions.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace opentracingshim
{
namespace utils
{

static inline opentelemetry::common::AttributeValue attributeFromValue(
    const opentracing::Value &value)
{
  using opentelemetry::common::AttributeValue;

  static struct
  {
    AttributeValue operator()(bool v) { return v; }
    AttributeValue operator()(double v) { return v; }
    AttributeValue operator()(int64_t v) { return v; }
    AttributeValue operator()(uint64_t v) { return v; }
    AttributeValue operator()(const std::string &v) { return nostd::string_view{v}; }
    AttributeValue operator()(opentracing::string_view v)
    {
      return nostd::string_view{v.data(), v.size()};
    }
    AttributeValue operator()(std::nullptr_t) { return nostd::string_view{}; }
    AttributeValue operator()(const char *v) { return v; }
    AttributeValue operator()(opentracing::util::recursive_wrapper<opentracing::Values>)
    {
      return nostd::string_view{};
    }
    AttributeValue operator()(opentracing::util::recursive_wrapper<opentracing::Dictionary>)
    {
      return nostd::string_view{};
    }
  } AttributeMapper;

  return opentracing::Value::visit(value, AttributeMapper);
}

static inline std::string stringFromValue(const opentracing::Value &value)
{
  static struct
  {
    std::string operator()(bool v) { return v ? "true" : "false"; }
    std::string operator()(double v) { return std::to_string(v); }
    std::string operator()(int64_t v) { return std::to_string(v); }
    std::string operator()(uint64_t v) { return std::to_string(v); }
    std::string operator()(const std::string &v) { return v; }
    std::string operator()(opentracing::string_view v) { return std::string{v.data(), v.size()}; }
    std::string operator()(std::nullptr_t) { return std::string{}; }
    std::string operator()(const char *v) { return std::string{v}; }
    std::string operator()(opentracing::util::recursive_wrapper<opentracing::Values>)
    {
      return std::string{};
    }
    std::string operator()(opentracing::util::recursive_wrapper<opentracing::Dictionary>)
    {
      return std::string{};
    }
  } StringMapper;

  return opentracing::Value::visit(value, StringMapper);
}

static inline bool isBaggageEmpty(const nostd::shared_ptr<opentelemetry::baggage::Baggage> &baggage)
{
  if (baggage)
  {
    return baggage->GetAllEntries([](nostd::string_view, nostd::string_view) { return false; });
  }

  return true;
}

class LinksIterable final : public opentelemetry::trace::SpanContextKeyValueIterable
{
public:
  using RefsList =
      std::vector<std::pair<opentracing::SpanReferenceType, const opentracing::SpanContext *>>;

  explicit LinksIterable(const RefsList &refs) noexcept : refs_(refs) {}

  bool ForEachKeyValue(nostd::function_ref<bool(opentelemetry::trace::SpanContext,
                                                const opentelemetry::common::KeyValueIterable &)>
                           callback) const noexcept override
  {
    using opentracing::SpanReferenceType;
    using namespace opentelemetry::semconv;
    using LinksList = std::initializer_list<std::pair<nostd::string_view, common::AttributeValue>>;

    for (const auto &entry : refs_)
    {
      nostd::string_view span_kind;

      if (entry.first == SpanReferenceType::ChildOfRef)
      {
        span_kind = opentracing::OpentracingRefTypeValues::kChildOf;
      }
      else if (entry.first == SpanReferenceType::FollowsFromRef)
      {
        span_kind = opentracing::OpentracingRefTypeValues::kFollowsFrom;
      }

      auto context_shim = SpanContextShim::extractFrom(entry.second);

      if (context_shim && !span_kind.empty() &&
          !callback(context_shim->context(), opentelemetry::common::KeyValueIterableView<LinksList>(
                                                 {{opentracing::kOpentracingRefType, span_kind}})))
      {
        return false;
      }
    }

    return true;
  }

  size_t size() const noexcept override { return refs_.size(); }

private:
  const RefsList &refs_;
};

class TagsIterable final : public opentelemetry::common::KeyValueIterable
{
public:
  explicit TagsIterable(
      const std::vector<std::pair<std::string, opentracing::Value>> &tags) noexcept
      : tags_(tags)
  {}

  bool ForEachKeyValue(nostd::function_ref<bool(nostd::string_view, common::AttributeValue)>
                           callback) const noexcept override
  {
    for (const auto &entry : tags_)
    {
      if (!callback(entry.first, utils::attributeFromValue(entry.second)))
        return false;
    }
    return true;
  }

  size_t size() const noexcept override { return tags_.size(); }

private:
  const std::vector<std::pair<std::string, opentracing::Value>> &tags_;
};

opentelemetry::trace::StartSpanOptions makeOptionsShim(
    const opentracing::StartSpanOptions &options) noexcept;
LinksIterable makeIterableLinks(const opentracing::StartSpanOptions &options) noexcept;
TagsIterable makeIterableTags(const opentracing::StartSpanOptions &options) noexcept;
nostd::shared_ptr<opentelemetry::baggage::Baggage> makeBaggage(
    const opentracing::StartSpanOptions &options) noexcept;

}  // namespace utils
}  // namespace opentracingshim
OPENTELEMETRY_END_NAMESPACE
