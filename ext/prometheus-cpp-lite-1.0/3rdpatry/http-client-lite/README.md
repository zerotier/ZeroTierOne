# HTTP Client lite: C++ Cross-platform library only from single-file header-only

This is a lite, C++ cross-platform header-only client library for http request based 
on [csachs/picohttpclient](https://github.com/csachs/picohttpclient) project.

A Lightweight HTTP 1.1 client where to quickly do very simple HTTP requests, 
without adding larger dependencies to a project.


## License

http client lite  is distributed under the [MIT License](https://github.com/john-jasper-doe/http-client-lite/blob/master/LICENSE).


## Example usage

To see how this can be used see the examples folders.


**Example:**
```C++
#include <jdl/httpclientlite.hpp>
...
using namespace jdl;
...
HTTPResponse response = HTTPClient::request(HTTPClient::GET, URI("http://example.com"));
cout << response.body << endl;
...
```

