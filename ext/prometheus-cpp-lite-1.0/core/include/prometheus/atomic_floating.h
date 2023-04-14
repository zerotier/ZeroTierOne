#pragma once

#include <type_traits>
#include <atomic>

namespace prometheus {

  template <typename FloatingType>
  inline std::atomic<FloatingType>& atomic_add_for_floating_types(std::atomic<FloatingType>& value,
    const FloatingType& add) {
    FloatingType desired;
    FloatingType expected = value.load(std::memory_order_relaxed);
    do {
      desired = expected + add;
    } while (!value.compare_exchange_weak(expected, desired));
    return value;
  }


  template <typename FloatingType, class = typename std::enable_if<std::is_floating_point<FloatingType>::value, int>::type>
  inline std::atomic<FloatingType>& operator++(std::atomic<FloatingType>& value) {
    return atomic_add_for_floating_types(value, 1.0);
  }

  template <typename FloatingType, class = typename std::enable_if<std::is_floating_point<FloatingType>::value, int>::type>
  inline std::atomic<FloatingType>& operator+=(std::atomic<FloatingType>& value, const FloatingType& val) {
    return atomic_add_for_floating_types(value, val);
  }

  template <typename FloatingType, class = typename std::enable_if<std::is_floating_point<FloatingType>::value, int>::type>
  inline std::atomic<FloatingType>& operator--(std::atomic<FloatingType>& value) {
    return atomic_add_for_floating_types(value, -1.0);
  }

  template <typename FloatingType, class = typename std::enable_if<std::is_floating_point<FloatingType>::value, int>::type>
  inline std::atomic<FloatingType>& operator-=(std::atomic<FloatingType>& value, const FloatingType& val) {
    return atomic_add_for_floating_types(value, -val);
  }

}
