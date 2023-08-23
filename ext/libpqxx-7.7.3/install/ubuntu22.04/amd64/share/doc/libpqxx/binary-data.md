Binary data                                                        {#binary}
===========

The database has two ways of storing binary data: `BYTEA` is like a string, but
containing bytes rather than text characters.  And _large objects_ are more
like a separate table containing binary objects.

Generally you'll want to use `BYTEA` for reasonably-sized values, and large
objects for very large values.

That's the database side.  On the C++ side, in libpqxx, all binary data must be
either `std::basic_string<std::byte>` or `std::basic_string_view<std::byte>`;
or if you're building in C++20 or better, anything that's a block of
contiguous `std::byte` in memory.

So for example, if you want to write a large object, you'd create a
`pqxx::blob` object.  And you might use that to write data in the form of
`std::basic_string_view<std::byte>`.

Your particular binary data may look different though.  You may have it in a
`std::string`, or a `std::vector<unsigned char>`, or a pointer to `char`
accompanied by a size (which could be signed or unsigned, and of any of a few
different widths).  Sometimes that's your choice, or sometimes some other
library will dictate what form it takes.

So long as it's _basically_ still a block of bytes though, you can use
`pqxx::binary_cast` to construct a `std::basic_string_view<std::byte>` from it.

There are two forms of `binary_cast`.  One takes a single argument that must
support `std::data()` and `std::size()`:

    std::string hi{"Hello binary world"};
    my_blob.write(pqxx::binary_cast(hi);

The other takes a pointer and a size:

    char const greeting[] = "Hello binary world";
    char const *hi = greeting;
    my_blob.write(pqxx::binary_cast(hi, sizeof(greeting)));


Caveats
-------

There are some restrictions on `binary_cast` that you must be aware of.

First, your data must of a type that gives us _bytes._  So: `char`,
`unsigned char`, `signed char`, `int8_t`, `uint8_t`, or of course `std::byte`.
You can't feed in a vector of `double`, or anything like that.

Second, the data must be laid out as a contiguous block in memory.  If there's
no `std::data()` implementation for your type, it's not suitable.

Third, `binary_cast` only constructs something like a `std::string_view`.  It
does not make a copy of your actual data.  So, make sure that your data remains
alive and in the same place while you're using it.
