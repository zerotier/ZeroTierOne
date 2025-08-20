# Requirements

* The API must have zero dependencies and be header-only.

Pros: allows you to drop it into a source tree (i.e. vendor it) without changing
the build or adding a dependency.

* The SDK should have minimal dependencies.

* Exporters can bring in transport dependencies, e.g. Stackdriver exporter
  depends on gRPC.

* Support both static linking and dynamic loading.

Dynamic loading is required because some vendors want to drop in their own
implementation of the SDK. It also allows low-dependency builds.

This also requires a stable C++ ABI. There is quite a bit of interest in this.

This means STL types can't be used in the interface.

* OpenTelemetry should not _require_ a dedicated thread for background work and
  exporters.
