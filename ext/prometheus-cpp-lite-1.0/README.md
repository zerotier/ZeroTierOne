# C++ Header-only Prometheus client library

It is a tool for quickly adding metrics (and profiling) functionality to C++ projects.

## Advantages:

1. Written in pure C++,
2. Header-only,
2. Cross-platform,
3. Compiles with C ++ 11, C ++ 14, C ++ 17 standards,
4. Has no third-party dependencies,
5. Several APIs for use in your projects,
6. Saving metrics to a file (and then works with node_exporter) or sending via http (uses built-in header-only http-client-lite library),
7. Possiblity to use different types for storing metrics data (default is uint32_t, but you can use double or uint64_t types if you want),
8. Five types of metrics are supported: counter, gauge, summary, histogram and benchmark,
10. Has detailed examples of use (see examples folder)

## How it differs from the [jupp0r/prometheus-cpp](https://github.com/jupp0r/prometheus-cpp) project:
1. I need a simple header only wariant library without dependencies to write metrics to a .prom file,
2. I need the fastest possible work using integer values of counters (original project use only floating pointer values),
3. The origianl project have problems on compilers that do not know how to do LTO optimization,
4. I did not like the python style of the original project and the large amount of extra code in it and I wanted to make it lighter and more c++ classic.

## How to use it:
The library has two API:
1. Complex API for those who want to control everything,
2. Simple API for those who want to quickly add metrics to their C ++ (and it is actually just a wrapper around the complex API).


### Let's start with a simple API because it's simple:

To add it to your C++ project add these lines to your CMakeLists.txt file:
```
add_subdirectory("prometheus-cpp-lite/core")
add_subdirectory("prometheus-cpp-lite/3rdpatry/http-client-lite")
add_subdirectory("prometheus-cpp-lite/simpleapi")
target_link_libraries(your_target prometheus-cpp-simpleapi)
```

The simplest way to create a metric would be like this:
``` c++
prometheus::simpleapi::METRIC_metric_t metric1 { "metric1", "first simple metric without any tag" };
prometheus::simpleapi::METRIC_metric_t metric2 { "metric2", "second simple metric without any tag" };
```
where ```METRIC``` can be ```counter```, ```gauge```, ```summary```, ```histogram``` or ```benchmark```.

If you want to access an existing metric again elsewhere in the code, you can do this:
``` c++
prometheus::simpleapi::METRIC_metric_t metric2_yet_another_link { "metric2", "" };
```
this works because when adding a metric, it checks whether there is already a metric with the same name and, if there is one, a link to it is returned.

You can create a family of metrics (metrics with tags) as follows:
``` c++
prometheus::simpleapi::METRIC_family_t family  { "metric_family", "metric family" };
prometheus::simpleapi::METRIC_metric_t metric1 { family.Add({{"name", "metric1"}}) };
prometheus::simpleapi::METRIC_metric_t metric2 { family.Add({{"name", "metric2"}}) };
```
where METRIC can be ```counter```, ```gauge```, ```summary```, ```histogram``` or ```benchmark```.

Next, you can do the following things with metrics:
``` c++
metric++; // for increment it (only for counter and gauge metrics)
metric += value; // for add value to metric (only for gauge metric)
metric -= value; // for sub value from metric (only for gauge metric) 
metric = value;  // save current value (only gauge metrics)
metric.start();  // start calculate time (only for benchmark metric)
metric.stop();   // stop calculate time (only for benchmark metric)
```

You can change the settings of save (or send) metrics data as follows:
``` c++
prometheus::simpleapi::saver.set_delay(period_in_seconds); // change the period of saving (or sending) metrics data in seconds (5 seconds by default)
prometheus::simpleapi::saver.set_out_file(filename);       // change the name of the output file (metrics.prom by default)
prometheus::simpleapi::saver.set_server_url(url);          // change the name of prometheus server (unset by default)
```

### Simple API complex example 1 (examples/simpleapi_example.cpp):

``` c++
#include <prometheus/simpleapi.h>

void main() {

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

}
```

Output in "metrics.prom" file (by default) will be:

```
# HELP simple_family simple family example
# TYPE simple_family counter
simple_family{name="counter1"} 10
simple_family{name="counter2"} 9
# HELP simple_counter_1 simple counter 1 without labels example
# TYPE simple_counter_1 counter
simple_counter_1 6
# HELP simple_counter_2 simple counter 2 without labels example
# TYPE simple_counter_2 counter
simple_counter_2 8
```

### Simple API complex example 2 (examples/simpleapi_use_in_class_example.cpp):

``` c++
#include <prometheus/simpleapi.h>

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

void main() {

  MyClass myClass;
  benchmark_metric_t benchmark { "simple_benchmark", "simple benchmark example" };

  for (;; ) {

    benchmark.start();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    benchmark.stop();

    myClass.member_to_do_something();

  }

}
```

Output in "metrics.prom" file (by default) will be:

```
# HELP simple_family simple family example
# TYPE simple_family counter
simple_family{name="counter1"} 3
simple_family{name="counter2"} 2
# HELP simple_counter_1 simple counter 1 without labels example
# TYPE simple_counter_1 counter
simple_counter_1 3
# HELP simple_counter_2 simple counter 2 without labels example
# TYPE simple_counter_2 counter
simple_counter_2 3
# HELP simple_benchmark_family simple benchmark family example
# TYPE simple_benchmark_family counter
simple_benchmark_family{benchmark="1"} 0.0001088
simple_benchmark_family{benchmark="2"} 1.48e-05
# HELP simple_benchmark simple benchmark example
# TYPE simple_benchmark counter
simple_benchmark 6.0503248
```

