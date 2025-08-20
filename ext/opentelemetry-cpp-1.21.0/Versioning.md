# Versioning

This document describes the versioning policy for this repository.

## Goals

### API and SDK Compatibility

Once the API for a given signal (spans, logs, metrics, baggage) has been
officially released, that API module will function with any SDK that has the
same MAJOR version and equal or greater MINOR or PATCH version. For example,
application compiled with API v1.1 is compatible with SDK v1.1.2, v1.2.0, etc.

For example, libraries that are instrumented with `opentelemetry 1.0.1` will
function in applications using `opentelemetry 1.11.33` or `opentelemetry 1.3.4`,
buy may not work in applications using `opentelemetry 2.0.0`.

### ABI Stability

Refer to the [ABI Policy](./docs/abi-policy.md) for more details. To summarise:

* The API is header only, and uses ABI compliant interfaces. However, ABI
  stability is not guaranteed for SDK.
* In case of ABI breaking changes, a new `inline namespace` version will be
  introduced, and the existing linked applications can continue using the older
  version unless they relink with newer version.

## Release Policy

* Release versions will follow [SemVer 2.0](https://semver.org/).
* Only a single source package containing the API, SDK, and exporters which are
  required by the specification would be released. All these components are
  always versioned and released together. For example, any changes in one of the
  exporter would result in version update of the entire source package even
  though there is no changes in API, SDK and other exporters.
* Experimental releases: New (unstable) telemetry signals and features will be
  introduced behind feature flag protected by a preprocessor macro.

  ```cpp
  #ifdef FEATURE_FLAG
    <metrics api/sdk definitions>
  #endif
  ```

  As we deliver the package in source form, and the user is responsible to build
  it for their platform, the user must be aware of these feature flags
  (documented in the [CHANGELOG.md](CHANGELOG.md) file). The user must enable
  them explicitly through their build system (CMake, Bazel or others) to use any
  preview features.

  The guidelines in creating feature flag would be:

  * Naming:
    * `ENABLE_<SIGNAL>_PREVIEW` : For experimental release of signal api/sdks
      eg, `METRICS_PREVIEW`, `LOGS_PREVIEW`,
    * `ENABLE_<SIGNAL>_<FEATURE_NAME>_PREVIEW` : For experimental release for
      any feature within stable signal. For example, `TRACING_JAEGER_PREVIEW` to
      release the experimental Jaeger exporter for tracing.
  * Cleanup: It is good practice to keep feature-flags as shortlived as
    possible. And, also important to keep the number of them low. They should be
    used such that it is easy to remove/cleanup them once the experimental
    feature is stable.

* New signals will be stabilized via a **minor version bump**, and are not
  allowed to break existing stable interfaces. Feature flags will be removed
  once we have a stable implementation for the signal.

* As an exception, small experimental features in otherwise stable
  signals/components mayn't necessarily be released under feature flag. These
  would be flagged as experimental by adding a `NOTE` in it's header file -
  either at the beginning of file, or as the comment for the experimental API
  methods. Also, if the complete header is experimental, it would be prefixed as
  `experimental_`. As an example, the semantic conventions for trace signal is
  experimental at the time of the writing and is within
  `experimental_semantic_conventions.h`

* Code under the "*::detail" namespace implements internal details, and is NOT
  part of public interface. Also, any API not documented in the [public
  documentation](https://opentelemetry-cpp.readthedocs.io/en/latest/) is NOT
  part of the public interface.

* GitHub releases will be made for all released versions.

## Example Versioning Lifecycle

Purely for illustration purposes, not intended to represent actual releases:

* v0.0.1 release:
  * Contains experimental API and SDK of trace (without feature flag)
  * No API and SDK of logging and metrics available
* v1.0.0-rc1 release:
  * Pre-release, no API/ABI guarantees, but more stable than alpha/beta.
  * Contains pre-release API and SDK of trace, baggage and resource
  * experimental metrics and logging API/SDK behind feature flag
* v1.0.0: ( with traces )
  * Contains stable API and SDK of trace, baggage and resource
  * experimental metrics and logging API/SDK behind feature flag
* v1.5.0 release (with metrics)
  * Contains stable API and SDK of metrics, trace, baggage, resource.
  * experimental logging API/SDK behind feature flag
* v1.10.0 release (with logging)
  * Contains stable API and SDK of logging, metrics, trace, baggage, resource.

### Before moving to version 1.0.0

* Major version zero (0.y.z) is for initial development. Anything MAY change at
  any time. The public API SHOULD NOT be considered stable.
