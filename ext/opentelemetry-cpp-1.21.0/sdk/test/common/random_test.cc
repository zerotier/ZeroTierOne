// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include "opentelemetry/nostd/span.h"
#include "src/common/random.h"

using opentelemetry::sdk::common::Random;

TEST(RandomTest, GenerateRandom64)
{
  EXPECT_NE(Random::GenerateRandom64(), Random::GenerateRandom64());
}

TEST(RandomTest, GenerateRandomBuffer)
{
  uint8_t buf1_array[8] = {0};
  uint8_t buf2_array[8] = {0};
  Random::GenerateRandomBuffer(buf1_array);
  Random::GenerateRandomBuffer(buf2_array);
  EXPECT_FALSE(std::equal(std::begin(buf1_array), std::end(buf1_array), std::begin(buf2_array)));

  // Edge cases.
  for (auto size : {7, 8, 9, 16, 17})
  {
    std::vector<uint8_t> buf1_vector(size);
    std::vector<uint8_t> buf2_vector(size);

    Random::GenerateRandomBuffer(buf1_vector);
    Random::GenerateRandomBuffer(buf2_vector);
    EXPECT_FALSE(
        std::equal(std::begin(buf1_vector), std::end(buf1_vector), std::begin(buf2_vector)));
  }
}

void doSomethingOnce(std::atomic_uint *count)
{
  static std::atomic_flag flag;
  if (!flag.test_and_set())
  {
    (*count)++;
  }
}

TEST(RandomTest, AtomicFlagMultiThreadTest)
{
  std::vector<std::thread> threads;
  std::atomic_uint count(0);
  threads.reserve(10);
  for (int i = 0; i < 10; ++i)
  {
    threads.push_back(std::thread(doSomethingOnce, &count));
  }
  for (auto &t : threads)
  {
    t.join();
  }
  EXPECT_EQ(1, count.load());
}
