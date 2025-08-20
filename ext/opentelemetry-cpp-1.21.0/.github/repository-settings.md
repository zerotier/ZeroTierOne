# Process

This file documents local admin changes for opentelemetry-cpp,
per the community process: https://github.com/open-telemetry/community/blob/main/docs/how-to-configure-new-repository.md

Please note that the EasyCLA check **MUST** stay **REQUIRED**,
it should never be disabled or bypassed, at the risk of tainting the repository.

# Guidelines

The best is to open a PR first that describes the change,
so it can be discussed during review (maybe it is not needed,
maybe there is an alternate solution, ...).

The PR must add a log entry in this file, detailing:

* the date the change is implemented
* what is changed exactly (which setting)
* a short rationale

Admin changes are then applied only when the PR is merged.

If for some reason a change is implemented in emergency,
before a PR can be discussed and merged,
a PR should still be prepared and pushed after the fact to
describe the settings changed.

# Log of local changes

## 2023-11-03

Created log file `.github/repository-settings.md`, since admin permissions are now granted to maintainers.

See https://github.com/open-telemetry/community/issues/1727

No setting changed.


