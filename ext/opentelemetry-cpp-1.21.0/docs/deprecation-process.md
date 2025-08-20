# Deprecation process

## Motivation

Deprecation of code is a difficult topic,
which can become a point of friction.

On one hand, maintainers have to maintain the code base
for many platforms, many compilers, while making bug fixes
and adding new features over time.

To do so, sometime code cleanup, minor refactoring, or removal of
previous engineering debt is required to maintain a healthy code base,
to allow for further improvements.

In short, maintainers may want to, or may have to,
remove some apis during the lifetime of a project.

On the other hand, users who have a working application built using
opentelemetry-cpp, for a given platform, a given compiler, and a given
configuration, prefer to have stability and as few changes as possible.

Doing code changes in the application imply a new build / test / deploy
cycle, which induce costs, and risks.

In short, users typically do now want changes, if the code works for them.

The following extreme behaviors are what needs to be avoided for a project
to stay healthy:

* constant changes in the code base, done without notice,
  introducing incompatibilities, causing adoption of each new release to
  be costly with code rewrite. While this allows maintainers to make
  more releases, it ultimately slows down adoption from users.

* impossibility to make any changes in the code base, or having to support
  many different flavors of the same api for compatibility reasons,
  which introduces cost, technical risks, and code bloat, ultimately slowing
  innovation down.

The whole idea of a deprecation and removal process is to provide some
mitigation, that will satisfy both maintainers and users, while allowing
both to move forward with their own areas or responsibilities.

Also, note that some areas of the code are considered stable, while others
are maturing (feature preview), or experimental and in active development.

The deprecation and removal process is primarily for stable parts of the
code. Code in feature preview and experimental is expected to change in a
faster pace, possibly with short of no notice in case of experimental code.

This process attempts to provide:

* visibility on the delivery pipeline itself, so that deprecations are
  visible and documented in advance,
* sufficient time between the deprecation and the removal of an API,
  so that users can adapt the application at a time of their choice,
  planned, instead of having to do it in emergency,
* tooling to verify if a given code base is exposed to deprecated code or
  not, to help assess how much effort is required to stay up to date,
* migration and mitigations, to document not only that some api is going
  away, but to also provide instructions to adjust the code with an
  alternate solution.

## Artifacts

### Issues

Deprecation issues should be tagged with the "Deprecated" label,
for example like
[issue #1892](https://github.com/open-telemetry/opentelemetry-cpp/issues/1892).

Removal issues should be tagged with the "Removal" label,
for example like
[issue #1938](https://github.com/open-telemetry/opentelemetry-cpp/issues/1938).

Discoverability of ongoing deprecations and removals is the key here,
to avoid surprises.

### The DEPRECATED document

At any given time, file `DEPRECATED` lists all the ongoing deprecations,
no matter how long ago these were announced.

A user using release N should find, when reading `DEPRECATED`,
if a given api was deprecated in release N-5 or N-10, without having to go back to
release notes from past versions.

When a removal is finally implemented, the `DEPRECATED` file is expunged,
so it contains only the currently active deprecations.

Main sections in that document are organized as follows:

* [Platforms]
* [Compilers]
* [Third party dependencies]
* [Build tools]
  * This is for `CMake`, `Bazel`, `Doxygen`, ...
  * Changes of tooling go here.
* [Build scripts]
  * This is for `CMakeLists.txt`, `BUILD`, `Doxyfile`, ...
  * Changes of build options go here.
* [opentelemetry-cpp API]
* [opentelemetry-cpp SDK]
* [opentelemetry-cpp Exporter]
* [Documentation]

Please keep main sections as is, with a content of "N/A", instead of
removing sections with no current deprecations.

## Workflow

### Decide deprecation

The first step is to discuss, typically in SIG C++ meeting,
the need to deprecate a given feature or api.

Upon agreement,
file an issue in GitHub so it becomes visible to all parties
following closely development.

Use [DEPRECATION] in the title, and tag with label "Deprecation".

For highly visible features,
consider other ways to improve awareness, such as:

* pin the deprecation issue in the GitHub UI,
* discussions in the slack channels
* discussions with other teams in open-telemetry
* blog posts

### Prepare deprecation

Implement the deprecation issue with a PR.

Deprecation consist of two parts, communication and tooling.

For the communication, the PR should add an entry in file `DEPRECATED`,
detailing the following sections:

* Announcement
  * set `Version:` to TO-BE-RELEASED-VERSION
  * set `Date:` to TO-BE-RELEASED-DATE
  * set `PR:` to the deprecation PR
  * If applicable, add references to any other relevant document
    (slack discussion, blog, meeting minutes)
* Motivation
  * add justifications for the need to deprecate.
  * this step is critical to get support from users that will be affected,
    instead of getting push back.
* Scope:
  * details which part are affected.
  * when applicable, give the exact name of CMake options,
    C++ header files, classes, functions, etc,
    so that users can easily search in their code base
    for references to the deprecated items,
    and assess the exposure to deprecated code.
* Mitigation:
  * indicate any tooling available to identify the
    deprecated code still in use,
  * indicate technical mitigations if known,
  * name replacement apis, libraries, compiling options, etc,
    and describe how to adjust the user code in general,
    to remove any dependencies on deprecated code.
* Planned removal:
  * Indicate an estimated date and/or version, if known,
    by which the deprecated code will be removed,
  * this allows users to plan accordingly, to minimize disruption.

For the tooling, the PR should implement any code change necessary
to help users transition to a clean code base,
where all dependencies to deprecated code are removed.

This imply these dependencies can be identified,
in a reliable way.

The goal is to, at the same time:

* not break existing functionality in any way, in "normal" mode,
* find all references to deprecated code, in "verify" mode.

See [Technical guidelines](#technical-guidelines) for examples.

Once both parts are addressed,
get the PR reviewed and merged to the main branch.

File a different issue, to remove the deprecated code later.

Use [REMOVAL] in the title, and tag with label "Removal".

In the removal issue, refer to the corresponding deprecation issue.

### Release deprecation

When making a new opentelemetry-cpp release,
in addition to the regular release tasks:

* Replace every occurrences of `TO-BE-RELEASED-VERSION` in file
  `DEPRECATED` with the new release version,
* Replace every occurrences of `TO-BE-RELEASED-DATE` in file
  `DEPRECATED` with the new release date,
* In the `CHANGELOG` and the release notes, add references
  to the new deprecated items for this release,
  pointing to the `DEPRECATED` document.

### Wait

Do not implement the removal right away.

First, if a removal is implemented and merged to main just after a new release,
it exposes itself to a full revert, should another release be necessary to fix
any regressions just caused recently.

Second, some people will only notice the deprecation when discovering it
in the release, no matter how many previous announcements were done.
Allow some time for people to raise issues or concerns,
especially if there are special usage patterns that were not anticipated.

Once things are stable, proceed with the removal.
Depending on the change, it can be implemented:

* in the next minor release,
* after a few minor releases,
* in the next major release,
* after a few major releases

following the deprecation.

The more disruptive the change is,
the more time users will need to adjust,
so trivial deprecations can be removed right away,
while complex ones can take much longer.

In any case, never implement the deprecation and the removal in the same
release: wait.

### Prepare removal

Implement the removal issue with a PR.

Remove all the deprecated code.

Remove all the tooling (compiling options) related to the deprecated code,
if any.

Remove all the relevant entries in `DEPRECATED`.

Add a `CHANGELOG` entry for the removal.

Get the removal PR reviewed and merged to the main branch.

### Release removal

When making a new opentelemetry-cpp release,
in addition to the regular release tasks:

* In the `CHANGELOG` and the release notes, add references
  to the new removed items for this release.

## Technical guidelines

### CMake options deprecation

Assume the option `WITH_FOO` needs to be deprecated.

Code using `WITH_FOO=OFF` or `WITH_FOO=ON` must build as before,
yet users should be notified if they use `WITH_FOO` in their build.

CMake defines a `WITH_NO_DEPRECATED_CODE` option, set to OFF by default.

In a normal build, used in production, code is compiled with
`WITH_NO_DEPRECATED_CODE=OFF`.

In a verification build, code is compiled with `WITH_NO_DEPRECATED_CODE=ON`.
This verification also defines `OPENTELEMETRY_NO_DEPRECATED_CODE`, for code
level checks.

Implement the following logic in CMake:

```cmake
  option(WITH_FOO "DEPRECATED - With the foo feature" OFF)

  if(WITH_FOO)
    if(WITH_NO_DEPRECATED_CODE)
      message(FATAL_ERROR "WITH_FOO is deprecated")
    else()
      message(WARNING "WITH_FOO is deprecated")
    endif()
  endif
```

The verification build is not meant to be used in production.
It is meant to find all references of deprecated items.

If the verification build succeeds, the user code is guaranteed to be clean,
and will not be impacted by the removal to come.

When implementing such logic, document it in the mitigation section,
in file `DEPRECATED`.

### C++ deprecation

#### C++ header deprecation

When a header is deprecated, it will be removed,
so users should no longer include the header.

Add the following code in the header file

```cpp
#ifdef OPENTELEMETRY_NO_DEPRECATED_CODE
#  error "header <opentelemetry/foo.h> is deprecated."
#endif
```

#### macro deprecation

For macros, there are no `[[deprecated]]` annotations.

Replace the macro with something that is sure to fail at build time,
so it gets noticed when used.

```cpp
#ifndef OPENTELEMETRY_NO_DEPRECATED_CODE
  #define OTEL_MACRO_FOO(X, Y) ... (normal implementation) ...
#else
  #define OTEL_MACRO_FOO(X, Y) { this macro foo is deprecated }
#endif

```

#### C++ code deprecation

Assume a C++ item needs to be deprecated.

For example:

```cpp
  struct some_options
  {
    int x;
    int y; // TODO: deprecate me
    int z;
  };
```

Code using y must build as before, and yet users should be notified if still
using y.

First, there is a way in C++ to flag deprecated code:

```cpp
  struct some_options
  {
    int x;
    OPENTELEMETRY_DEPRECATED int y; // deprecated, to be removed
    int z;
  };
```

This will cause deprecation warnings.
Some users will notice, but others will not,
because warnings are most of the time ignored.

A better solution is to provide a way to forcefully
remove the deprecated code:

```cpp
  struct some_options
  {
    int x;
#ifndef OPENTELEMETRY_NO_DEPRECATED_CODE
    OPENTELEMETRY_DEPRECATED int y; // deprecated, to be removed
#endif
    int z;
  };
```

A regular build, to use in production, will build with
the regular, unchanged, configuration,
in which OPENTELEMETRY_NO_DEPRECATED_CODE is not defined.

A verification build, to _not_ use in production,
will build with `OPENTELEMETRY_NO_DEPRECATED_CODE` defined,
removing the deprecated member entirely.

A verification build that succeeds is a proof that
the application does not use the deprecated member,
and needs no change.

A verification build that fails will list all the application code that needs
to be fixed before the deprecated code is finally removed.

Even if the verification build succeeds, it really should not be used in
production, because the memory layout just changed, breaking the ABI.

This verification build is used to enforce:

* the API (Application _Programming_ Interface) subset is compatible,
  aka the source code will build before and after the removal.

By the time the removal is implemented:

```cpp
  struct some_options
  {
    int x;
    int z;
  };
```

the new release will have:

* a reduced API
* an ABI (Application _Binary_ Interface) change

The reduced API will be:

* a subset of the original API,
* compatible with applications which no longer use the removed code
* incompatible with applications which still uses the removed code

When documenting the deprecation, document this logic in the mitigation
section, so that users know how to find and remove old references to
deprecated code.

When documenting the removal, clarify in the release notes that the
release is API compatible (when application cleanup was done),
but not ABI compatible (when sizeof() or virtual tables of classes changed),
and therefore a full build must be done again.

This example used a member in a struct, but the same applies to other
constructs as well (classes, structs, functions, methods, etc).
