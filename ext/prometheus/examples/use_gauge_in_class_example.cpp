#include <prometheus/registry.h>
#include <prometheus/gauge.h>
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
using IntegerGauge  = Gauge<int64_t>;
using FloatingGauge = Gauge<double>;

using IntegerGaugeFamily  = CustomFamily<IntegerGauge>;
using FloatingGaugeFamily = CustomFamily<FloatingGauge>;

// create global registry for use it from our classes
static Registry globalRegistry;

class MyClass {

  IntegerGaugeFamily& gaugeFamily1  { IntegerGauge::Family::Build(globalRegistry,
                                      "gauge_family_1", "gauge for check integer functionality",
                                      {{"type","integer"}} ) };
  
  IntegerGauge& gauge11{ gaugeFamily1.Add({{"number", "1"}}) };
  IntegerGauge& gauge12{ gaugeFamily1.Add({{"number", "2"}}) };
  IntegerGauge& gauge13{ gaugeFamily1.Add({{"number", "3"}}) };
  
  
  FloatingGaugeFamily& gaugeFamily2 { FloatingGauge::Family::Build(globalRegistry,
                                      "gauge_family_2", "gauge for check floating functionality",
                                      {{"type","float"}} ) };
  
  FloatingGauge& gauge21{ gaugeFamily2.Add({{"number", "1"}}) };
  FloatingGauge& gauge22{ gaugeFamily2.Add({{"number", "2"}}) };
  FloatingGauge& gauge23{ gaugeFamily2.Add({{"number", "3"}}) };

public:

  MyClass() = default;

  void member_to_do_something() {

    const int random_value = std::rand();

    if (random_value &  1      ) gauge11++; else gauge11--;
    if (random_value & (1 << 1)) gauge12++; else gauge12--;
    if (random_value & (1 << 2)) gauge13++; else gauge13--;
    if (random_value & (1 << 3)) gauge21++; else gauge21--;
    if (random_value & (1 << 4)) gauge22++; else gauge22--;
    if (random_value & (1 << 5)) gauge23++; else gauge23--;

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

