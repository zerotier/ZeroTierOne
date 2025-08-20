# SDK Factory Design

When an application owner needs to configure the opentelemetry SDK,
this can be done in different ways:

- by assembly of available SDK elements already provided,
- by extension of the SDK to support more functionality.

The sections below investigate each use case,
and the consequences when using shared libraries.

Last, a section discuss the impact of C++ conditional parameters
on the SDK interface, and how this affects shared libraries.

## SDK assembly

Assume the application owner needs to build a trace provider, using the gRPC trace
exporter.

### Case study, direct call to the SDK implementation classes

A possible way to build the exporter is to call, from the application code:

```cpp
opentelemetry::exporter::otlp::OtlpGrpcExporterOptions opts;

auto exporter = std::unique_ptr<opentelemetry::sdk::trace::SpanExporter>(
  new opentelemetry::exporter::otlp::OtlpGrpcExporter(opts));
```

When the application code is built and linked against static
opentelemetry-cpp libraries, the result is a binary that is consistent, and
works.

However, in case of a bug fixed in opentelemetry-cpp itself, the entire
application must be rebuilt (with a fixed version), redistributed,
and reinstalled, to have the bug effectively fixed.

This is highly undesirable.

Instead of static libraries, now consider the same application built and
linked against shared (dynamic) libraries instead.

The desired goal with shared libraries is to allow to:

- fix a bug in opentelemetry-cpp
- deploy a fixed shared library
- keep the application unchanged.

For this to be possible, the ABI exposed by the SDK shared library
must be stable.

Here, class OtlpGrpcExporter is the implementation itself,
and is very likely to change, even for a bug fix,
so it is not a good thing to expose it.

Setting the constraint on the SDK implementation that class
OtlpGrpcExporter will never change is unrealistic,
because a bug fix might require new members (for example, to add a mutex to
fix a race), which will change the ABI (the memory layout is different),
breaking the application code compiled with a different version.

In summary, this line of code alone, in the application space:

  new opentelemetry::exporter::otlp::OtlpGrpcExporter(opts);

prevents in practice any deployment using shared libraries.

### Case study, using Factory and builders from the SDK

The SDK also provide Factory classes, that can be used as follows
from the application code:

```cpp
opentelemetry::exporter::otlp::OtlpGrpcExporterOptions opts;

auto exporter =
  opentelemetry::exporter::otlp::OtlpGrpcExporterFactory::Create(opts);
```

While the application code does not change much,
the amount of SDK internals exposed to the application is reduced
significantly.

OtlpGrpcExporterFactory::Create() actually returns a abstract SpanExporter
object, instead of a concrete OtlpGrpcExporter object.

As a result, the application binary is not even aware of the implementation
class OtlpGrpcExporter.

This property makes it possible to:

- implement changes in the SDK itself
- deploy a new SDK shared library
- keep the application unchanged

### Case study, using Factory and shared gRPC client between OTLP gRPC exporters

To reduce the cost of gRPC, the SDK allow users to share gRPC clients between
OTLP gRPC exporters when these exporters have the same settings. This can be
used as follows from the application code:

```cpp
// Include following headers
#include "opentelemetry/exporters/otlp/otlp_grpc_client_factory.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_exporter_options.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_options.h"

// Create exporters with shared gRPC Client
namespace otlp = opentelemetry::exporter::otlp;

void SetupOtlp() {
  otlp::OtlpGrpcClientOptions client_opts;
  otlp::OtlpGrpcExporterOptions trace_opts;
  otlp::OtlpGrpcLogRecordExporterOptions log_opts;

  // Setting client_opts, trace_opts and log_opts
  // client_opts.endpoint = "localhost:1234";
  // Or we can use client_opts = trace_opts; to copy options from environment of
  // trace OTLP exporter.

  std::shared_ptr<otlp::OtlpGrpcClient> shared_client =
    otlp::OtlpGrpcClientFactory::Create(client_opts);

  // Create exporters
  auto trace_exporter =
    otlp::OtlpGrpcExporterFactory::Create(trace_opts, shared_client);
  auto log_exporter =
    otlp::OtlpGrpcLogRecordExporterFactory::Create(log_opts, shared_client);

  // Other initialization codes ...
}
```

Be careful, create OTLP exporters with an existing `OtlpGrpcClient` will ignore
the options of gRPC when passing the `OtlpGrpcExporterOptions` or other option
object.

## SDK extension

Applications owners who want to extend existing SDK classes are expected
to have a stronger dependency on the SDK internals.

For example, with

```cpp
class MyFancyOtlpGrpcExporter : public OtlpGrpcExporter {...}
```

the build depends on the actual SDK implementation.

Class OtlpGrpcExporter is visible in the SDK public header files,
to allow this pattern.

Using shared libraries in this case is not recommended,
because the SDK implementation is at greater risk of change.

## Conditional parameters

Assume Foo() is part of the SDK, delivered as a shared library.

When an API contains conditional parameters, as in:

```cpp
void Foo(int x = 0, int y = 0);
```

this in reality produces 3 APIs usable by the application code:

```cpp
void Foo();
void Foo(int x);
void Foo(int x, int y);
```

as well as 1 ABI provided in a library:

```cpp
void Foo(int x, int y);
```

The value of the defaults parameters (x = 0) is compiled in the application
code.

Assume that, for a bug fix, the API definition is changed to:

```cpp
void Foo(int x = 0, int y = 1);
```

Deploying a new version of the SDK will have no effect,
because the default value is in the application binary, not the shared
library.

Now, assume a later change needs to add a new parameter:

```cpp
void Foo(int x = 0, int y = 1, int z = 2);
```

Here, clients will call the old Foo(int x, int y) ABI, crashing in the SDK
because the SDK expects 3 parameters, not 2.

Because of this, conditional parameters are to be avoided,
not to be used in the SDK interface.

Note that using conditional parameters in the opentelemetry-cpp API is ok,
because the API is header only (there is no ABI).
