# Testing with SSL/TLS

## Required software

The opentelemetry-collector
[documentation](https://opentelemetry.io/docs/collector/configuration/#setting-up-certificates)
uses [cfssl](https://github.com/cloudflare/cfssl),
so we are using cfssl as well here.

In addition, install `openssl`, which provides tooling for testing.

## Generating CERT

### Generating CA CERT

First, write a CA request file in json, named `ca_csr.json`

It should contains the following data:

```console
shell> cat ca_csr.json
{
  "hosts": ["localhost", "127.0.0.1"],
  "key": {
    "algo": "rsa",
    "size": 2048
  },
  "names": [
    {
      "O": "OpenTelemetry CA Example"
    }
  ]
}
```

Then, generate a CA certificate:

```console
shell> cfssl genkey -initca ca_csr.json | cfssljson -bare ca
2022/12/13 16:42:57 [INFO] generate received request
2022/12/13 16:42:57 [INFO] received CSR
2022/12/13 16:42:57 [INFO] generating key: rsa-2048
2022/12/13 16:42:57 [INFO] encoded CSR
2022/12/13 16:42:57 [INFO] signed certificate with serial number 78705522450145914781445058194934247010984259280
```

This will create three files, `ca.csr`, `ca.pem` and `ca-key.pem`

Congratulation, "OpenTelemetry CA Example" is now a CA certification
authority, with signing keys.

### Generating Client CERT

Second, write a certificate request file in json, named `client_csr.json`,
for the opentelemetry-cpp client.

It should contains the following data:

```console
shell> cat client_csr.json
{
  "hosts": ["localhost", "127.0.0.1"],
  "key": {
    "algo": "rsa",
    "size": 2048
  },
  "names": [
    {
      "O": "OpenTelemetry Client Example"
    }
  ]
}
```

Note that the name ("OpenTelemetry Client Example") should be different
from the CA authority name ("OpenTelemetry CA Example"),
otherwise the client certificate will be self-signed, and rejected later in SSL/TLS.

Now, use the CA certificate generated in the previous step
to create and sign a new client certificate.

```console
shell> cfssl gencert -ca ca.pem -ca-key ca-key.pem client_csr.json | cfssljson -bare client_cert
2022/12/13 16:50:18 [INFO] generate received request
2022/12/13 16:50:18 [INFO] received CSR
2022/12/13 16:50:18 [INFO] generating key: rsa-2048
2022/12/13 16:50:18 [INFO] encoded CSR
2022/12/13 16:50:18 [INFO] signed certificate with serial number 579932794730090029542135367576037344135399122179
```

This will create three files, `client_cert.csr`, `client_cert.pem` and `client_cert-key.pem`

### Generating Server CERT

Third and likewise, write a certificate request file in json, named `server_csr.json`,
for the opentelemetry server (the opentelemetry-collector)

It should contains the following data:

```console
shell> cat server_csr.json
{
  "hosts": ["localhost", "127.0.0.1"],
  "key": {
    "algo": "rsa",
    "size": 2048
  },
  "names": [
    {
      "O": "OpenTelemetry Server Example"
    }
  ]
}
```

Likewise, use a different name from the CA authority name.

Use the CA certificate to create and sign a new server certificate.

```console
shell> cfssl gencert -ca ca.pem -ca-key ca-key.pem server_csr.json | cfssljson -bare server_cert
2022/12/13 17:04:40 [INFO] generate received request
2022/12/13 17:04:40 [INFO] received CSR
2022/12/13 17:04:40 [INFO] generating key: rsa-2048
2022/12/13 17:04:40 [INFO] encoded CSR
2022/12/13 17:04:40 [INFO] signed certificate with serial number 332420451189450993545238745169293824985460186638
```

This will create three files, `server_cert.csr`, `server_cert.pem` and `server_cert-key.pem`

### Verify certificates

Verify the certificates generated, using `openssl`:

```console
shell> openssl verify -CAfile ca.pem server_cert.pem client_cert.pem
server_cert.pem: OK
client_cert.pem: OK
```

Useful commands, to inspect certificates if needed (output not shown here)

```console
shell> openssl x509 -in ca.pem -text
```

```console
shell> openssl x509 -in client_cert.pem -text
```

```console
shell> openssl x509 -in server_cert.pem -text
```

## OpenTelemetry SSL clients

### Simulated client, for testing

Use `openssl` to simulate an opentelemetry-cpp client connecting to port 4318:

```console
shell> openssl s_client -connect localhost:4318 -CAfile ca.pem -cert client_cert.pem -key client_cert-key.pem
```

### OTLP HTTP Client

Use the example `example_otlp_http` client to connect to an OTLP HTTP server:

```console
shell> export OTEL_EXPORTER_OTLP_TRACES_ENDPOINT=https://localhost:4318/v1/traces
shell> export OTEL_EXPORTER_OTLP_TRACES_CERTIFICATE=ca.pem
shell> export OTEL_EXPORTER_OTLP_TRACES_CLIENT_CERTIFICATE=client_cert.pem
shell> export OTEL_EXPORTER_OTLP_TRACES_CLIENT_KEY=client_cert-key.pem
shell> example_otlp_http
```

## OpenTelemetry SSL servers

### Simulated server, for testing

Use `openssl` to simulate an opentelemetry-collector process serving port 4318:

```console
shell> openssl s_server -accept 4318 -CAfile ca.pem -cert server_cert.pem -key server_cert-key.pem
Using default temp DH parameters
ACCEPT
```

This command does not return,
the server waits for SSL connections (Use CTRL-C to quit).

### OTLP HTTP Server

To use a server that:

- implements OTLP HTTP
- supports SSL

use the opentelemetry-collector,
configured to use SSL/TLS for receivers:

```console
shell> cat otel-collector-config-ssl.yaml
receivers:
  otlp:
    protocols:
      http:
        tls:
          ca_file: ca.pem
          cert_file: server_cert.pem
          key_file: server_cert-key.pem
...
```

For example:

```console
shell> /path/to/bin/otelcorecol_linux_amd64 --config /path/to/otel-config-ssl.yaml
```

Note, the `example/http/http_example` can not be used (it understands neither SSL
nor OTLP HTTP).

## Testing SSL on the wire

### Fake client with fake server

This configuration tests nothing in opentelemetry,
but is useful to understand what a nominal SSL communication between a
client and a server should look like.

### Real client with fake server

In this configuration, we can test that the opentelemetry-cpp
client actually sends SSL traffic on the wire.

### Fake client with real server

In this configuration, we can test that the opentelemetry-collector
server actually accepts SSL traffic on the wire.
This can be used to verify the client keys are working properly.

## Full integration test with SSL

Start an opentelemetry-collector, configured to use SSL/TLS.

```console
shell> /path/to/bin/otelcorecol_linux_amd64 --config /path/to/otel-config-ssl.yaml
...
2022-12-13T18:03:21.140+0100    info    otlpreceiver@v0.66.0/otlp.go:89 Starting HTTP server    {"kind": "receiver", "name": "otlp", "pipeline": "metrics", "endpoint": "0.0.0.0:4318"}
2022-12-13T18:03:21.141+0100    info    service/pipelines.go:106        Receiver started.       {"kind": "receiver", "name": "otlp", "pipeline": "metrics"}
2022-12-13T18:03:21.141+0100    info    service/pipelines.go:102        Receiver is starting... {"kind": "receiver", "name": "otlp", "pipeline": "traces"}
2022-12-13T18:03:21.141+0100    info    service/pipelines.go:106        Receiver started.       {"kind": "receiver", "name": "otlp", "pipeline": "traces"}
2022-12-13T18:03:21.141+0100    info    service/service.go:105  Everything is ready. Begin running and processing data.
```

Start the example_otlp_http client, configured to use SSL/TLS.

```console
shell> export OTEL_EXPORTER_OTLP_TRACES_ENDPOINT=https://localhost:4318/v1/traces
shell> export OTEL_EXPORTER_OTLP_TRACES_CERTIFICATE=ca.pem
shell> export OTEL_EXPORTER_OTLP_TRACES_CLIENT_CERTIFICATE=client_cert.pem
shell> export OTEL_EXPORTER_OTLP_TRACES_CLIENT_KEY=client_cert-key.pem
shell> example_otlp_http
```

The opentelemetry-collector process receives data, as seen in logs:

```console
2022-12-13T18:05:36.611+0100 info TracesExporter {"kind": "exporter", "data_type": "traces", "name": "logging", "#spans": 4}
2022-12-13T18:05:36.611+0100 info ResourceSpans #0
Resource SchemaURL:
Resource attributes:
     -> service.name: Str(unknown_service)
     -> telemetry.sdk.version: Str(1.8.1)
     -> telemetry.sdk.name: Str(opentelemetry)
     -> telemetry.sdk.language: Str(cpp)
ScopeSpans #0
ScopeSpans SchemaURL:
InstrumentationScope foo_library 1.8.1
Span #0
    Trace ID       : ebbd7e13e9cdfb05f0ca9ed4b0cdf6c0
    Parent ID      : 042ae76539c294c6
    ID             : c87b21d63c505bae
    Name           : f1
    Kind           : Internal
    Start time     : 2022-12-13 17:05:36.482165738 +0000 UTC
    End time       : 2022-12-13 17:05:36.482170938 +0000 UTC
    Status code    : Unset
    Status message :
ResourceSpans #1
Resource SchemaURL:
Resource attributes:
     -> service.name: Str(unknown_service)
     -> telemetry.sdk.version: Str(1.8.1)
     -> telemetry.sdk.name: Str(opentelemetry)
     -> telemetry.sdk.language: Str(cpp)
ScopeSpans #0
ScopeSpans SchemaURL:
InstrumentationScope foo_library 1.8.1
Span #0
    Trace ID       : ebbd7e13e9cdfb05f0ca9ed4b0cdf6c0
    Parent ID      : 042ae76539c294c6
    ID             : 801227b9ee6d3b03
    Name           : f1
    Kind           : Internal
    Start time     : 2022-12-13 17:05:36.487636362 +0000 UTC
    End time       : 2022-12-13 17:05:36.487641983 +0000 UTC
    Status code    : Unset
    Status message :
ResourceSpans #2
Resource SchemaURL:
Resource attributes:
     -> service.name: Str(unknown_service)
     -> telemetry.sdk.version: Str(1.8.1)
     -> telemetry.sdk.name: Str(opentelemetry)
     -> telemetry.sdk.language: Str(cpp)
ScopeSpans #0
ScopeSpans SchemaURL:
InstrumentationScope foo_library 1.8.1
Span #0
    Trace ID       : ebbd7e13e9cdfb05f0ca9ed4b0cdf6c0
    Parent ID      : 6489f2ada8d95da0
    ID             : 042ae76539c294c6
    Name           : f2
    Kind           : Internal
    Start time     : 2022-12-13 17:05:36.482154908 +0000 UTC
    End time       : 2022-12-13 17:05:36.488641122 +0000 UTC
    Status code    : Unset
    Status message :
ResourceSpans #3
Resource SchemaURL:
Resource attributes:
     -> service.name: Str(unknown_service)
     -> telemetry.sdk.version: Str(1.8.1)
     -> telemetry.sdk.name: Str(opentelemetry)
     -> telemetry.sdk.language: Str(cpp)
ScopeSpans #0
ScopeSpans SchemaURL:
InstrumentationScope foo_library 1.8.1
Span #0
    Trace ID       : ebbd7e13e9cdfb05f0ca9ed4b0cdf6c0
    Parent ID      :
    ID             : 6489f2ada8d95da0
    Name           : library
    Kind           : Internal
    Start time     : 2022-12-13 17:05:36.482136052 +0000 UTC
    End time       : 2022-12-13 17:05:36.489263125 +0000 UTC
    Status code    : Unset
    Status message :
```
