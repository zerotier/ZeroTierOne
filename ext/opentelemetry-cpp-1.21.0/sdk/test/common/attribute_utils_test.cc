// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <stdint.h>
#include <array>
#include <initializer_list>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/common/key_value_iterable_view.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/sdk/common/attribute_utils.h"

TEST(AttributeMapTest, DefaultConstruction)
{
  opentelemetry::sdk::common::AttributeMap attribute_map;
  EXPECT_EQ(attribute_map.GetAttributes().size(), 0);
}

TEST(OrderedAttributeMapTest, DefaultConstruction)
{
  opentelemetry::sdk::common::OrderedAttributeMap attribute_map;
  EXPECT_EQ(attribute_map.GetAttributes().size(), 0);
}

TEST(AttributeMapTest, AttributesConstruction)
{
  const int kNumAttributes              = 3;
  std::string keys[kNumAttributes]      = {"attr1", "attr2", "attr3"};
  int values[kNumAttributes]            = {15, 24, 37};
  std::map<std::string, int> attributes = {
      {keys[0], values[0]}, {keys[1], values[1]}, {keys[2], values[2]}};

  opentelemetry::common::KeyValueIterableView<std::map<std::string, int>> iterable(attributes);
  opentelemetry::sdk::common::AttributeMap attribute_map(iterable);

  for (int i = 0; i < kNumAttributes; i++)
  {
    EXPECT_EQ(opentelemetry::nostd::get<int>(attribute_map.GetAttributes().at(keys[i])), values[i]);
  }
}

TEST(OrderedAttributeMapTest, AttributesConstruction)
{
  const int kNumAttributes              = 3;
  std::string keys[kNumAttributes]      = {"attr1", "attr2", "attr3"};
  int values[kNumAttributes]            = {15, 24, 37};
  std::map<std::string, int> attributes = {
      {keys[0], values[0]}, {keys[1], values[1]}, {keys[2], values[2]}};

  opentelemetry::common::KeyValueIterableView<std::map<std::string, int>> iterable(attributes);
  opentelemetry::sdk::common::OrderedAttributeMap attribute_map(iterable);

  for (int i = 0; i < kNumAttributes; i++)
  {
    EXPECT_EQ(opentelemetry::nostd::get<int>(attribute_map.GetAttributes().at(keys[i])), values[i]);
  }
}

TEST(AttributeEqualToVisitorTest, AttributeValueEqualTo)
{
  namespace sdk   = opentelemetry::sdk::common;
  namespace api   = opentelemetry::common;
  namespace nostd = opentelemetry::nostd;

  using AV = api::AttributeValue;
  using OV = sdk::OwnedAttributeValue;

  sdk::AttributeEqualToVisitor equal_to_visitor;

  // arithmetic types
  EXPECT_TRUE(opentelemetry::nostd::visit(equal_to_visitor, OV{bool(true)}, AV{bool(true)}));
  EXPECT_TRUE(opentelemetry::nostd::visit(equal_to_visitor, OV{int32_t(22)}, AV{int32_t(22)}));
  EXPECT_TRUE(opentelemetry::nostd::visit(equal_to_visitor, OV{int64_t(22)}, AV{int64_t(22)}));
  EXPECT_TRUE(opentelemetry::nostd::visit(equal_to_visitor, OV{uint32_t(22)}, AV{uint32_t(22)}));
  EXPECT_TRUE(opentelemetry::nostd::visit(equal_to_visitor, OV{uint64_t(22)}, AV{uint64_t(22)}));
  EXPECT_TRUE(opentelemetry::nostd::visit(equal_to_visitor, OV{double(22.0)}, AV{double(22.0)}));

  // string types
  EXPECT_TRUE(opentelemetry::nostd::visit(
      equal_to_visitor, OV{std::string("string to const char*")}, AV{"string to const char*"}));
  EXPECT_TRUE(opentelemetry::nostd::visit(equal_to_visitor,
                                          OV{std::string("string to string_view")},
                                          AV{nostd::string_view("string to string_view")}));

  // container types
  EXPECT_TRUE(opentelemetry::nostd::visit(equal_to_visitor, OV{std::vector<bool>{true, false}},
                                          AV{std::array<const bool, 2>{true, false}}));
  EXPECT_TRUE(opentelemetry::nostd::visit(equal_to_visitor, OV{std::vector<uint8_t>{33, 44}},
                                          AV{std::array<const uint8_t, 2>{33, 44}}));
  EXPECT_TRUE(opentelemetry::nostd::visit(equal_to_visitor, OV{std::vector<int32_t>{33, 44}},
                                          AV{std::array<const int32_t, 2>{33, 44}}));
  EXPECT_TRUE(opentelemetry::nostd::visit(equal_to_visitor, OV{std::vector<int64_t>{33, 44}},
                                          AV{std::array<const int64_t, 2>{33, 44}}));
  EXPECT_TRUE(opentelemetry::nostd::visit(equal_to_visitor, OV{std::vector<uint32_t>{33, 44}},
                                          AV{std::array<const uint32_t, 2>{33, 44}}));
  EXPECT_TRUE(opentelemetry::nostd::visit(equal_to_visitor, OV{std::vector<uint64_t>{33, 44}},
                                          AV{std::array<const uint64_t, 2>{33, 44}}));
  EXPECT_TRUE(opentelemetry::nostd::visit(equal_to_visitor, OV{std::vector<double>{33.0, 44.0}},
                                          AV{std::array<const double, 2>{33.0, 44.0}}));
  EXPECT_TRUE(opentelemetry::nostd::visit(
      equal_to_visitor, OV{std::vector<std::string>{"a string", "another string"}},
      AV{std::array<const nostd::string_view, 2>{"a string", "another string"}}));
}

TEST(AttributeEqualToVisitorTest, AttributeValueNotEqualTo)
{
  namespace sdk   = opentelemetry::sdk::common;
  namespace api   = opentelemetry::common;
  namespace nostd = opentelemetry::nostd;

  using AV = api::AttributeValue;
  using OV = sdk::OwnedAttributeValue;

  sdk::AttributeEqualToVisitor equal_to_visitor;

  // check different values of the same type
  EXPECT_FALSE(opentelemetry::nostd::visit(equal_to_visitor, OV{bool(true)}, AV{bool(false)}));
  EXPECT_FALSE(opentelemetry::nostd::visit(equal_to_visitor, OV{int32_t(22)}, AV{int32_t(33)}));
  EXPECT_FALSE(opentelemetry::nostd::visit(equal_to_visitor, OV{int64_t(22)}, AV{int64_t(33)}));
  EXPECT_FALSE(opentelemetry::nostd::visit(equal_to_visitor, OV{uint32_t(22)}, AV{uint32_t(33)}));
  EXPECT_FALSE(opentelemetry::nostd::visit(equal_to_visitor, OV{double(22.2)}, AV{double(33.3)}));
  EXPECT_FALSE(opentelemetry::nostd::visit(equal_to_visitor, OV{std::string("string one")},
                                           AV{"another string"}));
  EXPECT_FALSE(opentelemetry::nostd::visit(equal_to_visitor, OV{std::string("string one")},
                                           AV{nostd::string_view("another string")}));

  // check different value types
  EXPECT_FALSE(opentelemetry::nostd::visit(equal_to_visitor, OV{bool(true)}, AV{uint32_t(1)}));
  EXPECT_FALSE(opentelemetry::nostd::visit(equal_to_visitor, OV{int32_t(22)}, AV{uint32_t(22)}));

  // check containers of different element values
  EXPECT_FALSE(opentelemetry::nostd::visit(equal_to_visitor, OV{std::vector<bool>{true, false}},
                                           AV{std::array<const bool, 2>{false, true}}));
  EXPECT_FALSE(opentelemetry::nostd::visit(equal_to_visitor, OV{std::vector<int32_t>{22, 33}},
                                           AV{std::array<const int32_t, 2>{33, 44}}));
  EXPECT_FALSE(opentelemetry::nostd::visit(
      equal_to_visitor, OV{std::vector<std::string>{"a string", "another string"}},
      AV{std::array<const nostd::string_view, 2>{"a string", "a really different string"}}));

  // check containers of different element types
  EXPECT_FALSE(opentelemetry::nostd::visit(equal_to_visitor, OV{std::vector<int32_t>{22, 33}},
                                           AV{std::array<const uint32_t, 2>{22, 33}}));
}

TEST(AttributeMapTest, EqualTo)
{
  using Attributes = std::initializer_list<
      std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue>>;

  // check for case where both are empty
  Attributes attributes_empty = {};
  auto kv_iterable_empty =
      opentelemetry::common::MakeKeyValueIterableView<Attributes>(attributes_empty);
  opentelemetry::sdk::common::AttributeMap attribute_map_empty(kv_iterable_empty);
  EXPECT_TRUE(attribute_map_empty.EqualTo(kv_iterable_empty));

  // check for equality with a range of attributes and types
  Attributes attributes  = {{"key0", "some value"}, {"key1", 1}, {"key2", 2.0}, {"key3", true}};
  auto kv_iterable_match = opentelemetry::common::MakeKeyValueIterableView<Attributes>(attributes);
  opentelemetry::sdk::common::AttributeMap attribute_map(attributes);
  EXPECT_TRUE(attribute_map.EqualTo(kv_iterable_match));

  // check for several cases where the attributes are different
  Attributes attributes_different_value = {
      {"key0", "some value"}, {"key1", 1}, {"key2", 2.0}, {"key3", false}};
  Attributes attributes_different_type = {
      {"key0", "some value"}, {"key1", 1.0}, {"key2", 2.0}, {"key3", true}};
  Attributes attributes_different_size = {{"key0", "some value"}};

  // check for the case where the number of attributes is the same but all keys are different
  Attributes attributes_different_all = {{"a", "b"}, {"c", "d"}, {"e", 4.0}, {"f", uint8_t(5)}};

  auto kv_iterable_different_value =
      opentelemetry::common::MakeKeyValueIterableView<Attributes>(attributes_different_value);
  auto kv_iterable_different_type =
      opentelemetry::common::MakeKeyValueIterableView<Attributes>(attributes_different_type);
  auto kv_iterable_different_size =
      opentelemetry::common::MakeKeyValueIterableView<Attributes>(attributes_different_size);
  auto kv_iterable_different_all =
      opentelemetry::common::MakeKeyValueIterableView<Attributes>(attributes_different_all);

  EXPECT_FALSE(attribute_map.EqualTo(kv_iterable_different_value));
  EXPECT_FALSE(attribute_map.EqualTo(kv_iterable_different_type));
  EXPECT_FALSE(attribute_map.EqualTo(kv_iterable_different_size));
  EXPECT_FALSE(attribute_map.EqualTo(kv_iterable_different_all));
}
