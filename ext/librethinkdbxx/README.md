# RethinkDB driver for C++

This driver is compatible with RethinkDB 2.0. It is based on the
official RethinkDB Python driver.

* [RethinkDB server](http://rethinkdb.com/)
* [RethinkDB API docs](http://rethinkdb.com/api/python/)

## Example

```
#include <memory>
#include <cstdio>
#include <rethinkdb.h>

namespace R = RethinkDB;

int main() {
  std::unique_ptr<R::Connection> conn = R::connect("localhost", 28015);
  R::Cursor cursor = R::table("users").filter(R::row["age"] > 14).run(*conn);
  for (R::Datum& user : cursor) {
      printf("%s\n", user.as_json().c_str());
  }
}
```

## Build

Requires a modern C++ compiler. to build and install, run:

```
make
make install
```

Will build `include/rethinkdb.h`, `librethinkdb++.a` and
`librethinkdb++.so` into the `build/` directory.

To include documentation from the Python driver in the header file,
pass the following argument to make.

```
make INCLUDE_PYTHON_DOCS=yes
```

To build in debug mode:

```
make DEBUG=yes
```

To install to a specific location:

```
make install prefix=/usr/local DESTDIR=
```

## Status

Still in early stages of development.

## Tests

This driver is tested against the upstream ReQL tests from the
RethinkDB repo, which are programmatically translated from Python to
C++. As of 34dc13c, all tests pass:

```
$ make test
...
SUCCESS: 2053 tests passed
```
