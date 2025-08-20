Overview
========

The OpenTelemetry C++ API enables developers to instrument their
applications and libraries in order to make them ready to create and
emit telemetry data. The OpenTelemetry C++ API exclusively focuses on
instrumentation and does not address concerns like exporting, sampling,
and aggregating telemetry data. Those concerns are addressed by the
OpenTelemetry C++ SDK. This architecture enables developers to
instrument applications and libraries with the OpenTelemetry C++ API
while being completely agnostic of how telemetry data is exported and
processed.

Library design
--------------

The OpenTelemetry C++ API is provided as a header-only library and
supports all recent versions of the C++ standard, down to C++14.

A single application might dynamically or statically link to different
libraries that were compiled with different compilers, while several of
the linked libraries are instrumented with OpenTelemetry. OpenTelemetry
C++ supports those scenarios by providing a stable ABI. This is achieved
by a careful API design, and most notably by providing ABI stable
versions of classes from the standard library. All those classes are
provided in the ``opentelemetry::nostd`` namespace.
