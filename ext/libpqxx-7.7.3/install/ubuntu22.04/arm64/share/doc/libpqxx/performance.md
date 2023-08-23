Performance features                       {#performance}
====================

If your program's database interaction is not as efficient as it needs to be,
the first place to look is usually the SQL you're executing.  But libpqxx
has a few specialized features to help you squeeze more performance out
of how you issue commands and retrieve data:

* @ref streams.  Use these as a faster way to transfer data between your
    code and the database.
* `std::string_view` and `pqxx::zview`.  In places where traditional C++ worked
    with `std::string`, see whether `std::string_view` or `pqxx::zview` will
    do.  Of course that means that you'll have to look at the data's lifetime
    more carefully, but it'll save the computer a lot of copying.
* @ref prepared.  These can be executed many times without the server
    parsing and planning them anew each time.  They also save you having to
    escape string parameters.
* `pqxx::pipeline` lets you send queries to the database in batches, and
    continue other processing while they are executing.
* `pqxx::connecting` lets you start setting up a database connection, but
    without blocking the thread.

As always of course, don't risk the quality of your code for optimizations
that you don't need!
