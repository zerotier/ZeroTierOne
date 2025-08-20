# Application Binary Interface (ABI) Policy

To support scenarios where OpenTelemetry implementations are deployed as binary
plugins, certain restrictions are imposed on portions of the OpenTelemetry API.

Many parts of the C++ standard library don't have well-defined ABIs. To ensure
that a plugin compiled against one version of the C++ standard library can work
with an application or library compiled against a different version of the C++
standard library, we limit a portion of the OpenTelemetry API to use ABI stable
types.

In the areas of the API where we need ABI stability, we use C++ as an extended
C. We assume that standard language features like inheritance follow a
consistent ABI (vtable layouts, for example, are specified by the [Itanium
ABI](https://itanium-cxx-abi.github.io/cxx-abi/abi.html#vtable)) and can be used
across compilers, but don't rely on the ABI stability of the C++ standard
library classes.

ABI stability is not provided by the interfaces the SDK provides as
implementation hooks to vendor implementors, like exporters, processors,
aggregators and propagators.

These are some of the rules for where ABI stability is required.

Note: This assumes export maps are used to properly control symbol resolution.

1. Abstract classes in the OpenTelemetry API must use ABI stable types in their
   virtual function signatures. Example

```cpp
class Tracer {
public:
  ...

  // Bad: std::string doesn't have a well-defined ABI.
  virtual void f(const std::string& s);

  // OK: We provide a ABI stable string_view type.
  virtual void f(nostd::string_view s);

  // OK: g is non-virtual function.
  void g(const std::vector<int>& v);
  ...
};
```

The ABI restrictions don't apply to concrete classes.

```cpp
class TracerImpl final : public Tracer {
public:
  // ...
private:
  std::string s; // OK
};
```

Singletons defined by the OpenTelemetry API must use ABI stable types since they
could potentially be shared across multiple instrumented dynamic shared objects
(DSOs) compiled against different versions of the C++ standard library.
