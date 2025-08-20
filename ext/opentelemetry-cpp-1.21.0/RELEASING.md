# Release Process

## Pre Release

1: Upgrade to latest [dependencies](docs/maintaining-dependencies.md)
if required.

2: Make sure all relevant changes for this release are included under
`Unreleased` section in `CHANGELOG.md` and are in language that non-contributors
to the project can understand.

3: Run the pre-release script. It creates a branch `pre_release_<new-tag>` and
updates `CHANGELOG.md` with the `<new-tag>`:

```sh
./buildscripts/pre_release.sh -t <new-tag>
```

4: Verify that CHANGELOG.md is updated properly:

```sh
git diff main
```

5: Push the changes to upstream and create a Pull Request on GitHub. Be sure to
include the curated changes from the [Changelog](./CHANGELOG.md) in the
description.

## Tag

Once the above Pull Request has been approved and merged it is time to tag the
merged commit.

***IMPORTANT***: It is critical you use the same tag that you used in the
Pre-Release step! Failure to do so will leave things in a broken state.

1: Note down the commit hash of the master branch after above PR request is
merged: `<commit-hash>`

```sh
git show -s --format=%H
```

2: Create a github tag on this commit hash:

```sh
git tag -a "<new-tag>" -s -m "Version <new-tag>" "<commit-hash>"
```

3: Push tag to upstream remote

```sh
git push upstream
```

## Versioning

Once tag is created, it's time to use that tag for Runtime Versioning

1: Create a new brach for updating version information in
`./sdk/src/version.cc`.

```sh
git checkout -b update_version_${tag} master
```

2: Run the pre-commit script to update the version:

```sh
./buildscripts/pre-commit
```

3: Check if any changes made since last release broke ABI compatibility. If yes,
update `OPENTELEMETRY_ABI_VERSION_NO` in
[version.h](api/include/opentelemetry/version.h).

4: Push the changes to upstream and create a Pull Request on GitHub.

5: Once changes are merged, move the tag created earlier to the new commit hash
from step 4.

```sh
git tag -f <previous-tag> <new-commit-hash>
git push --tags --force
```

## Release

Finally create a Release for the new `<new-tag>` on GitHub. The release body
should include all the release notes from the Changelog for this release.

## Post Release

Update the OpenTelemetry.io document
[here](https://github.com/open-telemetry/opentelemetry.io/tree/main/content/en/docs/instrumentation/cpp)
by sending a Pull Request.
