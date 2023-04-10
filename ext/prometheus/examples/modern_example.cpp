#include <prometheus/registry.h>
#include <prometheus/counter.h>
#include <prometheus/text_serializer.h>

#include <array>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <iostream>

int main() {

  using namespace prometheus;

  // for clarity, we deduce the required types
  using IntegerCounter  = Counter<uint64_t>;
  using FloatingCounter = Counter<double>;

  using IntegerCounterFamily  = CustomFamily<IntegerCounter>;
  using FloatingCounterFamily = CustomFamily<FloatingCounter>;

  // create a metrics registry
  // @note it's the users responsibility to keep the object alive
  Registry registry;

  // add a new counter family to the registry (families combine values with the
  // same name, but distinct label dimensions)
  //
  // @note please follow the metric-naming best-practices:
  // https://prometheus.io/docs/practices/naming/
  FloatingCounterFamily& packet_counter{ FloatingCounter::Family::Build(registry, "observed_packets_total", "Number of observed packets") };

  // add and remember dimensional data, incrementing those is very cheap
  FloatingCounter& tcp_rx_counter{ packet_counter.Add({ {"protocol", "tcp"}, {"direction", "rx"} }) };
  FloatingCounter& tcp_tx_counter{ packet_counter.Add({ {"protocol", "tcp"}, {"direction", "tx"} }) };
  FloatingCounter& udp_rx_counter{ packet_counter.Add({ {"protocol", "udp"}, {"direction", "rx"} }) };
  FloatingCounter& udp_tx_counter{ packet_counter.Add({ {"protocol", "udp"}, {"direction", "tx"} }) };

  // add a counter whose dimensional data is not known at compile time
  // nevertheless dimensional values should only occur in low cardinality:
  // https://prometheus.io/docs/practices/naming/#labels
  IntegerCounterFamily& http_requests_counter = IntegerCounter::Family::Build(registry, "http_requests_total", "Number of HTTP requests");

  for (;; ) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    const int random_value = std::rand();

    if (random_value & 1)   tcp_rx_counter++;
    if (random_value & 2) ++tcp_tx_counter;
    if (random_value & 4) udp_rx_counter += 0.5;
    if (random_value & 8) udp_tx_counter += 0.7;

    const std::array<std::string, 4> methods = { "GET", "PUT", "POST", "HEAD" };
    const std::string& method = methods.at(static_cast<std::size_t>(random_value) % methods.size());

    // dynamically calling Family<T>.Add() works but is slow and should be avoided
    http_requests_counter.Add({ {"method", method} }) += 10;

    TextSerializer text_serializer;
    text_serializer.Serialize(std::cout, registry.Collect());

  }
}
