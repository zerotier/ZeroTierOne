// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>

#include "opentelemetry/common/key_value_iterable_view.h"
#include "opentelemetry/sdk/common/attribute_utils.h"
#include "opentelemetry/sdk/metrics/view/attributes_processor.h"

using namespace opentelemetry::sdk::metrics;
using namespace opentelemetry::common;
using namespace opentelemetry::sdk::common;

TEST(AttributesProcessor, FilteringAttributesProcessor)
{
  const int kNumFilterAttributes               = 3;
  std::unordered_map<std::string, bool> filter = {
      {"attr2", true}, {"attr4", true}, {"attr6", true}};
  const int kNumAttributes              = 6;
  std::string keys[kNumAttributes]      = {"attr1", "attr2", "attr3", "attr4", "attr5", "attr6"};
  int values[kNumAttributes]            = {10, 20, 30, 40, 50, 60};
  std::map<std::string, int> attributes = {{keys[0], values[0]}, {keys[1], values[1]},
                                           {keys[2], values[2]}, {keys[3], values[3]},
                                           {keys[4], values[4]}, {keys[5], values[5]}};
  FilteringAttributesProcessor attributes_processor(filter);
  opentelemetry::common::KeyValueIterableView<std::map<std::string, int>> iterable(attributes);
  auto filtered_attributes = attributes_processor.process(iterable);
  for (auto &e : filtered_attributes)
  {
    EXPECT_FALSE(filter.find(e.first) == filter.end());
  }
  EXPECT_EQ(filter.size(), kNumFilterAttributes);
}

TEST(AttributesProcessor, FilteringAllAttributesProcessor)
{
  const int kNumFilterAttributes               = 0;
  std::unordered_map<std::string, bool> filter = {};
  const int kNumAttributes                     = 6;
  std::string keys[kNumAttributes]      = {"attr1", "attr2", "attr3", "attr4", "attr5", "attr6"};
  int values[kNumAttributes]            = {10, 20, 30, 40, 50, 60};
  std::map<std::string, int> attributes = {{keys[0], values[0]}, {keys[1], values[1]},
                                           {keys[2], values[2]}, {keys[3], values[3]},
                                           {keys[4], values[4]}, {keys[5], values[5]}};
  FilteringAttributesProcessor attributes_processor(filter);
  opentelemetry::common::KeyValueIterableView<std::map<std::string, int>> iterable(attributes);
  auto filtered_attributes = attributes_processor.process(iterable);
  EXPECT_EQ(filter.size(), kNumFilterAttributes);
}
