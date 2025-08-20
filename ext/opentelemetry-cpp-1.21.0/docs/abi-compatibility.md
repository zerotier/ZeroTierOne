# Windows

## Visual Studio 2015, 2017, 2019

The Microsoft C++ (MSVC) compiler toolsets in Visual Studio 2013 and earlier
don't guarantee binary compatibility across versions. You can't link object
files, static libraries, dynamic libraries, and executables built by different
versions. The ABIs, object formats, and runtime libraries are incompatible.

We've changed this behavior in Visual Studio 2015, 2017, and 2019. The runtime
libraries and apps compiled by any of these versions of the compiler are
binary-compatible. It's reflected in the C++ toolset major number, which is 14
for all three versions. (The toolset version is v140 for Visual Studio 2015,
v141 for 2017, and v142 for 2019). Say you have third-party libraries built by
Visual Studio 2015. You can still use them in an application built by Visual
Studio 2017 or 2019. There's no need to recompile with a matching toolset. The
latest version of the Microsoft Visual C++ Redistributable package (the
Redistributable) works for all of them.

There are three important restrictions on binary compatibility:

- You can mix binaries built by different versions of the toolset. However, you
  must use a toolset at least as recent as the most recent binary to link your
  app. Here's an example: you can link an app compiled using the 2017 toolset to
  a static library compiled using 2019, if they're linked using the 2019
  toolset.

- The Redistributable your app uses has a similar binary-compatibility
  restriction. When you mix binaries built by different supported versions of
  the toolset, the Redistributable version must be at least as new as the latest
  toolset used by any app component.

- Static libraries or object files compiled using the /GL (Whole program
  optimization) compiler switch aren't binary-compatible across versions. All
  object files and libraries compiled using /GL must use exactly the same
  toolset for the compile and the final link.

[Reference](https://docs.microsoft.com/en-us/cpp/porting/binary-compat-2015-2017?view=vs-2019)

## Breaking Compatibility

### Exceptions

- [__CxxFrameHandler4](https://devblogs.microsoft.com/cppblog/making-cpp-exception-handling-smaller-x64/)
  - static library built with Visual Studio 2019 wont link to executable
  compiled with Visual Studio 2017

### Release vs Debug libraries on Windows

- [\_SECURE_SCL](https://docs.microsoft.com/en-us/cpp/standard-library/secure-scl?view=vs-2019)
  - checked iterators (old)

- [\_ITERATOR_DEBUG_LEVEL](https://docs.microsoft.com/en-us/cpp/standard-library/iterator-debug-level?view=vs-2019)
  - checked iterators (new)

### Spectre Mitigation

- [Runtime Libraries for Spectre
  Mitigation](https://docs.microsoft.com/en-us/cpp/build/reference/qspectre?view=vs-2019)

- [New Spectre Mitigations in Visual Studio
  2019](https://devblogs.microsoft.com/cppblog/more-spectre-mitigations-in-msvc/)
