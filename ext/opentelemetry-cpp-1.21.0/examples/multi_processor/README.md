# Multi Processor Trace Example

In this example, the application in `main.cc` initializes and registers a tracer
provider from the [OpenTelemetry
SDK](https://github.com/open-telemetry/opentelemetry-cpp). The `TracerProvider`
is connected to two `processor-exporter` pipelines - for exporting
simultaneously to `StdoutSpanExporter` and `InMemorySpanExporter`. The
application then calls a `foo_library` which has been instrumented using the
[OpenTelemetry
API](https://github.com/open-telemetry/opentelemetry-cpp/tree/main/api).
Resulting telemetry is directed to stdout through the StdoutSpanExporter, and
saved as a variable (vector of spans) through `InMemorySpanExporter`.

See [CONTRIBUTING.md](../../CONTRIBUTING.md) for instructions on building and
running the example.
