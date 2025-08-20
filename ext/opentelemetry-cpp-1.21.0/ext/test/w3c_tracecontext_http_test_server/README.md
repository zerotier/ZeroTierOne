# Test service endpoint for W3C validation

This test application is intended to be used as a test service for the [W3C
Distributed Tracing Validation
Service](https://github.com/w3c/trace-context/tree/master/test). It is
implemented according to [these
instructions](https://github.com/w3c/trace-context/tree/master/test#implement-test-service).

## Usage

1: Build and start the test service endpoint:

```sh
./w3c_tracecontext_http_test_server

Listening to http://localhost:30000/test
```

A custom port number for the test service to listen to can be specified:

```sh
./w3c_tracecontext_http_test_server 31339

Listening to http://localhost:31339/test
```

The test service will print the full URI that the validation service can connect
to.

2: In a different terminal, set up and start the validation service according to
the
[instructions](https://github.com/w3c/trace-context/tree/master/test#run-test-cases),
giving the address of the test service endpoint as argument:

```sh
python test.py http://localhost:31339/test
```

One can also use the `Dockerfile` provided in this folder to conveniently run
the validation service:

```sh
docker build --tag w3c_driver .
docker run --network host w3c_driver http://localhost:31339/test
```

3: The validation service will run the test suite and print detailed test
results.

4: Stop the test service by invoking `/stop`. Make sure to use the correct port number.

```sh
# Assuming the service is currently running at port 30000
curl http://localhost:30000/stop
```
