# Simple Logs Example

In this example, the application in `main.cc` initializes an
`OStreamLogRecordExporter` instance and registers a logger
provider, as well as initializes a `StdoutSpanExporter` instance and registers a
tracer provider from the [OpenTelemetry C++
SDK](https://github.com/open-telemetry/opentelemetry-cpp).

The application then calls into
[`logs_foo_library`](https://github.com/open-telemetry/opentelemetry-cpp/blob/main/examples/common/logs_foo_library/foo_library.cc)
which has been instrumented using the [OpenTelemetry Logs
API](../../api/include/opentelemetry/logs/logger.h). Resulting logs and traces
are directed to stdout.

See [CONTRIBUTING.md](../../CONTRIBUTING.md) for instructions on building and
running the example.
