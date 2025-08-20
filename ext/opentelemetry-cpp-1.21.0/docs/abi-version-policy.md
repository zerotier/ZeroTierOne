# Application Binary Interface (ABI) version policy

## Goals

### Instrumented applications

Once a given release of opentelemetry-cpp is published as stable,
subsequent releases are expected to provide compatibility,
to avoid disruption.

Compatibility at the source code level (API) is expected,
so that no code change in an application already instrumented is required
to adopt a newer release.

Also, compatibility at the binary level (ABI) is expected,
so that an instrumented application already compiled against
opentelemetry-cpp API headers from an older version, and distributed as a
binary package, can be linked against the SDK library from a newer version.

In other words, once an application is instrumented using the
opentelemetry-cpp API, adopting a newer version:

* should not require source code changes,
* should not require building and distributing a new package.

### opentelemetry-cpp

The opentelemetry-cpp project itself needs to deliver fixes and features on
a continual basis.

Reasons to change an API can be external:

* new APIs added in the specification, for new features
* changes in APIs in the specifications, extending existing features.

Changes can also be caused by internal issues:

* fix technical design issues with an API (incorrect types used, missing
  parameters)

Regardless of the root cause for a change (bug or feature),
changes to the existing APIs are unavoidable.

For the opentelemetry-cpp project to stay healthy,
there must be a way to deliver ABI breaking fixes while preserving
compatibility for users.

This is achieved with ABI versions.

## Concept of ABI version

### Inline namespaces

For the sake of illustration, let's consider a fictitious API such as:

```cpp
namespace opentelemetry
{
  namespace common
  {
    class OtelUtil
    {
      virtual void DoSomething() = 0;
    };
  }
}
```

An application can be instrumented to use it:

```cpp
opentelemetry::common::OtelUtil *p = ...;
p->DoSomething();
```

The problem here is that the binary package produced during the build
contains references to symbols such as:

```cpp
opentelemetry::common::OtelUtil::DoSomething()
```

Because all symbols are in the same `opentelemetry::` namespace, it becomes
impossible to deliver changes (new or different symbols) while at the same
time not deliver changes (to preserve binary compatibility).

This is resolved by the use of inline namespaces in C++, leading to
dedicated ABI versions.

Revised example:

```cpp
namespace opentelemetry
{
  inline namespace v1
  {
    namespace common
    {
      class OtelUtil
      {
        virtual void DoSomething() = 0;
      };
    }
  }
}
```

```cpp
opentelemetry::common::OtelUtil *p = ...;
p->DoSomething();
```

When building, the compiler translates `opentelemetry::common` to
`opentelemetry::v1::common`

The symbols delivered by the opentelemetry library, and used by the
instrumented application, are:

```cpp
opentelemetry::v1::common::OtelUtil::DoSomething()
```

With the help of the `OPENTELEMETRY_BEGIN_NAMESPACE` macro,
the source code can be abstracted to:

```cpp
#define OPENTELEMETRY_ABI_VERSION_NO 1

OPENTELEMETRY_BEGIN_NAMESPACE
{
  namespace common
  {
    class OtelUtil
    {
      virtual void DoSomething() = 0;
    };
  }
}
OPENTELEMETRY_END_NAMESPACE
```

Adding a new API to the OtelUtil class is an ABI breaking change (the C++
virtual table is different).

This change can be delivered, but in a different namespace, which defines
a different (extended) ABI:

```cpp
#define OPENTELEMETRY_ABI_VERSION_NO 2

OPENTELEMETRY_BEGIN_NAMESPACE
{
  namespace common
  {
    class OtelUtil
    {
      virtual void DoSomething() = 0;
      virtual void DoSomethingMore() = 0;
    };
  }
}
OPENTELEMETRY_END_NAMESPACE
```

Compiling this declaration produce the following symbols:

```cpp
opentelemetry::v2::common::OtelUtil::DoSomething()
opentelemetry::v2::common::OtelUtil::DoSomethingMore()
```

### ABI version

By defining an inline namespace per ABI, it is possible to deliver
an ABI v2 implementation independently of ABI v1, keeping v1 unchanged.

Code compiled against the v1 ABI can continue to link against a library
providing v1 symbols, while code compiled against the v2 ABI can link
against a library providing the v2 symbols.

An application, when building, can choose to:

* either build against the v1 interface
* or build against the v2 interface

Using v1 ensures compatibility, at the API and ABI level.

Using v2 allows to benefit from new features.

The choice is made by the application owner when building,
and not by the opentelemetry-cpp library.

This, in essence, describes the technical 'ABI version' building block used
to deliver breaking changes.

How to use this versioning feature, provided by the C++ language with inline
namespaces, is described in the next section.

## Versioning policy

### Version scope

Due to dependencies between classes, having an ABI version per class is not
viable.

For example, class TracerProvider depends on
class Tracer, as it builds tracers.

If class Tracer comes in multiple versions,
then class TracerProvider needs to also comes in multiple versions,
and these versions are correlated:

* v1::TracerProvider creates v1::Tracer instances
* v2::TracerProvider creates v2::Tracer instances

The next logical scope is to consider ABI versions per signal.

This is not viable either, because of interdependencies between signals:

* metrics can use traces in examplars
* eventually, the trace implementation can emit internal metrics
* all signals can use common utility classes

In conclusion, the scope of a version is the entire opentelemetry-cpp project.

### Source code

The number of breaking change that affects an APIs is expected to be very low,
with only specific methods affected directly.

It is very undesirable to:

* create a different git branch per ABI version, in effect forking the
  entire code base
* create a different header file per ABI version, in effect forking
  the entire include headers

just to handle a few modified APIs.

As a result, differences between ABI versions are handled using C
preprocessor macros, in the few places where it is necessary.

For example:

```cpp
OPENTELEMETRY_BEGIN_NAMESPACE
{
  namespace common
  {
    class OtelUtil
    {
      virtual void DoSomething() = 0;

#if OPENTELEMETRY_ABI_VERSION_NO >= 2
      // Added in ABI v2
      virtual void DoSomethingMore() = 0;
#endif

    };
  }
}
OPENTELEMETRY_END_NAMESPACE
```

The same source code, when compiled with different values of
OPENTELEMETRY_ABI_VERSION_NO, produces the proper declarations for ABI v1 or
ABI v2.

This solution reduces the maintenance burden on the source code itself.

## Versions lifecycle

For a given ABI, the lifecycle is:

* EXPERIMENTAL
* STABLE
* DEPRECATED
* REMOVED

In the EXPERIMENTAL status,
any change to the ABI can be implemented, without notice.

There are no compatibility guarantees.
This status is meant as a preview, until the ABI is declared STABLE.

In the STABLE status,
changes to the ABI are forbidden, to guarantee stability.

In the DEPRECATED status, the ABI is still functional and supported,
but instrumented applications are encouraged to migrate to a newer ABI.

In the REMOVED status,
the given ABI is no longer available.

The following sections describe the migration path from one ABI (v1) to the
next (v2).

### STABLE V1

In this state, only one ABI version is available, and it is closed to
changes.

Instrumented applications are built against ABI v1 by default.

opentelemetry-cpp produces a library for ABI v1 by default.

Fixes introducing breaking changes can __not__ be delivered.

This is the current status as of opentelemetry-cpp version 1.11.0

### STABLE V1, EXPERIMENTAL V2

In this state, two ABI versions are available.

CMake offers the following options:

```cmake
option(WITH_ABI_VERSION_1 "ABI version 1" ON)
option(WITH_ABI_VERSION_2 "EXPERIMENTAL: ABI version 2 preview" OFF)
```

Instrumented applications are built against ABI v1 by default,
but may ask to use ABI v2 instead.

opentelemetry-cpp produces a library for stable ABI v1 by default,
but can be configured to provide experimental ABI v2 instead.

Fixes introducing breaking changes can only be delivered in the experimental
ABI v2.

### STABLE V1, STABLE V2, EXPERIMENTAL V3

In this state, two stable ABI versions are available,
the ABI offered by default is the conservative ABI v1.

Fixes introducing breaking changes can no longer be delivered in ABI v2,
because it is declared stable.
An experimental ABI v3 is created.

CMake offers the following options:

```cmake
option(WITH_ABI_VERSION_1 "ABI version 1" ON)
option(WITH_ABI_VERSION_2 "ABI version 2" OFF)
option(WITH_ABI_VERSION_3 "EXPERIMENTAL: ABI version 3 preview" OFF)
```

Instrumented applications are built against stable ABI v1 by default,
but may ask to use the now stable ABI v2 instead.

opentelemetry-cpp produces a library for ABI v1 by default,
but can be configured to provide ABI v2 instead.

Fixes introducing breaking changes can only be delivered in the experimental
ABI v3.

### DEPRECATED V1, STABLE V2, EXPERIMENTAL V3

In this state, two stable ABI versions are available,
the ABI offered by default is the newer ABI v2.

CMake offers the following options:

```cmake
option(WITH_ABI_VERSION_1 "DEPRECATED: ABI version 1" OFF)
option(WITH_ABI_VERSION_2 "ABI version 2" ON)
option(WITH_ABI_VERSION_3 "EXPERIMENTAL: ABI version 3 preview" OFF)
```

Instrumented applications are built against stable ABI v2 by default,
but may ask to use stable ABI v1 instead.

opentelemetry-cpp produces a library for ABI v2 by default,
but can be configured to provide ABI v1 instead.

Fixes introducing breaking changes can only be delivered in the experimental
ABI v3.

### (REMOVED V1), STABLE V2, EXPERIMENTAL V3

In this state, the only stable ABI available is v2.
ABI v1 is no longer supported.

CMake offers the following options:

```cmake
option(WITH_ABI_VERSION_2 "ABI version 2" ON)
option(WITH_ABI_VERSION_3 "EXPERIMENTAL: ABI version 3 preview" OFF)
```

Instrumented applications and the opentelemetry-cpp library are build using
ABI v2.

Now that ABI v1 no longer exists, the code:

```cpp
OPENTELEMETRY_BEGIN_NAMESPACE
{
  namespace common
  {
    class OtelUtil
    {
      virtual void DoSomething() = 0;

#if OPENTELEMETRY_ABI_VERSION_NO >= 2
      // Added in ABI v2
      virtual void DoSomethingMore() = 0;
#endif

    };
  }
}
OPENTELEMETRY_END_NAMESPACE
```

can be simplified to:

```cpp
OPENTELEMETRY_BEGIN_NAMESPACE
{
  namespace common
  {
    class OtelUtil
    {
      virtual void DoSomething() = 0;
      virtual void DoSomethingMore() = 0;
    };
  }
}
OPENTELEMETRY_END_NAMESPACE
```

## Practical Example

### Fixing issue #2033

The problem is to change the MeterProvider::GetMeter() prototype,
to follow specification changes.

See the issue description for all details:

* [Metrics API/SDK]
  Add InstrumentationScope attributes in MeterProvider::GetMeter() #2033

#### API change

In the API, class MeterProvider is changed as follows:

```cpp
class MeterProvider
{
public:
  virtual ~MeterProvider() = default;
  /**
   * Gets or creates a named Meter instance.
   *
   * Optionally a version can be passed to create a named and versioned Meter
   * instance.
   */
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
  virtual nostd::shared_ptr<Meter> GetMeter(nostd::string_view library_name,
                                            nostd::string_view library_version = "",
                                            nostd::string_view schema_url = "",
                                            const common::KeyValueIterable *attributes = nullptr) noexcept = 0;
#else
  virtual nostd::shared_ptr<Meter> GetMeter(nostd::string_view library_name,
                                            nostd::string_view library_version = "",
                                            nostd::string_view schema_url      = "") noexcept = 0;
#endif

  /* ... */
};

```

Note how the ABI changes, while the API stays compatible, requiring no code
change in the caller when providing up to 3 parameters.

#### SDK change

In the SDK class declaration, implement the expected API.

```cpp
class MeterProvider final : public opentelemetry::metrics::MeterProvider
{
public:

  /* ... */

#if OPENTELEMETRY_ABI_VERSION_NO >= 2
  nostd::shared_ptr<opentelemetry::metrics::Meter> GetMeter(
      nostd::string_view name,
      nostd::string_view version                                = "",
      nostd::string_view schema_url                             = "",
      const opentelemetry::common::KeyValueIterable *attributes = nullptr) noexcept override;
#else
  nostd::shared_ptr<opentelemetry::metrics::Meter> GetMeter(
      nostd::string_view name,
      nostd::string_view version    = "",
      nostd::string_view schema_url = "") noexcept override;
#endif

  /* ... */
};
```

In the SDK implementation:

* either get the new parameters from the extended ABI v2 method
* or provide default values for the old ABI v1 method

```cpp
nostd::shared_ptr<metrics_api::Meter> MeterProvider::GetMeter(
    nostd::string_view name,
    nostd::string_view version,
    nostd::string_view schema_url
#if OPENTELEMETRY_ABI_VERSION_NO >= 2
    ,
    const opentelemetry::common::KeyValueIterable *attributes
#endif
    ) noexcept
{
#if OPENTELEMETRY_ABI_VERSION_NO < 2
  const opentelemetry::common::KeyValueIterable *attributes = nullptr;
#endif

  /* common implementation, use attributes */
}
```
