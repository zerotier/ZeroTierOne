Getting started
---------------

Tracing
~~~~~~~

When instrumenting libraries and applications, the most simple approach
requires three steps.

Obtain a tracer
^^^^^^^^^^^^^^^

.. code:: cpp

    auto provider = opentelemetry::trace::Provider::GetTracerProvider();
    auto tracer = provider->GetTracer("foo_library", "1.0.0");

The ``TracerProvider`` acquired in the first step is a singleton object
that is usually provided by the OpenTelemetry C++ SDK. It is used to
provide specific implementations for API interfaces. In case no SDK is
used, the API provides a default no-op implementation of a
``TracerProvider``.

The ``Tracer`` acquired in the second step is needed to create and start
``Span``\ s.

Start a span
^^^^^^^^^^^^

.. code:: cpp

    auto span = tracer->StartSpan("HandleRequest");

This creates a span, sets its name to ``"HandleRequest"``, and sets its
start time to the current time. Refer to the API documentation for other
operations that are available to enrich spans with additional data.

Mark a span as active
^^^^^^^^^^^^^^^^^^^^^

.. code:: cpp

    auto scope = tracer->WithActiveSpan(span);

This marks a span as active and returns a ``Scope`` object. The scope object
controls how long a span is active. The span remains active for the lifetime
of the scope object.

The concept of an active span is important, as any span that is created
without explicitly specifying a parent is parented to the currently
active span. A span without a parent is called root span.

Create nested Spans
^^^^^^^^^^^^^^^^^^^

.. code:: cpp

    auto outer_span = tracer->StartSpan("Outer operation");
    auto outer_scope = tracer->WithActiveSpan(outer_span);
    {
        auto inner_span = tracer->StartSpan("Inner operation");
        auto inner_scope = tracer->WithActiveSpan(inner_span);
        // ... perform inner operation
        inner_span->End();
    }
    // ... perform outer operation
    outer_span->End();


Spans can be nested, and have a parent-child relationship with other spans.
When a given span is active, the newly created span inherits the active span's
trace ID, and other context attributes.

Context Propagation
^^^^^^^^^^^^^^^^^^

.. code:: cpp

    // set global propagator
    opentelemetry::context::propagation::GlobalTextMapPropagator::SetGlobalPropagator(
        nostd::shared_ptr<opentelemetry::context::propagation::TextMapPropagator>(
            new opentelemetry::trace::propagation::HttpTraceContext()));

    // get global propagator
    HttpTextMapCarrier<opentelemetry::ext::http::client::Headers> carrier;
    auto propagator =
        opentelemetry::context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();

    //inject context to headers
    auto current_ctx = opentelemetry::context::RuntimeContext::GetCurrent();
    propagator->Inject(carrier, current_ctx);

    //Extract headers to context
    auto current_ctx = opentelemetry::context::RuntimeContext::GetCurrent();
    auto new_context = propagator->Extract(carrier, current_ctx);
    auto remote_span = opentelemetry::trace::propagation::GetSpan(new_context);



``Context`` contains the meta-data of the currently active Span including Span Id,
Trace Id, and flags. Context Propagation is an important mechanism in distributed
tracing to transfer this Context across service boundary often through HTTP headers.
OpenTelemetry provides a text-based approach to propagate context to remote services
using the W3C Trace Context HTTP headers.
