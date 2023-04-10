
#include <prometheus/simpleapi.h>

int main() {

  using namespace prometheus::simpleapi;

  counter_family_t family  { "simple_family", "simple family example" };
  counter_metric_t metric1 { family.Add({{"name", "counter1"}}) };
  counter_metric_t metric2 { family.Add({{"name", "counter2"}}) };

  counter_metric_t metric3 { "simple_counter_1", "simple counter 1 without labels example" };
  counter_metric_t metric4 { "simple_counter_2", "simple counter 2 without labels example" };

  for (;; ) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    const int random_value = std::rand();
    if (random_value & 1) metric1++;
    if (random_value & 2) metric2++;
    if (random_value & 4) metric3++;
    if (random_value & 8) metric4++;
  }

  //return 0;

}