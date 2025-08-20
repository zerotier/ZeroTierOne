// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <stddef.h>
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "opentelemetry/sdk/common/atomic_unique_ptr.h"
#include "opentelemetry/sdk/common/circular_buffer.h"
#include "opentelemetry/sdk/common/circular_buffer_range.h"

using opentelemetry::sdk::common::AtomicUniquePtr;
using opentelemetry::sdk::common::CircularBuffer;
using opentelemetry::sdk::common::CircularBufferRange;

static thread_local std::mt19937 RandomNumberGenerator{std::random_device{}()};

static void GenerateRandomNumbers(CircularBuffer<uint32_t> &buffer,
                                  std::vector<uint32_t> &numbers,
                                  int n)
{
  for (int i = 0; i < n; ++i)
  {
    auto value = static_cast<uint32_t>(RandomNumberGenerator());
    std::unique_ptr<uint32_t> x{new uint32_t{value}};
    if (buffer.Add(x))
    {
      numbers.push_back(value);
    }
  }
}

static void RunNumberProducers(CircularBuffer<uint32_t> &buffer,
                               std::vector<uint32_t> &numbers,
                               int num_threads,
                               int n)
{
  std::vector<std::vector<uint32_t>> thread_numbers(num_threads);
  std::vector<std::thread> threads(num_threads);
  for (int thread_index = 0; thread_index < num_threads; ++thread_index)
  {
    threads[thread_index] = std::thread{GenerateRandomNumbers, std::ref(buffer),
                                        std::ref(thread_numbers[thread_index]), n};
  }
  for (auto &thread : threads)
  {
    thread.join();
  }
  for (int thread_index = 0; thread_index < num_threads; ++thread_index)
  {
    numbers.insert(numbers.end(), thread_numbers[thread_index].begin(),
                   thread_numbers[thread_index].end());
  }
}

void RunNumberConsumer(CircularBuffer<uint32_t> &buffer,
                       std::atomic<bool> &exit,
                       std::vector<uint32_t> &numbers)
{
  while (true)
  {
    if (exit && buffer.Peek().empty())
    {
      return;
    }
    auto n = std::uniform_int_distribution<size_t>{0, buffer.Peek().size()}(RandomNumberGenerator);
    buffer.Consume(n, [&](CircularBufferRange<AtomicUniquePtr<uint32_t>> range) noexcept {
      assert(range.size() == n);
      range.ForEach([&](AtomicUniquePtr<uint32_t> &ptr) noexcept {
        assert(!ptr.IsNull());
        numbers.push_back(*ptr);
        ptr.Reset();
        return true;
      });
    });
  }
}

TEST(CircularBufferTest, Add)
{
  CircularBuffer<int> buffer{10};

  std::unique_ptr<int> x{new int{11}};
  EXPECT_TRUE(buffer.Add(x));
  EXPECT_EQ(x, nullptr);
  auto range = buffer.Peek();
  EXPECT_EQ(range.size(), 1);
  range.ForEach([](const AtomicUniquePtr<int> &y) {
    EXPECT_EQ(*y, 11);
    return true;
  });
}

TEST(CircularBufferTest, Clear)
{
  CircularBuffer<int> buffer{10};

  std::unique_ptr<int> x{new int{11}};
  EXPECT_TRUE(buffer.Add(x));
  EXPECT_EQ(x, nullptr);
  buffer.Clear();
  EXPECT_TRUE(buffer.empty());
}

TEST(CircularBufferTest, AddOnFull)
{
  CircularBuffer<int> buffer{10};
  for (int i = 0; i < static_cast<int>(buffer.max_size()); ++i)
  {
    std::unique_ptr<int> x{new int{i}};
    EXPECT_TRUE(buffer.Add(x));
  }
  std::unique_ptr<int> x{new int{33}};
  EXPECT_FALSE(buffer.Add(x));
  EXPECT_NE(x, nullptr);
  EXPECT_EQ(*x, 33);
}

TEST(CircularBufferTest, Consume)
{
  CircularBuffer<int> buffer{10};
  for (int i = 0; i < static_cast<int>(buffer.max_size()); ++i)
  {
    std::unique_ptr<int> x{new int{i}};
    EXPECT_TRUE(buffer.Add(x));
  }
  int count = 0;
  buffer.Consume(5, [&](CircularBufferRange<AtomicUniquePtr<int>> range) noexcept {
    range.ForEach([&](AtomicUniquePtr<int> &ptr) {
      EXPECT_EQ(*ptr, count++);
      ptr.Reset();
      return true;
    });
  });
  EXPECT_EQ(count, 5);
}

TEST(CircularBufferTest, Simulation)
{
  const int num_producer_threads = 4;
  const int n                    = 25000;
  for (size_t max_size : {1, 2, 10, 50, 100, 1000})
  {
    CircularBuffer<uint32_t> buffer{max_size};
    std::vector<uint32_t> producer_numbers;
    std::vector<uint32_t> consumer_numbers;
    auto producers = std::thread{RunNumberProducers, std::ref(buffer), std::ref(producer_numbers),
                                 num_producer_threads, n};
    std::atomic<bool> exit{false};
    auto consumer = std::thread{RunNumberConsumer, std::ref(buffer), std::ref(exit),
                                std::ref(consumer_numbers)};
    producers.join();
    exit = true;
    consumer.join();
    std::sort(producer_numbers.begin(), producer_numbers.end());
    std::sort(consumer_numbers.begin(), consumer_numbers.end());

    EXPECT_EQ(producer_numbers.size(), consumer_numbers.size());
    EXPECT_EQ(producer_numbers, consumer_numbers);
  }
}
