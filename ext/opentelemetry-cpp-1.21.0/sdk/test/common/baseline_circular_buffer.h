// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace testing
{
/**
 * A locking circular buffer.
 *
 * Used as a baseline in benchmarking.
 */
template <class T>
class BaselineCircularBuffer
{
public:
  explicit BaselineCircularBuffer(size_t max_size) : data_{max_size} {}

  /**
   * Add an element to the circular buffer.
   * @param element the element to add
   * @return true if the element was added successfully
   */
  bool Add(std::unique_ptr<T> &element) noexcept { return this->Add(std::move(element)); }

  bool Add(std::unique_ptr<T> &&element) noexcept
  {
    std::lock_guard<std::mutex> lock_guard{mutex_};
    if (tail_ + data_.size() == head_)
    {
      return false;
    }
    data_[head_ % data_.size()] = std::move(element);
    head_ += 1;
    return true;
  }

  /**
   * Consume elements in the circular buffer.
   * @param f the callback to call for each element
   */
  template <class F>
  void Consume(F f) noexcept
  {
    std::lock_guard<std::mutex> lock_guard{mutex_};
    if (head_ == tail_)
    {
      return;
    }
    auto tail_index = tail_ % data_.size();
    auto head_index = head_ % data_.size();
    if (tail_index < head_index)
    {
      for (auto i = tail_index; i < head_index; ++i)
      {
        f(std::move(data_[i]));
      }
    }
    else
    {
      for (auto i = tail_index; i < data_.size(); ++i)
      {
        f(std::move(data_[i]));
      }
      for (auto i = 0ull; i < head_index; ++i)
      {
        f(std::move(data_[i]));
      }
    }
    tail_ = head_;
  }

private:
  std::mutex mutex_;
  uint64_t head_{0};
  uint64_t tail_{0};
  std::vector<std::unique_ptr<T>> data_;
};
}  // namespace testing
OPENTELEMETRY_END_NAMESPACE
