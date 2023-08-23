Streams						{#streams}
=======

Most of the time it's fine to retrieve data from the database using `SELECT`
queries, and store data using `INSERT`.  But for those cases where efficiency
matters, there are two classes to help you do this better: `stream_from` and
`stream_to`.  They're less flexible than SQL queries, and there's the risk of
losing your connection while you're in mid-stream, but you get some speed and
memory efficiencies in return.

Both stream classes do data conversion for you: `stream_from` receives values
from the database in PostgreSQL's text format, and converts them to the C++
types you specify.  Likewise, `stream_to` converts C++ values you provide to
PostgreSQL's text format for transfer.  (On its end, the database of course
converts values to and from their SQL types.)


Null values
-----------

So how do you deal with nulls?  It depends on the C++ type you're using.  Some
types may have a built-in null value.  For instance, if you have a
`char const *` value and you convert it to an SQL string, then converting a
`nullptr` will produce a NULL SQL value.

But what do you do about C++ types which don't have a built-in null value, such
as `int`?  The trick is to wrap it in `std::optional`.  The difference between
`int` and `std::optional<int>` is that the former always has an `int` value,
and the latter doesn't have to.

Actually it's not just `std::optional`.  You can do the same thing with
`std::unique_ptr` or `std::shared_ptr`.  A smart pointer is less efficient than
`std::optional` in most situations because they allocate their value on the
heap, but sometimes that's what you want in order to save moving or copying
large values around.

This part is not generic though.  It won't work with just any smart-pointer
type, just the ones which are explicitly supported: `shared_ptr` and
`unique_ptr`.  If you really need to, you can build support for additional
wrappers and smart pointers by copying the implementation patterns from the
existing smart-pointer support.


stream\_from
------------

Use `stream_from` to read data directly from the database.  It's faster than
the transaction's `exec` functions if the result contains enough rows.  But
also, you won't need to keep your full result set in memory.  That can really
matter with larger data sets.

And, you can start processing your data right after the first row of data comes
in from the server.  With `exec()` you need to wait to receive all data, and
then you begin processing.  With `stream_from` you can be processing data on
the client side while the server is still sending you the rest.

You don't actually need to create a `stream_from` object yourself, though you
can.  Two shorthand functions, @ref pqxx::transaction_base::stream
and @ref pqxx::transaction_base::for_each, can create the streams for you with
a minimum of overhead.

Not all kinds of queries will work in a stream.  Internally the streams make
use of PostgreSQL's `COPY` command, so see the PostgreSQL documentation for
`COPY` for the exact limitations.  Basic `SELECT` and `UPDATE ... RETURNING`
queries should just work.

As you read a row, the stream converts its fields to a tuple type containing
the value types you ask for:

    auto stream pqxx::stream_from::query(
        tx, "SELECT name, points FROM score");
    std::tuple<std::string, int> row;
    while (stream >> row)
      process(row);
    stream.complete();

As the stream reads each row, it converts that row's data into your tuple,
goes through your loop body, and then promptly forgets that row's data.  This
means you can easily process more data than will fit in memory.


stream\_to
----------

Use `stream_to` to write data directly to a database table.  This saves you
having to perform an `INSERT` for every row, and so it can be significantly
faster if you want to insert more than just one or two rows at a time.

As with `stream_from`, you can specify the table and the columns, and not much
else.  You insert tuple-like objects of your choice:

    pqxx::stream_to stream{
        tx,
        "score",
        std::vector<std::string>{"name", "points"}};
    for (auto const &entry: scores)
        stream << entry;
    stream.complete();

Each row is processed as you provide it, and not retained in memory after that.

The call to `complete()` is more important here than it is for `stream_from`.
It's a lot like a "commit" or "abort" at the end of a transaction.  If you omit
it, it will be done automatically during the stream's destructor.  But since
destructors can't throw exceptions, any failures at that stage won't be visible
in your code.  So, always call `complete()` on a `stream_to` to close it off
properly!
