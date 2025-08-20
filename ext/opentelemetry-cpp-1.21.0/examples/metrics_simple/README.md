# Simple Metrics Example

See [INSTALL.md](../../INSTALL.md) for instructions on building and
running the example.

This example initializes the metrics pipeline with 3 different instrument types:

- [Counter](https://github.com/open-telemetry/opentelemetry-specification/blob/main/specification/metrics/api.md#counter)
- [Histogram](https://github.com/open-telemetry/opentelemetry-specification/blob/main/specification/metrics/api.md#histogram)
- [Asynchronous/Observable Counter](https://github.com/open-telemetry/opentelemetry-specification/blob/main/specification/metrics/api.md#asynchronous-counter)

 Here are more detailed steps with explanation. Note that the steps 4, 6, and 8
 are done in Instrumentation library for creating and recording Instruments,
 and rest of the steps are done in application to configure SDK.

 Namespace alias used in below steps

 ```cpp
using namespace metrics_api = opentelemetry::metrics;
using namespace metric_sdk = opentelemetry::sdk::metrics;
using namespace exportermetrics = opentelemetry::exporters;

 ```

1. Initialize an exporter and a reader. In this case, we initialize an OStream
Exporter which will print to stdout by default.
The reader periodically collects metrics from the Aggregation Store and exports them.

    ```cpp
    std::unique_ptr<metric_sdk::PushMetricExporter> exporter{new exportermetrics::OStreamMetricExporter};
    std::unique_ptr<metric_sdk::MetricReader> reader{
        new metric_sdk::PeriodicExportingMetricReader(std::move(exporter), options)};
    ```

2. Initialize a MeterProvider and add the reader.
We will use this to obtain Meter objects in the future.

    ```cpp
    auto provider = std::shared_ptr<metrics_api::MeterProvider>(new metric_sdk::MeterProvider());
    auto p = std::static_pointer_cast<metric_sdk::MeterProvider>(provider);
    p->AddMetricReader(std::move(reader));
    ```

3. Optional: Create a view to map the Counter Instrument to Sum Aggregation.
Add this view to provider. View creation is optional unless we want to add
custom aggregation config, and attribute processor. Metrics SDK  will implicitly
create a missing view with default mapping between Instrument and Aggregation.

    ```cpp
    std::string counter_name = "counter_name";
    std::unique_ptr<metric_sdk::InstrumentSelector> instrument_selector{
        new metric_sdk::InstrumentSelector(metric_sdk::InstrumentType::kCounter, counter_name)};
    std::unique_ptr<metric_sdk::MeterSelector> meter_selector{
        new metric_sdk::MeterSelector(name, version, schema)};
    std::unique_ptr<metric_sdk::View> sum_view{
        new metric_sdk::View{name, "description", metric_sdk::AggregationType::kSum}};
    p->AddView(std::move(instrument_selector), std::move(meter_selector), std::move(sum_view));
    ```

4. Create a Counter instrument from the Meter, and record the measurement.
Every Meter pointer returned by the MeterProvider points to the same Meter.
This means that the Meter will be able to combine metrics captured from
different functions without having to constantly pass the Meter around the library.

    ```cpp
    auto meter = provider->GetMeter(name, "1.2.0");
    auto double_counter = meter->CreateDoubleCounter(counter_name);
    // Create a label set which annotates metric values
    std::map<std::string, std::string> labels = {{"key", "value"}};
    auto labelkv = common::KeyValueIterableView<decltype(labels)>{labels};
    double_counter->Add(val, labelkv);
    ```

5. Optional: Create a view to map the Histogram Instrument to Histogram Aggregation.

    ```cpp
    std::unique_ptr<metric_sdk::InstrumentSelector> histogram_instrument_selector{
        new metric_sdk::InstrumentSelector(metric_sdk::InstrumentType::kHistogram, "histogram_name")};
    std::unique_ptr<metric_sdk::MeterSelector> histogram_meter_selector{
        new metric_sdk::MeterSelector(name, version, schema)};
    std::unique_ptr<metric_sdk::View> histogram_view{
        new metric_sdk::View{name, "description", metric_sdk::AggregationType::kHistogram}};
    p->AddView(std::move(histogram_instrument_selector), std::move(histogram_meter_selector),
        std::move(histogram_view));
    ```

6. Create a Histogram instrument from the Meter, and record the measurement.

    ```cpp
    auto meter = provider->GetMeter(name, "1.2.0");
    auto histogram_counter = meter->CreateDoubleHistogram("histogram_name");
    histogram_counter->Record(val, labelkv);
    ```

7. Optional: Create a view to map the Observable Counter Instrument to Sum Aggregation

    ```cpp
    std::unique_ptr<metric_sdk::InstrumentSelector> observable_instrument_selector{
        new metric_sdk::InstrumentSelector(metric_sdk::InstrumentType::kObservableCounter,
                                         "observable_counter_name")};
    std::unique_ptr<metric_sdk::MeterSelector> observable_meter_selector{
      new metric_sdk::MeterSelector(name, version, schema)};
    std::unique_ptr<metric_sdk::View> observable_sum_view{
      new metric_sdk::View{name, "description", metric_sdk::AggregationType::kSum}};
    p->AddView(std::move(observable_instrument_selector), std::move(observable_meter_selector),
             std::move(observable_sum_view));
    ```

8. Create a Observable Counter Instrument from the Meter, and add a callback.
The callbackwould be used to record the measurement during metrics collection.
Ensure to keep the Instrument object active for the lifetime of collection.

    ```cpp
    auto meter = provider->GetMeter(name, "1.2.0");
    auto counter = meter->CreateDoubleObservableCounter(counter_name);
    counter->AddCallback(MeasurementFetcher::Fetcher, nullptr);
    ```
