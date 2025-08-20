// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <stddef.h>
#include <ostream>
#include <vector>

#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/trace/propagation/detail/string.h"

using namespace opentelemetry;

namespace
{

struct SplitStringTestData
{
  opentelemetry::nostd::string_view input;
  char separator;
  size_t max_count;
  size_t expected_number_strings;

  // When googletest registers parameterized tests, it uses this method to format the parameters.
  // The default implementation prints hex dump of all bytes in the object. If there is any padding
  // in these bytes, valgrind reports this as a warning - "Use of uninitialized bytes".
  // See https://github.com/google/googletest/issues/3805.
  friend void PrintTo(const SplitStringTestData &data, std::ostream *os)
  {
    *os << "(" << data.input << "," << data.separator << "," << data.max_count << ","
        << data.expected_number_strings << ")";
  }
};

const SplitStringTestData split_string_test_cases[] = {
    {"foo,bar,baz", ',', 4, 3},
    {"foo,bar,baz,foobar", ',', 4, 4},
    {"foo,bar,baz,foobar", '.', 4, 1},
    {"foo,bar,baz,", ',', 4, 4},
    {"foo,bar,baz,", ',', 2, 2},
    {"foo ,bar, baz ", ',', 4, 3},
    {"foo ,bar, baz ", ',', 4, 3},
    {"00-0af7651916cd43dd8448eb211c80319c-00f067aa0ba902b7-01", '-', 4, 4},
};
}  // namespace

// Test fixture
class SplitStringTestFixture : public ::testing::TestWithParam<SplitStringTestData>
{};

TEST_P(SplitStringTestFixture, SplitsAsExpected)
{
  const SplitStringTestData test_param = GetParam();
  std::vector<opentelemetry::nostd::string_view> fields(test_param.expected_number_strings);
  size_t got_splits_num = opentelemetry::trace::propagation::detail::SplitString(
      test_param.input, test_param.separator, fields.data(), test_param.max_count);

  // Assert on the output
  EXPECT_EQ(got_splits_num, test_param.expected_number_strings);
}

INSTANTIATE_TEST_SUITE_P(SplitStringTestCases,
                         SplitStringTestFixture,
                         ::testing::ValuesIn(split_string_test_cases));
