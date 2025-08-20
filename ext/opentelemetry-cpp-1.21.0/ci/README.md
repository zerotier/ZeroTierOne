# Building and running tests as a developer

CI tests can be run on docker by invoking the script `./ci/run_docker.sh
./ci/do_ci.sh {TARGET}`or inside
[devcontainer](../CONTRIBUTING.md#devcontainer-setup-for-project)
by invoking the script
`./ci/do_ci.sh {TARGET}` where the targets are:

* `cmake.test`: build cmake targets and run tests.
* `cmake.maintainer.test`: build with cmake and test, in maintainer mode.
* `cmake.legacy.test`: build cmake targets with gcc 4.8 and run tests.
* `cmake.c++20.test`: build cmake targets with the C++20 standard and run tests.
* `cmake.test_example_plugin`: build and test an example OpenTelemetry plugin.
* `cmake.exporter.otprotocol.test`: build and test the otprotocol exporter
* `bazel.test`: build bazel targets and run tests.
* `bazel.legacy.test`: build bazel targets and run tests for the targets meant
  to work with older compilers.
* `bazel.noexcept`: build bazel targets and run tests with exceptions disabled.
* `bazel.nortti`: build bazel targets and run tests with runtime type
  identification disabled.
* `bazel.asan`: build bazel targets and run tests with AddressSanitizer.
* `bazel.tsan`: build bazel targets and run tests with ThreadSanitizer.
* `bazel.valgrind`: build bazel targets and run tests under the valgrind memory
  checker.
* `benchmark`: run all benchmarks.
* `format`: use `tools/format.sh` to enforce text formatting.
* `third_party.tags`: store third_party release tags.
* `code.coverage`: build cmake targets with CXX option `--coverage` and run
  tests.

Additionally, `./ci/run_docker.sh` can be invoked with no arguments to get a
docker shell where tests can be run manually.
