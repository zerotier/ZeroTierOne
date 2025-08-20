# Design of building OpenTelemetry C++ SDK and exporters as DLL

**NOTE**: the current build produces a single DLL which includes API, SDK and
exporters. This **MAY** change in future.

OpenTelemetry C++ can be built into single DLL named opentelemetry_cpp.dll on
Windows, with CMake flag `OPENTELEMETRY_BUILD_DLL` set to `ON`.

To instrument an application based on the DLL, please define macro
`OPENTELEMETRY_BUILD_IMPORT_DLL` before including any OpenTelemetry API header
files in the application source files.
