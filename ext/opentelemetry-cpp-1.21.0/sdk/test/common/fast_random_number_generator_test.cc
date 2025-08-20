// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <cstdint>
#include <random>
#include <set>
#include <string>
#include <utility>

#include "src/common/fast_random_number_generator.h"

using opentelemetry::sdk::common::FastRandomNumberGenerator;

TEST(FastRandomNumberGeneratorTest, GenerateUniqueNumbers)
{
  std::seed_seq seed_sequence{1, 2, 3};
  FastRandomNumberGenerator random_number_generator;
  random_number_generator.seed(seed_sequence);
  std::set<uint64_t> values;
  for (int i = 0; i < 1000; ++i)
  {
    EXPECT_TRUE(values.insert(random_number_generator()).second);
  }
}
