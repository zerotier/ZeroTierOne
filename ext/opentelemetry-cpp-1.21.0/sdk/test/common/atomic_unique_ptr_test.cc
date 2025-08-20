// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/sdk/common/atomic_unique_ptr.h"

#include <gtest/gtest.h>
#include <string>

using opentelemetry::sdk::common::AtomicUniquePtr;

TEST(AtomicUniquePtrTest, SwapIfNullWithNull)
{
  AtomicUniquePtr<int> ptr;
  EXPECT_TRUE(ptr.IsNull());

  std::unique_ptr<int> x{new int{33}};
  EXPECT_TRUE(ptr.SwapIfNull(x));
  EXPECT_EQ(x, nullptr);
}

TEST(AtomicUniquePtrTest, SwapIfNullWithNonNull)
{
  AtomicUniquePtr<int> ptr;
  ptr.Reset(new int{11});
  std::unique_ptr<int> x{new int{33}};
  EXPECT_TRUE(!ptr.SwapIfNull(x));
  EXPECT_NE(x, nullptr);
  EXPECT_EQ(*x, 33);
  EXPECT_EQ(*ptr, 11);
}

TEST(AtomicUniquePtrTest, Swap)
{
  AtomicUniquePtr<int> ptr;
  EXPECT_TRUE(ptr.IsNull());

  ptr.Reset(new int{11});
  std::unique_ptr<int> x{new int{33}};
  ptr.Swap(x);
  EXPECT_FALSE(ptr.IsNull());
  EXPECT_NE(x, nullptr);
  EXPECT_EQ(*x, 11);
  EXPECT_EQ(*ptr, 33);
}
