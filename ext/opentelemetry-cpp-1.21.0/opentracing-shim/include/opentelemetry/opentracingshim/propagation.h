/*
 * Copyright The OpenTelemetry Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "opentracing/propagation.h"
#include "opentracing/value.h"

#include "opentelemetry/baggage/baggage.h"
#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/context/propagation/text_map_propagator.h"
#include "opentelemetry/nostd/function_ref.h"
#include "opentelemetry/nostd/string_view.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace opentracingshim
{

class CarrierWriterShim : public opentelemetry::context::propagation::TextMapCarrier
{
public:
  CarrierWriterShim(const opentracing::TextMapWriter &writer) : writer_(writer) {}

  virtual nostd::string_view Get(nostd::string_view) const noexcept override
  {
    return {};  // Not required for Opentracing writer
  }

  virtual void Set(nostd::string_view key, nostd::string_view value) noexcept override
  {
    writer_.Set(opentracing::string_view{key.data(), key.size()},
                opentracing::string_view{value.data(), value.size()});
  }

private:
  const opentracing::TextMapWriter &writer_;
};

class CarrierReaderShim : public opentelemetry::context::propagation::TextMapCarrier
{
public:
  CarrierReaderShim(const opentracing::TextMapReader &reader) : reader_(reader) {}

  virtual nostd::string_view Get(nostd::string_view key) const noexcept override
  {
    nostd::string_view value;

    // First try carrier.LookupKey since that can potentially be the fastest approach.
    if (auto result = reader_.LookupKey(opentracing::string_view{key.data(), key.size()}))
    {
      value = nostd::string_view{result.value().data(), result.value().size()};
    }
    else  // Fall back to iterating through all of the keys.
    {
      reader_.ForeachKey([key, &value](opentracing::string_view k,
                                       opentracing::string_view v) -> opentracing::expected<void> {
        if (key == nostd::string_view{k.data(), k.size()})
        {
          value = nostd::string_view{v.data(), v.size()};
          // Found key, so bail out of the loop with a success error code.
          return opentracing::make_unexpected(std::error_code{});
        }
        return opentracing::make_expected();
      });
    }

    return value;
  }

  virtual void Set(nostd::string_view, nostd::string_view) noexcept override
  {
    // Not required for Opentracing reader
  }

  virtual bool Keys(nostd::function_ref<bool(nostd::string_view)> callback) const noexcept override
  {
    return reader_
        .ForeachKey([&callback](opentracing::string_view key,
                                opentracing::string_view) -> opentracing::expected<void> {
          return callback(nostd::string_view{key.data(), key.size()})
                     ? opentracing::make_expected()
                     : opentracing::make_unexpected(std::error_code{});
        })
        .has_value();
  }

private:
  const opentracing::TextMapReader &reader_;
};

}  // namespace opentracingshim
OPENTELEMETRY_END_NAMESPACE
