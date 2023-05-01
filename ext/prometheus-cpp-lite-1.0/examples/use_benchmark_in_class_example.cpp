#include <prometheus/registry.h>
#include <prometheus/benchmark.h>
#include <prometheus/text_serializer.h>

#include <array>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <iostream>

// use prometheus namespace
using namespace prometheus;

// create global registry for use it from our classes
static Registry globalRegistry;

class MyClass {

  Benchmark::Family& benchmarkFamily { Benchmark::Family::Build(globalRegistry,
                                         "benchmark_family", "family for check benchmark functionality") };

  Benchmark& benchmark1 { benchmarkFamily.Add({{"number", "1"}}) };
  Benchmark& benchmark2 { benchmarkFamily.Add({{"number", "2"}}) };

public:

  MyClass() = default;

  void member_to_do_something() {

    benchmark1.start();
    benchmark2.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    benchmark1.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    benchmark2.stop();

  }

};

int main() {

  MyClass myClass;

  for (;; ) {

    std::this_thread::sleep_for(std::chrono::seconds(1));

    myClass.member_to_do_something();

    TextSerializer text_serializer;
    text_serializer.Serialize(std::cout, globalRegistry.Collect());

  }
}

