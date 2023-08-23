Statement parameters                                        {#parameters}
====================

When you execute a prepared statement (see @ref prepared), or a parameterised
statement (using functions like `pqxx::connection::exec_params`), you may write
special _placeholders_ in the query text.  They look like `$1`, `$2`, and so
on.

If you execute the query and pass parameter values, the call will respectively
substitute the first where it finds `$1`, the second where it finds `$2`, et
cetera.

Doing this saves you work.  If you don't use statement parameters, you'll need
to quote and escape your values (see `connection::quote()` and friends) as you
insert them into your query as literal values.

Or if you forget to do that, you leave yourself open to horrible
[SQL injection attacks](https://xkcd.com/327/).  Trust me, I was born in a town
whose name started with an apostrophe!

Statement parameters save you this work.  With these parameters you can pass
your values as-is, and they will go across the wire to the database in a safe
format.

In some cases it may even be faster!  When a parameter represents binary data
(as in the SQL `BYTEA` type), libpqxx will send it directly as binary, which is
a bit more efficient.  If you insert the binary data directly in your query
text, your CPU will have some extra work to do, converting the data into a text
format, escaping it, and adding quotes.


Dynamic parameter lists
-----------------------

In rare cases you may just not know how many parameters you'll pass into your
statement when you call it.

For these situations, have a look at `params`.  It lets you compose your
parameters list on the fly, even add whole ranges of parameters at a time.

You can pass a `params` into your statement as a normal parameter.  It will
fill in all the parameter values it contains into that position of the
statement's overall parameter list.

So if you call your statement passing a regular parameter `a`, a
`params` containing just a parameter `b`, and another regular parameter `c`,
then your call will pass parameters `a`, `b`, and `c`.  Or if the params object
is empty, it will pass just `a` and `c`.  If the params object contains `x` and
`y`, your call will pass `a, x, y, c`.

You can mix static and dynamic parameters freely.  Don't go overboard though:
complexity is where bugs happen!


Generating placeholders
-----------------------

If your code gets particularly complex, it may sometimes happen that it becomes
hard to track which parameter value belongs with which placeholder.  Did you
intend to pass this numeric value as `$7`, or as `$8`?  The answer may depend
on an `if` that happened earlier in a different function.

(Generally if things get that complex, it's a good idea to look for simpler
solutions.  But especially when performance matters, sometimes you can't avoid
complexity like that.)

There's a little helper class called `placeholders`.  You can use it as a
counter which produces those placeholder strings, `$1`, `$2`, `$3`, et cetera.
When you start generating a complex statement, you can create both a `params`
and a `placeholders`:

    pqxx::params values;
    pqxx::placeholders name;

Let's say you've got some complex code to generate the conditions for an SQL
"WHERE" clause.  You'll generally want to do these things close together in
your, so that you don't accidentally update one part and forget another:

    if (extra_clause)
    {
      // Extend the query text, using the current placeholder.
      query += " AND x = " + name.get();
      // Add the parameter value.
      values.append(my_x);
      // Move on to the next placeholder value.
      name.next();
    }

Depending on the starting value of `name`, this might add to `query` a fragment
like "` AND x = $3`" or "` AND x = $5`".
