
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

  // create a metrics registry
  // @note it's the users responsibility to keep the object alive
  auto registry = std::make_shared<Registry>();

  // add a new counter family to the registry (families combine values with the
  // same name, but distinct label dimensions)
  //
  // @note please follow the metric-naming best-practices:
  // https://prometheus.io/docs/practices/naming/
  auto& packet_counter = BuildCounter()
                             .Name("observed_packets_total")
                             .Help("Number of observed packets")
                             .Register(*registry);

  // add and remember dimensional data, incrementing those is very cheap
  auto& tcp_rx_counter = packet_counter.Add({ {"protocol", "tcp"}, {"direction", "rx"} });
  auto& tcp_tx_counter = packet_counter.Add({ {"protocol", "tcp"}, {"direction", "tx"} });
  auto& udp_rx_counter = packet_counter.Add({ {"protocol", "udp"}, {"direction", "rx"} });
  auto& udp_tx_counter = packet_counter.Add({ {"protocol", "udp"}, {"direction", "tx"} });

  // add a counter whose dimensional data is not known at compile time
  // nevertheless dimensional values should only occur in low cardinality:
  // https://prometheus.io/docs/practices/naming/#labels
  auto& http_requests_counter = BuildCounter()
                                    .Name("http_requests_total")
                                    .Help("Number of HTTP requests")
                                    .Register(*registry);

  // ask the exposer to scrape the registry on incoming HTTP requests
  //exposer.RegisterCollectable(registry);

  for ( ;; ) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    const auto random_value = std::rand();

    if (random_value & 1) tcp_rx_counter.Increment();
    if (random_value & 2) tcp_tx_counter.Increment();
    if (random_value & 4) udp_rx_counter.Increment(10);
    if (random_value & 8) udp_tx_counter.Increment(10);

    const std::array<std::string, 4> methods = { "GET", "PUT", "POST", "HEAD" };
    auto method = methods.at(static_cast<std::size_t>(random_value) % methods.size());
    // dynamically calling Family<T>.Add() works but is slow and should be avoided
    http_requests_counter.Add({ {"method", method} }).Increment();

    TextSerializer text_serializer;
    text_serializer.Serialize(std::cout, registry->Collect());

  }
}
