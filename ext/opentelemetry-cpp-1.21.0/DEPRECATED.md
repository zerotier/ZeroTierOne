# Deprecated

This document lists all the items currently deprecated in opentelemetry-cpp.

Deprecated items will be removed in the future.

## Guidelines

### Maintainer guidelines

See the [deprecation-process](./docs/deprecation-process.md)

## [TEMPLATE]

### New Deprecation Title (Template)

#### Announcement (Template)

#### Motivation (Template)

#### Scope (Template)

#### Mitigation (Template)

#### Planned removal (Template)

## [Platforms]

N/A

## [Compilers]

N/A

## [Third party dependencies]

N/A

## [Build tools]

N/A

## [Build scripts]

N/A

## [opentelemetry-cpp API]

N/A

## [opentelemetry-cpp SDK]

N/A

## [opentelemetry-cpp Exporter]

N/A

## [Documentation]

N/A

## Semantic conventions

### Header files "semantic_conventions.h"

#### Announcement (semantic_conventions.h)

Deprecation is announced as part of the migration to weaver:

* `Version:` release following opentelemetry-cpp 1.17.0
* `Date:` Nov 9, 2024
* `PR:` [PR 3105](https://github.com/open-telemetry/opentelemetry-cpp/pull/3105)

#### Motivation (semantic_conventions.h)

The header files for semantic conventions are generated automatically.
The tooling to generate these files is changing:

* before, the build-tool repository was used
* now, the weaver repository is used

Changes in tooling allows to generate code that is better organized,
with dedicated header files per group of semantic conventions,
instead of a single header file for everything.

#### Scope (semantic_conventions.h)

The following files:

* `api/include/opentelemetry/trace/semantic_conventions.h`
* `sdk/include/opentelemetry/sdk/resource/semantic_conventions.h`

are now deprecated.

They correspond to semantic conventions v1.27.0,
and will no longer be maintained up to date.

These files will be removed in the future.

#### Mitigation (semantic_conventions.h)

Two things have changed:

* the header file to use
* the symbol name to use.

Before, the semantic convention for `url.full` was:

* declared in file `semantic_conventions.h`
* declared as symbol `SemanticConventions::kUrlFull`

Now, the `url.full` convention, which is part or the `url` group, is:

* declared in file `semconv/url_attributes.h`
* declared as symbol `semconv::url::kUrlFull`

Application code that uses semantic conventions must be adjusted
accordingly.

In addition, semantic conventions that are not marked as stable
are generated in a different header file, placed under directory
`incubating`, to better separate stable and non stable code.

For example, file `semconv/incubating/url_attributes.h`
defines `semconv::url::kUrlDomain`,
which is not marked as stable in semconv v1.27.0

#### Planned removal (semantic_conventions.h)

The following files:

* `api/include/opentelemetry/trace/semantic_conventions.h`
* `sdk/include/opentelemetry/sdk/resource/semantic_conventions.h`

will be removed.

The removal date is planned for July 1, 2025.
This allows more than six months for applications to adjust.
