Accessing results and result rows                   {#accessing-results}
---------------------------------

When you execute a query using one of the transaction `exec` functions, you
normally get a `result` object back.  A `result` is a container of `row`s.

(There are exceptions.  The `exec1` functions expect exactly one row of data,
so they return just a `row`, not a full `result`.)

Result objects are an all-or-nothing affair.  The `exec` function waits until
it's received all the result data, and then gives it to you in the form of the
`result`.  _(There is a faster, easier way of executing simple queries, so see
"streaming rows" below as well.)_

For example, your code might do:

```cxx
    pqxx::result r = tx.exec("SELECT * FROM mytable");
```

Now, how do you access the data inside `r`?

Result sets act as standard C++ containers of rows.  Rows act as standard
C++ containers of fields.  So the easiest way to go through them is:

```cxx
    for (auto const &row: r)
    {
       for (auto const &field: row) std::cout << field.c_str() << '\t';
       std::cout << '\n';
    }
```

But results and rows also support other kinds of access.  Array-style
indexing, for instance, such as `r[rownum]`:

```cxx
    std::size_t const num_rows = std::size(r);
    for (std::size_t rownum=0u; rownum < num_rows; ++rownum)
    {
      pqxx::row const row = r[rownum];
      std::size_t const num_cols = std::size(row);
      for (std::size_t colnum=0u; colnum < num_cols; ++colnum)
      {
        pqxx::field const field = row[colnum];
        std::cout << field.c_str() << '\t';
      }

      std::cout << '\n';
    }
```

Every row in the result has the same number of columns, so you don't need to
look up the number of fields again for each one:

```cxx
    std::size_t const num_rows = std::size(r);
    std::size_t const num_cols = r.columns();
    for (std::size_t rownum=0u; rownum < num_rows; ++rownum)
    {
      pqxx::row const row = r[rownum];
      for (std::size_t colnum=0u; colnum < num_cols; ++colnum)
      {
        pqxx::field const field = row[colnum];
        std::cout << field.c_str() << '\t';
      }

      std::cout << '\n';
    }
```

You can even address a field by indexing the `row` using the field's _name:_

```cxx
    std::cout << row["salary"] << '\n';
```

But try not to do that if speed matters, because looking up the column by name
takes time.  At least you'd want to look up the column index before your loop
and then use numerical indexes inside the loop.

For C++23 or better, there's also a two-dimensional array access operator:

```cxx
    for (std::size_t rownum=0u; rownum < num_rows; ++rownum)
    {
        for (std::size_t colnum=0u; colnum < num_cols; ++colnum)
	    std::cout result[rownum, colnum].c_str() << '\t';
	std::cout << '\n';
    }
```

And of course you can use classic "begin/end" loops:

```cxx
    for (auto row = std::begin(r); row != std::end(r); row++)
    {
      for (auto field = std::begin(row); field != std::end(row); field++)
        std::cout << field->c_str() << '\t';
      std::cout << '\n';
    }
```

Result sets are immutable, so all iterators on results and rows are actually
`const_iterator`s.  There are also `const_reverse_iterator` types, which
iterate backwards from `rbegin()` to `rend()` exclusive.

All these iterator types provide one extra bit of convenience that you won't
normally find in C++ iterators: referential transparency.  You don't need to
dereference them to get to the row or field they refer to.  That is, instead
of `row->end()` you can also choose to say `row.end()`.  Similarly, you
may prefer `field.c_str()` over `field->c_str()`.

This becomes really helpful with the array-indexing operator.  With regular
C++ iterators you would need ugly expressions like `(*row)[0]` or
`row->operator[](0)`.  With the iterator types defined by the result and
row classes you can simply say `row[0]`.


Streaming rows
--------------

There's another way to go through the rows coming out of a query.  It's
usually easier and faster, but there are drawbacks.

**One,** you start getting rows before all the data has come in from the
database.  That speeds things up, but what happens if you lose your network
connection while transferring the data?  Your application may already have
processed some of the data before finding out that the rest isn't coming.  If
that is a problem for your application, streaming may not be the right choice.

**Two,** streaming only works for some types of query.  The `stream()` function
wraps your query in a PostgreSQL `COPY` command, and `COPY` only supports a few
commands: `SELECT`, `VALUES`, `or an `INSERT`, `UPDATE`, or `DELETE` with a
`RETURNING` clause.  See the `COPY` documentation here:
https://www.postgresql.org/docs/current/sql-copy.html

**Three,** when you convert a field to a "view" type (such as
`std::string_view` or `std::basic_string_view<std::byte>`), the view points to
underlying data which only stays valid until you iterate to the next row or
exit the loop.  So if you want to use that data for longer than a single
iteration of the streaming loop, you'll have to store it somewhere yourself.

Now for the good news.  Streaming does make it very easy to query data and loop
over it:

```cxx
    for (auto [id, name, x, y] :
        tx.stream<int, std::string_view, float, float>(
            "SELECT id, name, x, y FROM point"))
      process(id + 1, "point-" + name, x * 10.0, y * 10.0);
```

The conversion to C++ types (here `int`, `std::string_view`, and two `float`s)
is built into the function.  You never even see `row` objects, `field` objects,
iterators, or conversion methods.  You just put in your query and you receive
your data.
