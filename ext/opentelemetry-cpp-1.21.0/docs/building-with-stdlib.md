# Building with Standard C++ Library

Standard Library build flavor works best for statically linking the SDK in a
process (environment where ABI compat is not a requirement), or for
"header-only" implementation of SDK.

Proposed approach cannot be employed for shared libs in environments where ABI
compatibility is required. OpenTelemetry SDK binary compiled with `compiler A +
STL B` will not be ABI -compatible with the main executable compiled with
`compiler C + STL D`.

In addition to standard library, similar approach can be reused to implement the
API surface classes with [Abseil classes](https://abseil.io/) instead of
`nostd`, in products that prefer Abseil.

## Motivation

`nostd` classes in OpenTelemetry API were introduced for the following reasons:

* ABI stability: scenario where different modules are compiled with different
  compiler and incompatible standard library.
* backport of C++17 and above features to C++14 compiler.

The need for custom `nostd` classes is significantly diminished when the SDK is
compiled with C++17 or above compiler. Only `std::span` needs to be backported.

Subsequently, there is no need for `nostd` classes in environment with C++20 or
above compiler, where all system modules are compiled with the same / compatible
compiler. And where the same standard library implementation is being used. This
is the case when SDK is compiled into product itself, with no runtime loadable
components.

Compiling OpenTelemetry SDK from source using standard library classes:
`std::map`, `std::string_view`, `std::span`, `std::variant` instead of `nostd::`
yields better performance and debugability at expense of potentially losing ABI
compatibility. However, the standard library built for Release is guaranteed to
be compatible across Visual Studio 2015, 2017 and 2019 compilers on Windows with
vc14x runtime. Thus, ABI stability requirement introduces an additional
unnecessary runtime complexity and overhead.

While we are committed to support `nostd` classes for those environments where
ABI compatibility is a requirement, we would also like to add flexibility to
build system to optimize the SDK for the case where ABI compatibility is NOT a
requirement.

Implementation of this feature can be subsequently be used as a foundation for
further work - allow bindings to [Abseil](https://github.com/abseil/abseil-cpp)
"backport" implementation of the standard library.

Implementation is completely opaque from SDK code / SDK developer perspective:
mapping / aliasing from `nostd::` classes back to their `std::` counterparts is
done in a corresponding `opentelemetry/nostd/*.h` header. Users still use
`nostd` classes, but the most optimal implementation is picked up depending on
whether users require ABI stability or not.

Example environments that contain the full set of standard classes:

* C++17 or above compiler, with Microsoft GSL backport of `gsl::span`
* C++20 compilers: Visual Studio 2019+, latest LLVM clang, latest gcc

We continue fully supporting both models (`nostd`, `stdlib`) by running CI for
both.

## Implementation

Allow to alias from `nostd::` to `std::` classes for C++17 and above.

Consistent handling of `std::variant` across various OS:

* backport of a few missing variant features, e.g. `std::get` and `std::visit`
  for older version of Mac OS X. Patches that enable proper handling of
  `std::visit` and `std::variant` irrespective of OS version to resolve [this
  quirk](https://stackoverflow.com/questions/52310835/xcode-10-call-to-unavailable-function-stdvisit).

* ability to borrow implementation of C++20 `gsl::span` from [Microsoft
  Guidelines Support Library](https://github.com/microsoft/GSL). This is
  necessary for C++14 and C++17 standard compilers.

## Pros and Cons

### Using trusted/certified Standard Library

Using standard library implementation classes in customer code and on API
surface instead of `nostd` classes. Certain environments would prefer to have
**standard** instead of non-standard classes due to security, performance,
stability and debug'ability considerations. For example, Visual Studio IDE
provides 1st class Debug experience for Standard containers, plus additional
runtime-checks for Debug builds that use Standard containers.

### Minimizing binary size

No need to marshal types from standard to `nostd`, then back to standard library
across ABI boundary - means less code involved and less memcpy. We use Standard
Library classes used elsewhere in the app. We can optimize the event passing by
avoiding `KeyValueIterable` transform (and, thus, unnecessary memcpy) when we
know that the incoming container type matches that one used by SDK.

### Avoiding unnecessary extra memcpy (perf improvements)

No need to transform from 'native' standard library types, e.g. `std::map` via
`KeyValueIterable` means we can bypass that transformation process, if and when
we know that the ABI compatibility is not a requirement in certain environment.
ETA perf improvement is 1.5%-3% better perf since an extra transform, memcpy,
iteration for-each key-value is avoided. Custom OpenTelemetry SDK implementation
may operate on standard container classes rather than doing transform from one
container type to another.

### ABI stability

Obviously this approach does not work in Linux environments, where ABI stability
guarantee must be provided, e.g. for dynamically loadable OpenTelemetry SDK
module and plugins for products such as NGINX, Envoy, etc. Example: a product is
compiled with modern gcc compiler. But OpenTelemetry SDK is compiled with an
older runtime library. In this case the SDK must be compiled with `nostd`.

Note that for most scenarios with modern Windows compilers, STL library is
ABI-safe across Visual Studio 2015, 2017 and 2019 with vc14x runtime.

Quote from [official
documentation](https://docs.microsoft.com/en-us/cpp/porting/binary-compat-2015-2017?view=msvc-160)
:

Visual Studio 2015, 2017, and 2019: the runtime libraries and apps compiled by
any of these versions of the compiler are binary-compatible. It's reflected in
the C++ toolset major number, which is 14 for all three versions. The toolset
version is v140 for Visual Studio 2015, v141 for 2017, and v142 for 2019. Say
you have third-party libraries built by Visual Studio 2015. You can still use
them in an application built by Visual Studio 2017 or 2019. There's no need to
recompile with a matching toolset. The latest version of the Microsoft Visual
C++ Redistributable package (the Redistributable) works for all of them.

Visual Studio provides 1st class debug experience for the standard library.

## Build and Test considerations

### Separate flavors of SDK build

Supported build flavors:

* `nostd` - OpenTelemetry backport of classes for C++14. Not using standard lib.
* `stdlib` - Standard Library.
  Native experience with C++14/C++17/C++20/C++23 compiler.
  Depending on the stdlib level in effect,
  C++ features are used from the standard library,
  completed with `nostd` replacement implementations.
  C++17 and below works but with additional dependencies,
  e.g. either MS-GSL or Abseil for
  `std::span` implementation (`gsl::span` or `absl::Span`).
* `absl`  - TODO: this should allow using Abseil C++ library only (no MS-GSL).

Currently only `nostd` and `stdlib` configurations are implemented in CMake
build. `absl` is reserved for future use. Build systems other than CMake need to
`#define OPENTELEMETRY_STL_VERSION=<version>` to enable the Standard Library classes.

Valid values for `OPENTELEMETRY_STL_VERSION` are `2011`, `2014`, `2017`, `2020` and
`2023`.

### Build matrix

List of compilers that support building with standard library classes:

| Compiler           | Language standard | Notes                                               |
| ------------------ | ----------------- | --------------------------------------------------- |
| Visual Studio 2015 | C++14             | requires `gsl::span` for `std::span` implementation |
| Visual Studio 2017 | C++17             | requires `gsl::span` for `std::span` implementation |
| Visual Studio 2019 | C++20             |                                                     |
| Xcode 11.x         | C++17             | requires `gsl::span` for `std::span` implementation |
| Xcode 12.x         | C++20             |                                                     |
| gcc-7              | C++17             | requires `gsl::span` for `std::span` implementation |
| gcc-9+             | C++20             |                                                     |

If SDK is compiled without standard library classes, then `nostd::variant`
implementation internally uses a private snapshot of [Abseil
Variant](https://github.com/abseil/abseil-cpp/blob/master/absl/types/variant.h)
.

Other modern C++ language features used by OpenTelemetry, e.g.
`std::string_view` and `std::variant` are available in C++17 standard library.

Minor customization needed for Apple LLVM clang in `std::variant` exception
handling due to the fact that the variant exception handler presence depends on
what OS version developer is targeting. This exception on Apple systems is
implemented inside the OS system library. This idiosyncrasy is now handled by
OpenTelemetry API in opaque manner: support `nostd::variant` exception handling
even on older Mac OS X and iOS by providing implementation of it in
OpenTelemetry SDK: if exceptions are enabled, then throw
`nostd::bad_variant_access` exception old OS where `std::bad_variant_access` is
unavailable.

#### Note on `gsl::span` vs `absl::Span`

It is important to note that, while `absl::Span` is similar in design and
purpose to the `std::span` (and existing `gsl::span` reference implementation),
`absl::Span` is not currently guaranteeing to be a drop-in replacement for any
eventual standard. Instead, `absl::Span` aims to have an interface as similar as
possible to `absl::string_view`, without the string-specific functionality.

Thus, OpenTelemetry built with standard library and C++17 compiler prefers
`gsl::span` as it is fully compatible with standard `std::span` available in
C++20 compiler.

### Test setup

CI allows to validate that all OpenTelemetry functionality is working the same
identical way irrespective of what C++ runtime / STL library it is compiled
with, to ensure that both `nostd::` and `std::` classes on API surface are
operating the same way.
