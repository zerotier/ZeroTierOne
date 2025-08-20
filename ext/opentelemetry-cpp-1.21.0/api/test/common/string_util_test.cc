// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <string.h>

#include <opentelemetry/common/string_util.h>

// ------------------------- StringUtil class tests ---------------------------------

using opentelemetry::common::StringUtil;

TEST(StringUtilTest, TrimStringWithIndex)
{
  struct
  {
    const char *input;
    const char *expected;
  } testcases[] = {{"k1=v1", "k1=v1"},     {"k1=v1,k2=v2, k3=v3", "k1=v1,k2=v2, k3=v3"},
                   {"   k1=v1", "k1=v1"},  {"k1=v1   ", "k1=v1"},
                   {"   k1=v1 ", "k1=v1"}, {"  ", ""}};
  for (auto &testcase : testcases)
  {
    EXPECT_EQ(StringUtil::Trim(testcase.input, 0, strlen(testcase.input) - 1), testcase.expected);
  }
}

TEST(StringUtilTest, TrimString)
{
  struct
  {
    const char *input;
    const char *expected;
  } testcases[] = {{"k1=v1", "k1=v1"},
                   {"k1=v1,k2=v2, k3=v3", "k1=v1,k2=v2, k3=v3"},
                   {"   k1=v1", "k1=v1"},
                   {"k1=v1   ", "k1=v1"},
                   {"k1=v1\t", "k1=v1"},
                   {"\t k1=v1 \t", "k1=v1"},
                   {"\t\t k1=v1\t  ", "k1=v1"},
                   {"\t\t k1=v1\t  ,k2=v2", "k1=v1\t  ,k2=v2"},
                   {"   k1=v1 ", "k1=v1"},
                   {" ", ""},
                   {"", ""},
                   {"\n_some string_\t", "_some string_"}};

  for (auto &testcase : testcases)
  {
    EXPECT_EQ(StringUtil::Trim(testcase.input), testcase.expected);
  }
}
