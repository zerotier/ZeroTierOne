Prepared statements                    {#prepared}
===================

Prepared statements are SQL queries that you define once and then invoke
as many times as you like, typically with varying parameters.  It's basically
a function that you can define ad hoc.

If you have an SQL statement that you're going to execute many times in
quick succession, it may be more efficient to prepare it once and reuse it.
This saves the database backend the effort of parsing complex SQL and
figuring out an efficient execution plan.  Another nice side effect is that
you don't need to worry about escaping parameters.  Some corporate coding
standards require all SQL parameters to be passed in this way, to reduce the
risk of programmer mistakes leaving room for SQL injections.


Preparing a statement
---------------------

You create a prepared statement by preparing it on the connection (using the
`pqxx::connection::prepare` functions), passing an identifier and its SQL text.

The identifier is the name by which the prepared statement will be known; it
should consist of ASCII letters, digits, and underscores only, and start with
an ASCII letter.  The name is case-sensitive.

```cxx
    void prepare_my_statement(pqxx::connection &c)
    {
      c.prepare(
          "my_statement",
          "SELECT * FROM Employee WHERE name = 'Xavier'");
    }
```

Once you've done this, you'll be able to call `my_statement` from any
transaction you execute on the same connection.  For this, use the
`pqxx::transaction_base::exec_prepared` functions.

```cxx
    pqxx::result execute_my_statement(pqxx::transaction_base &t)
    {
      return t.exec_prepared("my_statement");
    }
```


Parameters
----------

Did I mention that prepared statements can have parameters?  The query text
can contain `$1`, `$2` etc. as placeholders for parameter values that you
will provide when you invoke the prepared satement.

See @ref parameters for more about this.  And here's a simple example of
preparing a statement and invoking it with parameters:

```cxx
    void prepare_find(pqxx::connection &c)
    {
      // Prepare a statement called "find" that looks for employees with a
      // given name (parameter 1) whose salary exceeds a given number
      // (parameter 2).
      c.prepare(
  	    "find",
  	    "SELECT * FROM Employee WHERE name = $1 AND salary > $2");
    }
```

This example looks up the prepared statement "find," passes `name` and
`min_salary` as parameters, and invokes the statement with those values:

```cxx
    pqxx::result execute_find(
      pqxx::transaction_base &t, std::string name, int min_salary)
    {
      return t.exec_prepared("find", name, min_salary);
    }
```


A special prepared statement
----------------------------

There is one special case: the _nameless_ prepared statement.  You may prepare
a statement without a name, i.e. whose name is an empty string.  The unnamed
statement can be redefined at any time, without un-preparing it first.


Performance note
----------------

Don't assume that using prepared statements will speed up your application.
There are cases where prepared statements are actually slower than plain SQL.

The reason is that the backend can often produce a better execution plan when
it knows the statement's actual parameter values.

For example, say you've got a web application and you're querying for users
with status "inactive" who have email addresses in a given domain name X.  If
X is a very popular provider, the best way for the database engine to plan the
query may be to list the inactive users first and then filter for the email
addresses you're looking for.  But in other cases, it may be much faster to
find matching email addresses first and then see which of their owners are
"inactive."  A prepared statement must be planned to fit either case, but a
direct query will be optimised based on table statistics, partial indexes, etc.


Zero bytes
----------

@warning Beware of "nul" bytes!

Any string you pass as a parameter will end at the _first char with value
zero._  If you pass a string that contains a zero byte, the last byte in the
value will be the one just before the zero.

So, if you need a zero byte in a string, consider that it's really a _binary
string,_ which is not the same thing as a text string.  SQL represents binary
data as the `BYTEA` type, or in binary large objects ("blobs").

In libpqxx, you represent binary data as a range of `std::byte`.  They must be
contiguous in memory, so that libpqxx can pass pointers to the underlying C
library.  So you might use `std::basic_string<std::byte>`, or
`std::basic_string_view<std::byte>`, or `std::vector<std::byte>`.
