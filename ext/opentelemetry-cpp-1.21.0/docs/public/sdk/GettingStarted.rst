Getting started
^^^^^^^^^^^^^^^

OpenTelemetry C++ SDK provides the reference implementation of OpenTelemetry C++ API,
and also provides implementation for Processor, Sampler, and core Exporters as per the
specification.


Exporter
^^^^^^^^

An exporter is responsible for sending the telemetry data to a particular backend.
OpenTelemetry offers six tracing exporters out of the box:

- In-Memory Exporter: keeps the data in memory, useful for debugging.
- Zipkin Exporter: prepares and sends the collected telemetry data to a Zipkin backend via the Zipkin APIs.
- Logging Exporter: saves the telemetry data into log streams.
- OpenTelemetry(otlp) Exporter: sends the data to the OpenTelemetry Collector using protobuf/gRPC or protobuf/HTTP.
- ETW Exporter: sends the telemetry data to Event Tracing for Windows (ETW).

.. code:: cpp

    //namespace alias used in sample code here.
    namespace sdktrace   = opentelemetry::sdk::trace;

    // logging exporter
    auto ostream_exporter =
        std::unique_ptr<sdktrace::SpanExporter>(new opentelemetry::exporter::trace::OStreamSpanExporter);

    // memory exporter
    auto memory_exporter =
        std::unique_ptr<sdktrace::SpanExporter>(new opentelemetry::exporter::memory::InMemorySpanExporter);

    // zipkin exporter
    opentelemetry::exporter::zipkin::ZipkinExporterOptions opts;
    opts.endpoint = "http://localhost:9411/api/v2/spans" ; // or export OTEL_EXPORTER_ZIPKIN_ENDPOINT="..."
    opts.service_name = "default_service" ;
    auto zipkin_exporter =
        std::unique_ptr<sdktrace::SpanExporter>(new opentelemetry::exporter::zipkin::ZipkinExporter(opts));

    // otlp grpc exporter
    opentelemetry::exporter::otlp::OtlpGrpcExporterOptions opts;
    opts.endpoint = "localhost:4317";
    opts.use_ssl_credentials = true;
    opts.ssl_credentials_cacert_as_string = "ssl-certificate";
    auto otlp_grpc_exporter =
        std::unique_ptr<sdktrace::SpanExporter>(new opentelemetry::exporter::otlp::OtlpGrpcExporter(opts));

    // otlp http exporter
    opentelemetry::exporter::otlp::OtlpHttpExporterOptions opts;
    opts.url = "http://localhost:4318/v1/traces";
    auto otlp_http_exporter =
        std::unique_ptr<sdktrace::SpanExporter>(new opentelemetry::exporter::otlp::OtlpHttpExporter(opts));


Span Processor
^^^^^^^^^^^^^^

Span Processor is initialised with an Exporter. Different Span Processors are offered by OpenTelemetry C++ SDK:

- SimpleSpanProcessor: immediately forwards ended spans to the exporter.
- BatchSpanProcessor: batches the ended spans and send them to exporter in bulk.
- MultiSpanProcessor: Allows multiple span processors to be active and configured at the same time.

.. code:: cpp

    // simple processor
    auto simple_processor = std::unique_ptr<sdktrace::SpanProcessor>(
        new sdktrace::SimpleSpanProcessor(std::move(ostream_exporter)));

    // batch processor
    sdktrace::BatchSpanProcessorOptions options{};
    auto batch_processor = std::unique_ptr<sdktrace::SpanProcessor>(
        new sdktrace::BatchSpanProcessor(std::move(memory_exporter), options));

    // multi-processor
    std::vector<std::unique_ptr<SpanProcessor>>
        processors{std::move(simple_processor), std::move(batch_processor)};
    auto multi_processor = std::unique_ptr<sdktrace::SpanProcessor>(
        new sdktrace::MultiSpanProcessor(std::move(processors));

Resource
^^^^^^^^

A Resource is an immutable representation of the entity producing telemetry as key-value pair.
The OpenTelemetry C++ SDK allow for creation of Resources and for associating them with telemetry.

.. code:: cpp

    auto resource_attributes = opentelemetry::sdk::resource::ResourceAttributes
        {
            {"service.name", "shoppingcart"},
            {"service.instance.id", "instance-12"}
        };
    auto resource = opentelemetry::sdk::resource::Resource::Create(resource_attributes);
    auto received_attributes = resource.GetAttributes();
    // received_attributes contains
    //      - service.name = shoppingcart
    //      - service.instance.id = instance-12
    //      - telemetry.sdk.name = opentelemetry
    //      - telemetry.sdk.language = cpp
    //      - telemetry.sdk.version = <current sdk version>

It is possible to define the custom resource detectors by inhering from
`opentelemetry::sdk::Resource::ResourceDetector` class.

Sampler
^^^^^^^

Sampling is mechanism to control/reducing the number of samples of traces collected and sent to the backend.
OpenTelemetry C++ SDK  offers four samplers out of the box:

- AlwaysOnSampler which samples every trace regardless of upstream sampling decisions.
- AlwaysOffSampler which doesn’t sample any trace, regardless of upstream sampling decisions.
- ParentBased which uses the parent span to make sampling decisions, if present.
- TraceIdRatioBased which samples a configurable percentage of traces.

.. code:: cpp

    //AlwaysOnSampler
    auto always_on_sampler = std::unique_ptr<sdktrace::AlwaysOnSampler>
        (new sdktrace::AlwaysOnSampler);

    //AlwaysOffSampler
    auto always_off_sampler = std::unique_ptr<sdktrace::AlwaysOffSampler>
        (new sdktrace::AlwaysOffSampler);

    //ParentBasedSampler
    auto parent_based_sampler = std::unique_ptr<sdktrace::ParentBasedSampler>
        (new sdktrace::ParentBasedSampler);

    //TraceIdRatioBasedSampler - Sample 50% generated spans
    double ratio       = 0.5;
    auto always_off_sampler = std::unique_ptr<sdktrace::TraceIdRatioBasedSampler>
        (new sdktrace::TraceIdRatioBasedSampler(ratio));

TracerContext
^^^^^^^^^^^^^

SDK configuration are shared between `TracerProvider` and all it's `Tracer` instances through `TracerContext`.

.. code:: cpp

    auto tracer_context = std::make_shared<sdktrace::TracerContext>
        (std::move(multi_processor), resource, std::move(always_on_sampler));

TracerProvider
^^^^^^^^^^^^^^

`TracerProvider` instance holds the SDK configurations ( Span Processors, Samplers, Resource). There is single
global TracerProvider instance for an application, and it is created at the start of application.
There are two different mechanisms to create TraceProvider instance

- Using constructor which takes already created TracerContext shared object as parameter.
- Using consructor which takes SDK configurations as parameter.

.. code:: cpp

    // Created using `TracerContext` instance
    auto tracer_provider = nostd::shared_ptr<sdktrace::TracerProvider>
        (new sdktrace::TracerProvider(tracer_context));

    // Create using SDK configurations as parameter
    auto tracer_provider = nostd::shared_ptr<sdktrace::TracerProvider>
        (std::move(simple_processor), resource, std::move(always_on_sampler));

    // set the global tracer TraceProvider
    opentelemetry::trace::Provider::SetTracerProvider(tracer_provider);


Logging and Error Handling
^^^^^^^^^^^^^^^^^^^^^^^^^^

OpenTelemetry C++ SDK provides mechanism for application owner to add customer log and error handler.
The default log handler is redirected to standard output ( using std::cout ).

The logging macro supports logging using C++ stream format, and key-value pair.
The log handler is meant to capture errors and warnings arising from SDK, not supposed to be used for the application errors.
The different log levels are supported - Error, Warn, Info and Debug. The default log level is Warn ( to dump both Error and Warn)
and it can be changed at compile time.

.. code:: cpp

    OTEL_INTERNAL_LOG_ERROR(" Connection failed. Error string " << error_str << " Error Num: " << errorno);
    opentelemetry::sdk::common::AttributeMap error_attributes = {
      {"url", url}, {"content-length", len}, {"content-type", type}};
    OTEL_INTERNAL_LOG_ERROR(" Connection failed." , error_attributes);
    opentelemetry::sdk::common::AttributeMap http_attributes = {
      {"url", url}, {"content-length", len}, {"content-type", type}};
    OTEL_INTERNAL_LOG_DEBUG(" Connection Established Successfully. Headers:", http_attributes);

The custom log handler can be defined by inheriting from `opentelemetry::sdk::common::internal_log::LogHandler` class.

.. code:: cpp

    class CustomLogHandler : public opentelemetry::sdk::common::internal_log::LogHandler
    {
        void Handle(opentelemetry::sdk::common::internal_log::LogLevel level,
                    const char \*file,
                    int line,
                    const char \*msg,
                    const opentelemetry::sdk::common::AttributeMap &attributes) noexcept override

        {
            // add implementation here
        }
    };
    opentelemetry::sdk::common::internal_log::GlobalLogHandler::SetLogHandler(CustomLogHandler());
    opentelemetry::sdk::common::internal_log::GlobalLogHandler::SetLogLevel(opentelemetry::sdk::common::internal_log::LogLevel::Debug);
