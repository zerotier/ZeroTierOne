
#include <prometheus/simpleapi.h>

// use prometheus namespace
using namespace prometheus::simpleapi;

class MyClass {

  counter_family_t metric_family { "simple_family", "simple family example" };
  counter_metric_t metric1 { metric_family.Add({{"name", "counter1"}}) };
  counter_metric_t metric2 { metric_family.Add({{"name", "counter2"}}) };

  counter_metric_t metric3 { "simple_counter_1", "simple counter 1 without labels example" };
  counter_metric_t metric4 { "simple_counter_2", "simple counter 2 without labels example" };

  benchmark_family_t benchmark_family { "simple_benchmark_family", "simple benchmark family example" };
  benchmark_metric_t benchmark1 { benchmark_family.Add({{"benchmark", "1"}}) };
  benchmark_metric_t benchmark2 { benchmark_family.Add({{"benchmark", "2"}}) };

public:

  MyClass() = default;

  void member_to_do_something() {

    benchmark1.start();
    const int random_value = std::rand();
    benchmark1.stop();

    benchmark2.start();
    if (random_value & 1)  metric1++;
    if (random_value & 2)  metric2++;
    if (random_value & 4)  metric3++;
    if (random_value & 8)  metric4++;
    benchmark2.stop();

  }

};

int main() {

  MyClass myClass;
  benchmark_metric_t benchmark { "simple_benchmark", "simple benchmark example" };

  for (;; ) {

    benchmark.start();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    benchmark.stop();

    myClass.member_to_do_something();

  }

}

