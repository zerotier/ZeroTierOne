# Library Distribution

This document describes the considerations and recommendations for using the
OpenTelemetry-cpp library.

## Definitions

### Binary vs Source Distribution

The OpenTelemetry-cpp libraries (API and SDK) and any library that depends on
them are expected to be distributed as either "source" or "binary" artifacts.
This document defines these distribution models as:

* **Source** - The source code is shipped as-is and can be built as desired.
* **Binary** - The library and/or its dependants may be distributed as pre-built
  binaries which can be built using different toolchains or compilers.

This document will focus on the specific challenges of the "binary" distribution
model.

### Library Binding Model

The library and its dependants can be distributed and bound (loaded) in multiple
ways which are defined as follows:

* **Static Linkage** - The library is distributed as a compiled artifact (`.a`
  extension on Unix-like systems) which is linked and bound at compile-time.
* **Dynamic Linkage with Early Binding** - The library is distributed as a
  compiled artifact (`.so` on Unix-like systems), and its binding is defined at
  compile-time. It is loaded by the dynamic linker when the application/library
  depending on it is started/loaded.
* **Dynamic Linkage with Late Binding** - The library is distributed as a
  compiled artifact (`.so` on Unix-like systems), only its interface is known at
  compile-time. It is loaded dynamically (using `dlopen` on POSIX).

_NOTE: A library may itself link in other libraries based on any of the above._

### Components

The following components will be considered for analysing the impact of
different binding models and the recommendations.

* **OpenTelemetry Libraries** - The libraries produced from this project, the
  API library will contain a minimal implementation including some compiled
  globals/singletons such as `TracerProvider`.
* **OpenTelemetry Instrumented Libraries** - A library that depends on the
  OpenTelemetry API library to generate telemetry.
* **OpenTelemetry Implementations or Exporters** - A specific implementation of
  the OpenTelemetry API or the `exporters` may be provided externally (e.g.
  distributed by a vendor).
* **Binary Executable** - The compiled binary that has `main()`, this also
  includes interpreters such as Python which will be considered. This binary may
  itself instrument using OpenTelemetry or register a specific implementation.

For more information on language-agnostic library, please see: [OpenTelemetry
Specification Library
Guidelines](https://github.com/open-telemetry/opentelemetry-specification/blob/main/specification/library-guidelines.md).

### ABI Compatibility

Some guarantees regarding binary compatibility will be necessary given the
binary distribution model supported by this project. The policies and approach
regarding this will be documented in [ABI Policy](abi-policy.md).

## Recommendations

### Link OpenTelemetry plugins for portability

If you're a vendor and you wish to distribute an OpenTelemetry plugin (either a
full implementation of the API or an exporter), you need to take precautions
when linking your plugin to ensure it's portable. Here are some steps you should
follow:

* Ensure you compile to target portable architectures (e.g. x86-64).
* Statically link most dependencies. You should statically both external
  dependencies and the standard C++ library. The exceptions are the standard C
  library and other low-level system libraries that need to be dynamically
  linked.
* Use an export map to avoid unwanted symbol resolution. When statically linking
  dependencies in a dynamic library, care should be taken to make sure that
  symbol resolution for dependencies doesn't conflict with that of the app or
  other dynamic libraries. See this [StackOverflow
  post](https://stackoverflow.com/q/47841812/4447365) for more information.
* Re-map symbols from the standard C library to portable versions. If you want
  to plugin to work on systems with different versions of the standard C
  library, you need to link to portable symbols. See this [StackOverflow
  answer](https://stackoverflow.com/a/20065096/4447365) for how to do this.

## Example Scenarios

### Statically compiled binary executable

Binary executable can be distributed with static linkage ,for all libraries
known at compile time. In this case the OpenTelemetry API Library can be linked
in statically, guaranteeing that only a single symbol is exported for
singletons.

### Dynamically linked binary executable

An application can link to the OpenTelemetry API but dynamically load an
implementation at runtime. Under this mode, an application can work with any
vendor's implementation by using it as a plugin.

For example, a C++ database server might add support for the OpenTelemetry API
and exposes configuration options that let a user point to a vendor's plugin and
load it with a JSON config. (With OpenTracing, Ceph explored a deployment
scenario similar to this. See this
[link](https://www.spinics.net/lists/ceph-devel/msg41007.html))

### Non OpenTelemetry aware application with OpenTelemetry capability library

An application itself may not be OpenTelemetry-aware, but it can support
OpenTelemetry via extensions.

Examples:

* The core NGINX application has no knowledge of tracing. However, through
  NGINX's dynamic module capability, tracing can be supported as a plugin. For
  instance, the [nginx-opentracing
  module](https://github.com/opentracing-contrib/nginx-opentracing) provides
  this type of extension and is used by projects such as Kubernetes [ingress
  controller](https://kubernetes.github.io/ingress-nginx/user-guide/third-party-addons/opentracing/).
* The CPython binary also has no knowledge of OpenTelemetry, but C++ Python
  extension modules can be instrumented for OpenTelemetry.

Additionally, when multiple OpenTelemetry-aware extensions co-exist in the same
application, the extensions should be able to coordinate and share context
through the OpenTelemetry API's singletons.
