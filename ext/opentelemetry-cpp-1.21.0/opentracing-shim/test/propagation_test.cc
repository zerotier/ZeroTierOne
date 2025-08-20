/*
 * Copyright The OpenTelemetry Authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

#include "opentracing/expected/expected.hpp"
#include "opentracing/propagation.h"
#include "opentracing/string_view.h"
#include "opentracing/util.h"

#include "opentelemetry/baggage/baggage.h"
#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/nostd/function_ref.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/opentracingshim/propagation.h"

#include "shim_mocks.h"

namespace baggage = opentelemetry::baggage;
namespace common  = opentelemetry::common;
namespace nostd   = opentelemetry::nostd;
namespace shim    = opentelemetry::opentracingshim;

TEST(PropagationTest, TextMapReader_Get_LookupKey_Unsupported)
{
  std::unordered_map<std::string, std::string> text_map;
  TextMapCarrier testee{text_map};
  ASSERT_FALSE(testee.supports_lookup);
  ASSERT_EQ(testee.foreach_key_call_count, 0);

  shim::CarrierReaderShim tester{testee};
  auto lookup_unsupported = testee.LookupKey("foo");
  ASSERT_TRUE(text_map.empty());
  ASSERT_TRUE(opentracing::are_errors_equal(lookup_unsupported.error(),
                                            opentracing::lookup_key_not_supported_error));
  ASSERT_EQ(tester.Get("foo"), nostd::string_view{});
  ASSERT_EQ(testee.foreach_key_call_count, 1);

  text_map["foo"]               = "bar";
  auto lookup_still_unsupported = testee.LookupKey("foo");
  ASSERT_FALSE(text_map.empty());
  ASSERT_TRUE(opentracing::are_errors_equal(lookup_still_unsupported.error(),
                                            opentracing::lookup_key_not_supported_error));
  ASSERT_EQ(tester.Get("foo"), nostd::string_view{"bar"});
  ASSERT_EQ(testee.foreach_key_call_count, 2);
}

TEST(PropagationTest, TextMapReader_Get_LookupKey_Supported)
{
  std::unordered_map<std::string, std::string> text_map;
  TextMapCarrier testee{text_map};
  testee.supports_lookup = true;
  ASSERT_TRUE(testee.supports_lookup);
  ASSERT_EQ(testee.foreach_key_call_count, 0);

  shim::CarrierReaderShim tester{testee};
  auto lookup_not_found = testee.LookupKey("foo");
  ASSERT_TRUE(text_map.empty());
  ASSERT_TRUE(
      opentracing::are_errors_equal(lookup_not_found.error(), opentracing::key_not_found_error));
  ASSERT_EQ(tester.Get("foo"), nostd::string_view{});
  ASSERT_EQ(testee.foreach_key_call_count, 1);

  text_map["foo"]   = "bar";
  auto lookup_found = testee.LookupKey("foo");
  ASSERT_FALSE(text_map.empty());
  ASSERT_EQ(lookup_found.value(), opentracing::string_view{"bar"});
  ASSERT_EQ(tester.Get("foo"), nostd::string_view{"bar"});
  ASSERT_EQ(testee.foreach_key_call_count, 1);
}

TEST(PropagationTest, TextMapReader_Keys)
{
  std::unordered_map<std::string, std::string> text_map;
  TextMapCarrier testee{text_map};
  ASSERT_EQ(testee.foreach_key_call_count, 0);

  shim::CarrierReaderShim tester{testee};
  std::vector<nostd::string_view> kvs;
  auto callback = [&text_map, &kvs](nostd::string_view k) {
    kvs.emplace_back(k);
    return !text_map.empty();
  };

  ASSERT_TRUE(tester.Keys(callback));
  ASSERT_TRUE(text_map.empty());
  ASSERT_TRUE(kvs.empty());
  ASSERT_EQ(testee.foreach_key_call_count, 1);

  text_map["foo"] = "bar";
  text_map["bar"] = "baz";
  text_map["baz"] = "foo";
  ASSERT_TRUE(tester.Keys(callback));
  ASSERT_FALSE(text_map.empty());
  ASSERT_EQ(text_map.size(), kvs.size());
  ASSERT_EQ(testee.foreach_key_call_count, 2);
}

TEST(PropagationTest, TextMapWriter_Set)
{
  std::unordered_map<std::string, std::string> text_map;
  TextMapCarrier testee{text_map};
  shim::CarrierWriterShim tester{testee};
  ASSERT_TRUE(text_map.empty());

  tester.Set("foo", "bar");
  tester.Set("bar", "baz");
  tester.Set("baz", "foo");
  ASSERT_EQ(text_map.size(), 3);
  ASSERT_EQ(text_map["foo"], "bar");
  ASSERT_EQ(text_map["bar"], "baz");
  ASSERT_EQ(text_map["baz"], "foo");
}
