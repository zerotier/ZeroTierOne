# ETW Exporter multithreaded context propagation example

## Preface

This example shows how to pass context from main dispatcher thread to worker threads.
While this example is convenient to run in Visual Studio with ETW exporter, the same
logic should apply to any other exporter. Only the initial portion that obtains ETW
Tracer is unique to ETW, the rest can be reused.

## How to debug events in Visual Studio 2019 or newer

Specify your component instrumentation name, which should match the destination ETW
Provider Name or GUID. Example uses "OpenTelemetry-ETW-TLD" for the instrument /
instrumentation name.

In Visual Studio IDE:

- navigate to `View -> Other Windows -> Diagnostic Events...`
- click `Configure...` gear on top.
- specify `OpenTelemetry-ETW-TLD` in the list of providers to monitor.
- run example.
- `Diagnostic Events` view shows you the event flow in realtime.

## Explanation of the code flow

`main` function acts as a dispatcher to manage thread pool called `pool`. `beep_bop`
span is started in the `main`. Then in a loop up to `kMaxThreads` get started
concurrently. Each thread executing `beep` function with a parent scope of `main`.
`beep` subsequently calls into `bop` function, with a parent scope of `beep` span.
Entire execution of all threads is grouped under the main span called `beep_bop`.
At the end of execution, the `main` function joins all pending threads and ends
the main span.
