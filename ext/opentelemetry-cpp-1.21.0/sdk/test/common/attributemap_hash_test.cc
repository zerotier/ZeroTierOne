// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/sdk/common/attributemap_hash.h"
#include <gtest/gtest.h>

using namespace opentelemetry::sdk::common;
TEST(AttributeMapHashTest, BasicTests)
{
  {
    OrderedAttributeMap map1 = {{"k1", "v1"}, {"k2", "v2"}, {"k3", "v3"}};
    OrderedAttributeMap map2 = {{"k1", "v1"}, {"k2", "v2"}, {"k3", "v3"}, {"k4", "v4"}};
    OrderedAttributeMap map3 = {{"k3", "v3"}, {"k1", "v1"}, {"k2", "v2"}};

    EXPECT_TRUE(GetHashForAttributeMap(map1) != 0);
    EXPECT_TRUE(GetHashForAttributeMap(map1) == GetHashForAttributeMap(map1));
    EXPECT_TRUE(GetHashForAttributeMap(map1) != GetHashForAttributeMap(map2));
    EXPECT_TRUE(GetHashForAttributeMap(map1) == GetHashForAttributeMap(map3));
  }

  {
    // hash algo returns same value irrespective of order of attributes.
    OrderedAttributeMap map1 = {{"k1", 10}, {"k2", true}, {"k3", 12.22}};
    OrderedAttributeMap map2 = {{"k3", 12.22}, {"k1", 10}, {"k2", true}};
    EXPECT_TRUE(GetHashForAttributeMap(map1) == GetHashForAttributeMap(map2));
    EXPECT_TRUE(GetHashForAttributeMap(map1) != 0);
  }

  {
    OrderedAttributeMap map1 = {};
    EXPECT_TRUE(GetHashForAttributeMap(map1) == 0);
  }
}
