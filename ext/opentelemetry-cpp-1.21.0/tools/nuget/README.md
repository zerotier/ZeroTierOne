# NuGet package creation process

This directory contains NuGet specification files (.nuspec).

Creating nuget packages:

1. Set `PackageVersion` environment variable that defines the package version.

```console
set PackageVersion=0.2.0
```

If environment variable is not set, then the version is automatically obtained from git tag:

```console
git describe --tags
```

2. Run `tools/build-nuget.cmd` script to create the package(s). Packages are copied to `.\packages` directory.

3. Push selected package to nuget feed.

Learn more about native code NuGet packages [here](https://docs.microsoft.com/en-us/nuget/guides/native-packages).

## Proposed NuGet package flavors

This section is a `DRAFT` - Work-in-Progress. The following packages MAY be created and published at nuget.org :

| Package Name                 | Desription                                                  | Notes |
|------------------------------|-------------------------------------------------------------|-------|
| OpenTelemetry.Cpp.Source     | Complete recursive source code snapshot with dependencies.  |       |
| OpenTelemetry.Cpp.Dev        | Source code snapshot excluding 3rd party dependencies.      |       |
| OpenTelemetry.Cpp.Api        | API headers package.                                        |       |
| OpenTelemetry.Cpp.Sdk        | SDK headers and source package.                             |       |
| OpenTelemetry.Cpp.Ext        | Extended functionality.                                     |       |
| OpenTelemetry.Cpp.Exporters  | Exporters.                                                  |       |

Prebuilt libraries MAY be provided in future in `OpenTelemetry.Cpp.${CompilerABI}` package for each compiler.

Refer to [Boost library](https://www.nuget.org/packages?q=boost) nuget packages for example how to create
a nuget package for a given compiler and runtime, using [Fuget Package Explorer](https://www.fuget.org/)
that allows to see the original nuget spec for each package.
