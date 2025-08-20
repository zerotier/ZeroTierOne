# OpenTelemetry C++  Example

## gRPC

This is a simple example that demonstrates tracing a gRPC request from client to
server. There is an experimental directory in this example - the code within has
been commented out to prevent any conflicts. The example shows several aspects
of tracing such as:

* Using the `TracerProvider`
* Implementing the TextMapCarrier
* Context injection/extraction
* Span Attributes
* Span Semantic Conventions
* Using the ostream exporter
* Nested spans
* W3c Trace Context Propagation (Very soon!)

### Running the example

1. The example uses gRPC C++ as well as Google's protocol buffers. Make sure you
   have installed both of these packages on your system, in such a way that
   CMake would know how to find them with this command:

    ``find_package(gRPC)``

2. Build and Deploy the opentelemetry-cpp as described in
   [INSTALL.md](../../INSTALL.md). Building the project will build all of the
   examples and create new folders containing their executables within the
   'build' directory NOT the 'examples' directory.

3. Start the server from your `build/examples/grpc` directory. Both the server
   and client are configured to use 8800 as the default port, but if you would
   like to use another port, you can specify that as an argument.

   ```console
   $ ./server [port_num]
    Server listening on port: 0.0.0.0:8800
   ```

4. In a separate terminal window, run the client to make a single request:

    ```console
    $ ./client [port_num]
    ...
    ```

5. You should see console exporter output for both the client and server
   sessions.
   * Client console

   ```console
   {
        name          : GreeterClient/Greet
        trace_id      : f5d16f8399be0d2c6b39d992634ffdbb
        span_id       : 9c79a2dd744d7d2d
        tracestate    :
        parent_span_id: 0000000000000000
        start         : 1622603339918985700
        duration      : 4960500
        description   :
        span kind     : Client
        status        : Ok
        attributes    :
            rpc.grpc.status_code: 0
            net.peer.port: 8080
            net.peer.ip: 0.0.0.0
            rpc.method: Greet
            rpc.service: grpc-example.GreetService
            rpc.system: grpc
        events        :
   }
   ```

   * Server console

   ```console
   {
        name          : GreeterService/Greet
        trace_id      : f5d16f8399be0d2c6b39d992634ffdbb
        span_id       : 1e8a7d2d46e08573
        tracestate    :
        parent_span_id: 9c79a2dd744d7d2d
        start         : 1622603339923163800
        duration      : 76400
        description   :
        span kind     : Server
        status        : Ok
        attributes    :
            rpc.grpc.status_code: 0
            rpc.method: Greet
            rpc.service: GreeterService
            rpc.system: grpc
        events        :
            {
                name          : Processing client attributes
                timestamp     : 1622603339923180800
                attributes    :
            }
            {
                name          : Response sent to client
                timestamp     : 1622603339923233700
                attributes    :
            }
        links         :
    }
   ```
