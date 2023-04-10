#include <prometheus/registry.h>
#include <prometheus/counter.h>
#include <prometheus/push_to_server.h>

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
  using Metric = Counter<uint64_t>;

  using Family = Metric::Family;

  // create a metrics registry
  // @note it's the users responsibility to keep the object alive
  std::shared_ptr<Registry> registry_ptr = std::make_shared<Registry>();

  PushToServer pusher(registry_ptr, std::chrono::seconds(5),
                      std::string("http://127.0.0.1:9091/metrics/job/samples/instance/test") );

  // add a new counter family to the registry (families combine values with the
  // same name, but distinct label dimensions)
  //
  // @note please follow the metric-naming best-practices:
  // https://prometheus.io/docs/practices/naming/
  Family& family { Family::Build(*registry_ptr, "our_metric", "some metric") };

  // add and remember dimensional data, incrementing those is very cheap
  Metric& metric { family.Add({}) };

  for (;; ) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    const int random_value = std::rand();
    metric += random_value % 10;
  }
}
