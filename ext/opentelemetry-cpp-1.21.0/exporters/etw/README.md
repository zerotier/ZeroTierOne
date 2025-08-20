# Getting Started with OpenTelemetry C++ SDK and ETW exporter on Windows

Event Tracing for Windows (ETW) is an efficient kernel-level tracing facility
that lets you log kernel or application-defined events to a log file. You can
consume the events in real time or from a log file and use them to debug an
application or to determine where performance issues are occurring in the
application.

OpenTelemetry C++ SDK ETW exporter allows the code instrumented using
OpenTelemetry API to forward events to out-of-process ETW listener, for
subsequent data recording or forwarding to alternate pipelines and flows.
Windows Event Tracing infrastructure is available to any vendor or application
being deployed on Windows.

It is recommended to consume this exporter via
[vcpkg](https://vcpkg.io/en/package/opentelemetry-cpp).
