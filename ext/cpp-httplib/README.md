cpp-httplib
===========

[![Build Status](https://travis-ci.org/yhirose/cpp-httplib.svg?branch=master)](https://travis-ci.org/yhirose/cpp-httplib)
[![Bulid Status](https://ci.appveyor.com/api/projects/status/github/yhirose/cpp-httplib?branch=master&svg=true)](https://ci.appveyor.com/project/yhirose/cpp-httplib)

A C++ header-only cross platform HTTP/HTTPS library.

It's extremely easy to setup. Just include **httplib.h** file in your code!

Inspired by [Sinatra](http://www.sinatrarb.com/) and [express](https://github.com/visionmedia/express).

Server Example
--------------

```c++
#include <httplib.h>

int main(void)
{
    using namespace httplib;

    Server svr;

    svr.Get("/hi", [](const Request& req, Response& res) {
        res.set_content("Hello World!", "text/plain");
    });

    svr.Get(R"(/numbers/(\d+))", [&](const Request& req, Response& res) {
        auto numbers = req.matches[1];
        res.set_content(numbers, "text/plain");
    });

    svr.listen("localhost", 1234);
}
```

`Post`, `Put`, `Delete` and `Options` methods are also supported.

### Bind a socket to multiple interfaces and any available port

```cpp
int port = svr.bind_to_any_port("0.0.0.0");
svr.listen_after_bind();
```

### Method Chain

```cpp
svr.Get("/get", [](const auto& req, auto& res) {
        res.set_content("get", "text/plain");
    })
    .Post("/post", [](const auto& req, auto& res) {
        res.set_content(req.body(), "text/plain");
    })
    .listen("localhost", 1234);
```

### Static File Server

```cpp
svr.set_base_dir("./www");
```

### Logging

```cpp
svr.set_logger([](const auto& req, const auto& res) {
    your_logger(req, res);
});
```

### Error Handler

```cpp
svr.set_error_handler([](const auto& req, auto& res) {
    const char* fmt = "<p>Error Status: <span style='color:red;'>%d</span></p>";
    char buf[BUFSIZ];
    snprintf(buf, sizeof(buf), fmt, res.status);
    res.set_content(buf, "text/html");
});
```

### 'multipart/form-data' POST data

```cpp
svr.Post("/multipart", [&](const auto& req, auto& res) {
    auto size = req.files.size();
    auto ret = req.has_file("name1"));
    const auto& file = req.get_file_value("name1");
    // file.filename;
    // file.content_type;
    auto body = req.body.substr(file.offset, file.length));
})
```

Client Example
--------------

### GET

```c++
#include <httplib.h>
#include <iostream>

int main(void)
{
    httplib::Client cli("localhost", 1234);

    auto res = cli.Get("/hi");
    if (res && res->status == 200) {
        std::cout << res->body << std::endl;
    }
}
```

### GET with Content Receiver

```c++
  std::string body;
  auto res = cli.Get("/large-data", [&](const char *data, size_t len) {
    body.append(data, len);
  });
  assert(res->body.empty());
```

### POST

```c++
res = cli.Post("/post", "text", "text/plain");
res = cli.Post("/person", "name=john1&note=coder", "application/x-www-form-urlencoded");
```

### POST with parameters

```c++
httplib::Params params;
params.emplace("name", "john");
params.emplace("note", "coder");

auto res = cli.Post("/post", params);
```
 or

```c++
httplib::Params params{
  { "name", "john" },
  { "note", "coder" }
};

auto res = cli.Post("/post", params);
```

### PUT

```c++
res = cli.Put("/resource/foo", "text", "text/plain");
```

### DELETE

```c++
res = cli.Delete("/resource/foo");
```

### OPTIONS

```c++
res = cli.Options("*");
res = cli.Options("/resource/foo");
```

### Connection Timeout

```c++
httplib::Client cli("localhost", 8080, 5); // timeouts in 5 seconds
```
### With Progress Callback

```cpp
httplib::Client client(url, port);

// prints: 0 / 000 bytes => 50% complete
std::shared_ptr<httplib::Response> res =
    cli.Get("/", [](uint64_t len, uint64_t total) {
        printf("%lld / %lld bytes => %d%% complete\n",
            len, total,
            (int)((len/total)*100));
        return true; // return 'false' if you want to cancel the request.
    }
);
```

![progress](https://user-images.githubusercontent.com/236374/33138910-495c4ecc-cf86-11e7-8693-2fc6d09615c4.gif)

This feature was contributed by [underscorediscovery](https://github.com/yhirose/cpp-httplib/pull/23).

### Basic Authentication

```cpp
httplib::Client cli("httplib.org");

auto res = cli.Get("/basic-auth/hello/world", {
  httplib::make_basic_authentication_header("hello", "world")
});
// res->status should be 200
// res->body should be "{\n  \"authenticated\": true, \n  \"user\": \"hello\"\n}\n".
```

### Range

```cpp
httplib::Client cli("httpbin.org");

auto res = cli.Get("/range/32", {
  httplib::make_range_header(1, 10) // 'Range: bytes=1-10'
});
// res->status should be 206.
// res->body should be "bcdefghijk".
```

OpenSSL Support
---------------

SSL support is available with `CPPHTTPLIB_OPENSSL_SUPPORT`. `libssl` and `libcrypto` should be linked.

```c++
#define CPPHTTPLIB_OPENSSL_SUPPORT

SSLServer svr("./cert.pem", "./key.pem");

SSLClient cli("localhost", 8080);
cli.set_ca_cert_path("./ca-bundle.crt");
cli.enable_server_certificate_verification(true);
```

Zlib Support
------------

'gzip' compression is available with `CPPHTTPLIB_ZLIB_SUPPORT`.

The server applies gzip compression to the following MIME type contents:

  * all text types
  * image/svg+xml
  * application/javascript
  * application/json
  * application/xml
  * application/xhtml+xml

NOTE
----

g++ 4.8 cannot build this library since `<regex>` in g++4.8 is [broken](https://stackoverflow.com/questions/12530406/is-gcc-4-8-or-earlier-buggy-about-regular-expressions).

License
-------

MIT license (© 2019 Yuji Hirose)
