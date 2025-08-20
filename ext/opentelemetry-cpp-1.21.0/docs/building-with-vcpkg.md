# Building OpenTelemetry C++ SDK with vcpkg

vcpkg is a Microsoft cross-platform open source C++ package manager. Onboarding
instructions for Windows, Linux and Mac OS X [available
here](https://docs.microsoft.com/en-us/cpp/build/vcpkg). This document assumes
that the customer build system is already configured to use vcpkg. OpenTelemetry
C++ SDK maintainers provide a build recipe, `opentelemetry` port or CONTROL file
for vcpkg. Mainline vcpkg repo is refreshed to point to latest stable open
source release of OpenTelemetry C++ SDK.

## Installing opentelemetry package

The following command can be used to install the public open source release:

```console
vcpkg install opentelemetry-cpp
```

That's it! The package should be compiled for the current OS.

See instructions below to build the SDK with additional Microsoft-proprietary
modules.

## Testing custom dev OpenTelemetry build on Windows

`cmd.exe` command line prompt commands:

```console
git clone --recurse-submodules https://github.com/open-telemetry/opentelemetry-cpp
cd opentelemetry-cpp
vcpkg install --head --overlay-ports=%CD%\tools\ports opentelemetry
```

## Testing custom dev OpenTelemetry build on POSIX (Linux and Mac)

Shell commands:

```console
git clone --recurse-submodules https://github.com/open-telemetry/opentelemetry-cpp
cd opentelemetry-cpp
vcpkg install --head --overlay-ports=`pwd`/tools/ports opentelemetry
```

## Using response files to specify dependencies

vcpkg allows to consolidate parameters passed to vcpkg in a response file. All
3rd party dependencies needed for OpenTelemetry SDK can be described and
installed via response file.

Example for Mac:

```console
vcpkg install @tools/ports/opentelemetry/response_file_mac.txt
```

Example for Linux:

```console
vcpkg install @tools/ports/opentelemetry/response_file_linux.txt
```

vcpkg build log files are created in
`${VCPKG_INSTALL_DIR}/buildtrees/opentelemetry/build-[err|out].log` . Review the
logs in case if you encounter package installation failures.

## Using triplets

In order to enable custom build flags - vcpkg triplets and custom environment
variables may be used. Please see [triplets instruction
here](https://learn.microsoft.com/en-us/vcpkg/users/triplets). Response file for
a custom build, e.g. `response_file_linux_PRODUCTNAME.txt` may specify a custom
triplet. For example, custom triplet controls if the library is built as static
or dynamic. Default triplets may also be overridden with [overlay
triplets](https://learn.microsoft.com/en-us/vcpkg/users/examples/overlay-triplets-linux-dynamic).
Overlay triplets specific to various products must be maintained by product
teams. Product teams may optionally decide to integrate their triplets in the
mainline OpenTelemetry C++ SDK repo as-needed.

## Using Feature Packages

To install opentelemetry built with standard library API surface classes:

```console
vcpkg install opentelemetry[stdlib]
```

To install opentelemetry built with Abseil API surface classes:

```console
vcpkg install opentelemetry[abseil]
```

## Build with vcpkg dependencies

`CMakeLists.txt` in top-level directory lists the following package
dependencies:

- `Protobuf` - required for OTLP exporter. Not required otherwise.
- `GTest` - required when building with tests enabled.
- `Benchmark` - required when building with tests enabled.
- `ms-gsl` - required for `gsl::span` when building with Standard Library with
  C++14 or C++17 compiler.
- `nlohmann-json` - required when building with zPages module.
- `prometheus-cpp` - required for Prometheus exporter.
- `gRPC` and `Protobuf` - required for OTLP exporter

It is possible to adjust the build system to use either vcpkg-installed
dependencies or OS-provided dependencies, e.g. `brew` or `deb` packages.
To install the dependencies through vcpkg,

- Set the VCPKG_ROOT env variable to the vcpkg install directory, or
- Set the CMake variable CMAKE_TOOLCHAIN_FILE to vcpkg toolchain file `vcpkg.cmake`.

With either of these settings, the appropriate vcpkg folders get added to the cmake
search path, and makes the required libraries to be found through `find_package()`.

The opentelemetry-cpp repo also brings the vcpkg package under `tools` directory.
This would be used during Windows builds to install the missing dependencies ONLY
if the external vcpkg toolchain is not configured.
