# OpenTelemetry C++  Example

## HTTP

This is a simple example that demonstrates tracing an HTTP request from client
to server. The example shows several aspects of tracing such as:

* Using the `TracerProvider`
* Using the `GlobalPropagator`
* Span Attributes
* Span Events
* Using the ostream exporter
* Nested spans (TBD)
* W3C Trace Context Propagation

### Running the example

1. The example uses HTTP server and client provided as part of this repo:
    * [HTTP
      Client](https://github.com/open-telemetry/opentelemetry-cpp/tree/main/ext/include/opentelemetry/ext/http/client)
    * [HTTP
      Server](https://github.com/open-telemetry/opentelemetry-cpp/tree/main/ext/include/opentelemetry/ext/http/server)

2. Build and Deploy the opentelemetry-cpp as described in
   [INSTALL.md](../../INSTALL.md)

3. Start the server from the `examples/http` directory

   ```console
   $ http_server 8800
    Server is running..Press ctrl-c to exit...
   ```

4. In a separate terminal window, run the client to make a single request:

    ```console
    $ ./http_client 8800
    ...
    ...
    ```

5. You should see console exporter output for both the client and server
   sessions.
   * Client console

   ```console

   {
     name          : /helloworld
     trace_id      : baa922bc0da6f79d46373371f4416463
     span_id       : 83bed4da7a01267d
     tracestate    :
     parent_span_id: 0000000000000000
     start         : 1620287026111457000
     duration      : 5164400
     description   :
     span kind     : Client
     status        : Unset
     attributes    :
           http.header.Date: Thu, 06 May 2021 07:43:46 GMT
           http.header.Content-Length: 0
           http.status_code: 200
           http.method: GET
           http.header.Host: localhost
           http.header.Content-Type: text/plain
           http.header.Connection: keep-alive
           http.scheme: http
           http.url: h**p://localhost:8800/helloworld
     events        :
     links         :

   }
   ```

   * Server console

   ```console

   {
     name          : /helloworld
     trace_id      : baa922bc0da6f79d46373371f4416463
     span_id       : c3e7e23042eb670e
     tracestate    :
     parent_span_id: 83bed4da7a01267d
     start         : 1620287026115443300
     duration      : 50700
     description   :
     span kind     : Server
     status        : Unset
     attributes    :
           http.header.Traceparent: 00-baa922bc0da6f79d46373371f4416463-83bed4da7a01267d-01
           http.header.Accept: */*
           http.request_content_length: 0
           http.header.Host: localhost:8800
           http.scheme: http
           http.client_ip: 127.0.0.1:50792
           http.method: GET
           net.host.port: 8800
           http.server_name: localhost
     events        :
           {
             name          : Processing request
             timestamp     : 1620287026115464000
             attributes    :
           }
     links         :

   }
   ```

   As seen from example above, `trace_id` and `parent_span_id` is propagated
   from client to server.
