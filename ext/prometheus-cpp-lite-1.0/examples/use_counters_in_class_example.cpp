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

// use prometheus namespace
using namespace prometheus;

// for clarity, we deduce the required types
using IntegerCounter  = Counter<uint64_t>;
using FloatingCounter = Counter<double>;

using IntegerCounterFamily  = CustomFamily<IntegerCounter>;
using FloatingCounterFamily = CustomFamily<FloatingCounter>;

// create global registry for use it from our classes
static Registry globalRegistry;

class MyClass {

  IntegerCounterFamily& counterFamily1 { IntegerCounter::Family::Build(globalRegistry,
                                         "counter_family_1", "counter for check integer functionality",
                                         {{"type","integer"}} ) };
  
  IntegerCounter& counter11{ counterFamily1.Add({{"number", "1"}}) };
  IntegerCounter& counter12{ counterFamily1.Add({{"number", "2"}}) };
  IntegerCounter& counter13{ counterFamily1.Add({{"number", "3"}}) };
  
  
  FloatingCounterFamily& counterFamily2 { FloatingCounter::Family::Build(globalRegistry,
                                          "counter_family_2", "counter for check floating functionality",
                                          {{"type","float"}} ) };
  
  FloatingCounter& counter21{ counterFamily2.Add({{"number", "1"}}) };
  FloatingCounter& counter22{ counterFamily2.Add({{"number", "2"}}) };
  FloatingCounter& counter23{ counterFamily2.Add({{"number", "3"}}) };

public:

  MyClass() = default;

  void member_to_do_something() {

    const int random_value = std::rand();

    if (random_value & 1)  counter11++;
    if (random_value & 2)  counter12++;
    if (random_value & 4)  counter13++;
    if (random_value & 8)  counter21++;
    if (random_value & 16) counter22++;
    if (random_value & 32) counter23++;

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

