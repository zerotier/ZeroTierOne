# Zipkin Exporter Example

This is an example of how to use the Zipkin exporter.

The application in `main.cc` initializes an `ZipkinExporter` instance and uses
it to register a tracer provider from the [OpenTelemetry
SDK](https://github.com/open-telemetry/opentelemetry-cpp). The application then
calls a `foo_library` which has been instrumented using the [OpenTelemetry
API](https://github.com/open-telemetry/opentelemetry-cpp/tree/main/api).

Resulting spans are exported to the Zipkin server using the Zipkin exporter.

Note that the Zipkin exporter connects to the server at `localhost:9411` by
default.

## Running Zipkin server locally

The quick way to run the Zipkin server is using Docker container :

``console

$ docker run -d -p 9411:9411 openzipkin/zipkin

``

## Running Zipkin example

Build this example using instructions in [INSTALL.md](../../INSTALL.md).

## Viewing the traces

Please visit the Zipkin UI endpoint `http://localhost:9411`
