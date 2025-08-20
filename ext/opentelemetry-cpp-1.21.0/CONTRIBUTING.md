# Contributing to opentelemetry-cpp

The OpenTelemetry C/C++ special interest group (SIG) meets regularly. See the
OpenTelemetry [community](https://github.com/open-telemetry/community#cc-sdk)
repo for information on this and other language SIGs.

See the [public meeting
notes](https://docs.google.com/document/d/1i1E4-_y4uJ083lCutKGDhkpi3n4_e774SBLi9hPLocw/edit)
for a summary description of past meetings. To request edit access, join the
meeting or get in touch on
[Slack](https://cloud-native.slack.com/archives/C01N3AT62SJ).

See the [community membership
document](https://github.com/open-telemetry/community/blob/main/community-membership.md)
on how to become a
[**Member**](https://github.com/open-telemetry/community/blob/main/community-membership.md#member),
[**Approver**](https://github.com/open-telemetry/community/blob/main/community-membership.md#approver)
and
[**Maintainer**](https://github.com/open-telemetry/community/blob/main/community-membership.md#maintainer).

## Development

OpenTelemetry C++ uses the [Google naming
convention](https://google.github.io/styleguide/cppguide.html#Naming).

Code is formatted automatically and enforced by CI.

### Build and Run Code Examples

Note: these instructions apply to examples configured with Bazel, see
example-specific documentation for other build automation tools.

Install the latest bazel version by following the steps listed
[here](https://docs.bazel.build/versions/master/install.html).

Select an example of interest from the [examples
folder](https://github.com/open-telemetry/opentelemetry-cpp/tree/main/examples).
Inside each example directory is a `BUILD` file containing instructions for
Bazel. Find the binary name of your example by inspecting the contents of this
`BUILD` file.

Build the example from the root of the opentelemetry-cpp directory using Bazel.
Replace `<binary name>` with the identifier found in the previous step:

```sh
bazel build //examples/<example directory name>:<binary name>
```

Run the resulting executable to see telemetry from the application as it calls
the instrumented library: </li>

```sh
bazel-bin/examples/<example directory name>/<binary name>
```

For instance, building and running the `simple` example can be done as follows:

```sh
bazel build //examples/simple:example_simple
bazel-bin/examples/simple/example_simple
```

### DevContainer Setup for Project

This guide provides instructions on how to set up and use the development
container (`devcontainer`) environment to streamline testing and development
for this project. With the DevContainer, you can work in a consistent environment
configured with all the necessary dependencies and tools.

#### Prerequisites

Before getting started, ensure you have the following installed:

* **Docker**: DevContainers require Docker for containerization.
* **Visual Studio Code (VSCode)** with the **Remote - Containers** extension.

#### Getting Started

* **Open the Project in DevContainer**:

   Open the project in VSCode. When prompted to "Reopen in Container," select
   this option. If you’re not prompted, you can manually open the container by
   selecting **Remote-Containers: Reopen in Container** from the command palette
   (`F1` or `Ctrl+Shift+P`).

* **Container Setup**:

   The DevContainer environment will automatically build based on the configuration
   files provided (e.g., `.devcontainer/devcontainer.json`). This setup will install
   required dependencies, tools, and environment variables needed for the project.

* **Container Customization**:
   See `.devcontainer/README.md` for devcontainer configuration options.

#### Available Commands

Once inside the DevContainer, you can use the following commands to run tests
and CI workflows.

##### 1. Run Tests with Bazelisk

To run tests with Bazelisk using specific compilation options, use:

```bash
bazelisk-linux-amd64 test --copt=-DENABLE_LOGS_PREVIEW
--test_output=errors --cache_test_results=no --copt=-DENABLE_TEST //exporters/otlp/...
```

###### Command Breakdown

* `--copt=-DENABLE_LOGS_PREVIEW`: Enables preview logs.
* `--test_output=errors`: Shows only the errors in the test output.
* `--cache_test_results=no`: Forces Bazel to re-run tests without caching.
* `--copt=-DENABLE_TEST`: Enables testing capabilities for the target code.
* `//exporters/otlp/...`: Specifies the test target path.

##### 2. Run CI Script

You can also run the CI script provided to perform testing with the
following command as an
example:

```bash
bash ci/do_ci.sh cmake.exporter.otprotocol.test
```

This command initiates the CI pipeline, executing tests specifically for the
**cmake.exporter.otprotocol** module.

#### Troubleshooting

If you encounter issues:

* **Rebuild the DevContainer**: From the command palette, run
  **Remote-Containers: Rebuild Container** to reinitialize the environment.
* **Check Bazelisk and CI Script Logs**: Inspect logs for any configuration or
  dependency issues.

#### Additional Notes

* You can adjust compiler options (`--copt`) as needed to test additional flags
  or enable/disable specific features.
* The test results will be displayed in the terminal within the DevContainer for
  easy debugging.

#### Resources

* **Bazelisk Documentation**: [https://github.com/bazelbuild/bazelisk](https://github.com/bazelbuild/bazelisk)
* **VSCode DevContainer Documentation**: [https://code.visualstudio.com/docs/remote/containers](https://code.visualstudio.com/docs/remote/containers)

### Docker Development Image

The `.devcontainer/Dockerfile.dev`
dockerfile can be built directly with the following command.

```sh
 docker build -t opentelemetry-cpp-dev -f ./.devcontainer/Dockerfile.dev .
```

You can customize the image using build arguments
 to match permissions with the host user.

```sh
 docker build -t opentelemetry-cpp-dev \
  --build-arg USER_UID="$(id -u)" \
  --build-arg USER_GID="$(id -g)" \
  -f ./.devcontainer/Dockerfile.dev .

```

Run an interactive bash session binding your host
 opentelemetry-cpp directory to the container's workspace:

```sh
docker run -it -v "$PWD:/workspaces/opentelemetry-cpp" opentelemetry-cpp-dev bash
```

## Pull Requests

### How to Send Pull Requests

Everyone is welcome to contribute code to `opentelemetry-cpp` via GitHub pull
requests (PRs).

To create a new PR, fork the project in GitHub and clone the upstream repo:

```sh
git clone --recursive https://github.com/open-telemetry/opentelemetry-cpp.git
```

Add your fork as a remote:

```sh
git remote add fork https://github.com/YOUR_GITHUB_USERNAME/opentelemetry-cpp.git
```

If you haven't, make sure you are loading the submodules required to build
OpenTelemetry

```sh
git submodule init
git submodule update
```

The source code is automatically formatted using clang-format.

The output can vary between versions, so make sure to install `clang-format`
version `10.0`, and have `clang-format-10` in your execution path,
so that the helper script `tools/format.sh` can find it.

Check out a new branch, make modifications and push the branch to your fork:

```sh
git checkout -b feature
# edit files
tools/format.sh
git commit
git push fork feature
```

If you made changes to the Markdown documents (`*.md` files), install the latest
[`markdownlint-cli`](https://github.com/igorshubovych/markdownlint-cli) and run:

```sh
markdownlint .
```

If you modified shell scripts (`*.sh` files), install `shellcheck` and run:

```sh
shellcheck --severity=error <path to shell script>.sh
```

Open a pull request against the main `opentelemetry-cpp` repo.

To run tests locally, please read the [CI instructions](ci/README.md).

### How to Receive Comments

* If the PR is not ready for review, please put `[WIP]` in the title, tag it as
  `work-in-progress`, or mark it as
  [`draft`](https://github.blog/2019-02-14-introducing-draft-pull-requests/).
* Make sure [CLA](https://identity.linuxfoundation.org/projects/cncf) is signed
  and CI is clear.
* For non-trivial changes, please update the [CHANGELOG](./CHANGELOG.md).

### How to Get PRs Merged

A PR is considered to be **ready to merge** when:

* It has received two approvals with at least one approval from
  [Approver](https://github.com/open-telemetry/community/blob/main/community-membership.md#approver)
  /
  [Maintainer](https://github.com/open-telemetry/community/blob/main/community-membership.md#maintainer)
  (at different company).
* A pull request opened by an Approver / Maintainer can be merged with only one
  approval from  Approver / Maintainer (at different company).
* Major feedback items/points are resolved.
* It has been open for review for at least one working day. This gives people
  reasonable time to review.
* Trivial changes (typo, cosmetic, doc, etc.) don't have to wait for one day.
* Urgent fixes can take exceptions as long as it has been actively communicated.

Any Maintainer can merge the PR once it is **ready to merge**. Maintainer can
make conscious judgement to merge pull requests which have not strictly met
above mentioned requirements.

If a PR has been stuck (e.g. there are lots of debates and people couldn't agree
on each other), the owner should try to get people aligned by:

* Consolidating the perspectives and putting a summary in the PR. It is
  recommended to add a link into the PR description, which points to a comment
  with a summary in the PR conversation
* Stepping back to see if it makes sense to narrow down the scope of the PR or
  split it up.

If none of the above worked and the PR has been stuck for more than 2 weeks, the
owner should bring it to the OpenTelemetry C++ SIG meeting. See
[README.md](README.md#contributing) for the meeting link.

## Design Choices

As with other OpenTelemetry clients, opentelemetry-cpp follows the
[opentelemetry-specification](https://github.com/open-telemetry/opentelemetry-specification).

It's especially valuable to read through the [library
guidelines](https://github.com/open-telemetry/opentelemetry-specification/blob/main/specification/library-guidelines.md).

## Useful Resources

Hi! If you’re looking at this document, these resources will provide you the
knowledge to get started as a newcomer to the OpenTelemetry project. They will
help you understand the OpenTelemetry Project, its components, and specifically
the C++ repository.

### Reading Resources

* Medium
  [article](https://medium.com/opentelemetry/how-to-start-contributing-to-opentelemetry-b23991ad91f4)
  (October 2019) on how to start contributing to the OpenTelemetry project.
* Medium
  [article](https://medium.com/opentelemetry/opentelemetry-beyond-getting-started-5ac43cd0fe26)
  (January 2020) describing the overarching goals and use cases for
  OpenTelemetry.

### Relevant Documentation

* [OpenTelemetry
  Specification](https://github.com/open-telemetry/opentelemetry-specification)
* The OpenTelemetry Specification describes the requirements and expectations
  of for all OpenTelemetry implementations.

* Read through the OpenTelemetry C++ documentation
* The
    [API](https://opentelemetry-cpp.readthedocs.io/en/latest/api/api.html)
    and
    [SDK](https://opentelemetry-cpp.readthedocs.io/en/latest/sdk/sdk.html)
    documentation provides a lot of information on what the classes and their
    functions are used for.

Please contribute! You’re welcome to add more information if you come across any
helpful resources.
