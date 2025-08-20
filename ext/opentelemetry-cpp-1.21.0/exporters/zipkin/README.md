# Zipkin Exporter for OpenTelemetry C++

## Prerequisite

* [Get Zipkin](https://zipkin.io/pages/quickstart.html)

## Installation

### CMake Install Instructions

Refer to install instructions [INSTALL.md](../../INSTALL.md#building-as-standalone-cmake-project).
Modify step 2 to create `cmake` build configuration for compiling Zipkin as below:

```console
   $ cmake -DWITH_ZIPKIN=ON ..
   -- The C compiler identification is GNU 9.3.0
   -- The CXX compiler identification is GNU 9.3.0
   ...
   -- Configuring done
   -- Generating done
   -- Build files have been written to: /home/<user>/source/opentelemetry-cpp/build
   $
```

### Bazel Install Instructions

TODO

## Usage

Install the exporter on your application and pass the options. `service_name`
is an optional string. If omitted, the exporter will first try to get the
service name from the Resource. If no service name can be detected on the
Resource, a fallback name of "unknown_service" will be used.

```cpp

opentelemetry::exporter::zipkin::ZipkinExporterOptions options;
options.endpoint = "http://localhost:9411/api/v2/spans";
options.service_name = "my_service";

auto exporter = std::unique_ptr<opentelemetry::sdk::trace::SpanExporter>(
    new opentelemetry::exporter::zipkin::ZipkinExporter(options));
auto processor = std::shared_ptr<sdktrace::SpanProcessor>(
    new sdktrace::SimpleSpanProcessor(std::move(exporter)));
auto provider = nostd::shared_ptr<opentelemetry::trace::TracerProvider>(
    new sdktrace::TracerProvider(processor, opentelemetry::sdk::resource::Resource::Create({}),
                std::make_shared<opentelemetry::sdk::trace::AlwaysOnSampler>()));

// Set the global trace provider
opentelemetry::trace::Provider::SetTracerProvider(provider);
```

## Viewing your traces

Please visit the Zipkin UI endpoint <http://localhost:9411>
