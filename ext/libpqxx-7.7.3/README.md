libpqxx
=======

Welcome to libpqxx, the C++ API to the PostgreSQL database management system.

Home page: http://pqxx.org/development/libpqxx/

Find libpqxx on Github: https://github.com/jtv/libpqxx

Documentation on Read The Docs: https://libpqxx.readthedocs.io

Compiling this package requires PostgreSQL to be installed -- or at least the C
headers and library for client development.  The library builds on top of
PostgreSQL's standard C API, libpq, though your code won't notice.

If you're getting the code straight from the Git repo, the head of the `master`
branch represents the current _development version._  Releases are tags on
commits in `master`.  For example, to get version 7.1.1:

    git checkout 7.1.1


Upgrade notes
-------------

**The 7.x versions require at least C++17.**  Make sure your compiler is up to
date.  For libpqxx 8.x you will need at least C++20.

Also, **7.0 makes some breaking changes in rarely used APIs:**
* There is just a single `connection` class.  It connects immediately.
* Custom `connection` classes are no longer supported.
* It's no longer possible to reactivate a connection once it's been closed.
* The API for defining string conversions has changed.

If you're defining your own type conversions, **7.1 requires one additional
field in your `nullness` traits.**


Building libpqxx
----------------

There are two different ways of building libpqxx from the command line:
1. Using CMake, on any system which supports it.
2. On Unix-like systems, using a `configure` script.

"Unix-like" systems include GNU/Linux, Apple macOS and the BSD family, AIX,
HP-UX, Irix, Solaris, etc.  Even on Microsoft Windows, a Unix-like environment
such as WSL, Cygwin, or MinGW should work.

You'll find detailed build and install instructions in `BUILDING-configure.md`
and `BUILDING-cmake.md`, respectively.

And if you're working with Microsoft Visual Studio, have a look at Gordon
Elliott's
[
  Easy-PQXX Build for Windows Visual Studio
](https://github.com/GordonLElliott/Easy-PQXX-Build-for-Windows-Visual-Studio)
project.


Documentation
-------------

Building the library, if you have the right tools installed, generates HTML
documentation in the `doc/` directory.  It is based on the headers in
`include/pqxx/` and text in `include/pqxx/doc/`.  This documentation is also
available online at [readthedocs](https://libpqxx.readthedocs.io).


Programming with libpqxx
------------------------

Your first program will involve the libpqxx classes `connection` (see the
`pqxx/connection.hxx` header), and `work` (a convenience alias for
`transaction<>` which conforms to the interface defined in
`pqxx/transaction_base.hxx`).

These `*.hxx` headers are not the ones you include in your program.  Instead,
include the versions without filename suffix (e.g. `pqxx/connection`).  Those
will include the actual .hxx files for you.  This was done so that includes are
in standard C++ style (as in `<iostream>` etc.), but an editor will still
recognize them as files containing C++ code.

Continuing the list of classes, you will most likely also need the result class
(`pqxx/result.hxx`).  In a nutshell, you create a `connection` based on a
Postgres connection string (see below), create a `work` in the context of that
connection, and run one or more queries on the work which return `result`
objects.  The results are containers of rows of data, each of which you can
treat as an array of strings: one for each field in the row.  It's that simple.

Here is a simple example program to get you going, with full error handling:

```c++
    #include <iostream>
    #include <pqxx/pqxx>

    int main()
    {
        try
        {
            // Connect to the database.
            pqxx::connection C;
            std::cout << "Connected to " << C.dbname() << '\n';

            // Start a transaction.
            pqxx::work W{C};

            // Perform a query and retrieve all results.
            pqxx::result R{W.exec("SELECT name FROM employee")};

            // Iterate over results.
            std::cout << "Found " << R.size() << "employees:\n";
            for (auto row: R)
                std::cout << row[0].c_str() << '\n';

            // Perform a query and check that it returns no result.
            std::cout << "Doubling all employees' salaries...\n";
            W.exec0("UPDATE employee SET salary = salary*2");

            // Commit the transaction.
            std::cout << "Making changes definite: ";
            W.commit();
            std::cout << "OK.\n";
        }
        catch (std::exception const &e)
        {
            std::cerr << e.what() << '\n';
            return 1;
        }
        return 0;
    }
```


Connection strings
------------------

Postgres connection strings state which database server you wish to connect to,
under which username, using which password, and so on.  Their format is defined
in the documentation for libpq, the C client interface for PostgreSQL.
Alternatively, these values may be defined by setting certain environment
variables as documented in e.g. the manual for psql, the command line interface
to PostgreSQL.  Again the definitions are the same for libpqxx-based programs.

The connection strings and variables are not fully and definitively documented
here; this document will tell you just enough to get going.  Check the
PostgreSQL documentation for authoritative information.

The connection string consists of attribute=value pairs separated by spaces,
e.g. "user=john password=1x2y3z4".  The valid attributes include:
* `host` —
  Name of server to connect to, or the full file path (beginning with a
  slash) to a Unix-domain socket on the local machine.  Defaults to
  "/tmp".  Equivalent to (but overrides) environment variable PGHOST.
* `hostaddr` —
  IP address of a server to connect to; mutually exclusive with "host".
* `port` —
  Port number at the server host to connect to, or socket file name
  extension for Unix-domain connections.  Equivalent to (but overrides)
  environment variable PGPORT.
* `dbname` —
  Name of the database to connect to.  A single server may host multiple
  databases.  Defaults to the same name as the current user's name.
  Equivalent to (but overrides) environment variable PGDATABASE.
* `user` —
  User name to connect under.  This defaults to the name of the current
  user, although PostgreSQL users are not necessarily the same thing as
  system users.
* `requiressl` —
  If set to 1, demands an encrypted SSL connection (and fails if no SSL
  connection can be created).

Settings in the connection strings override the environment variables, which in
turn override the default, on a variable-by-variable basis.  You only need to
define those variables that require non-default values.


Linking with libpqxx
--------------------

To link your final program, make sure you link to both the C-level libpq library
and the actual C++ library, libpqxx.  With most Unix-style compilers, you'd do
this using the options

    -lpqxx -lpq

while linking.  Both libraries must be in your link path, so the linker knows
where to find them.  Any dynamic libraries you use must also be in a place
where the loader can find them when loading your program at runtime.

Some users have reported problems using the above syntax, however, particularly
when multiple versions of libpqxx are partially or incorrectly installed on the
system.  If you get massive link errors, try removing the "-lpqxx" argument from
the command line and replacing it with the name of the libpqxx library binary
instead.  That's typically libpqxx.a, but you'll have to add the path to its
location as well, e.g. /usr/local/pqxx/lib/libpqxx.a.  This will ensure that the
linker will use that exact version of the library rather than one found
elsewhere on the system, and eliminate worries about the exact right version of
the library being installed with your program..
