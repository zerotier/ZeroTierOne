Thread safety                    {#thread-safety}
=============

This library does not contain any locking code to protect objects against
simultaneous modification in multi-threaded programs.  Therefore it is up
to you, the user of the library, to ensure that your threaded client
programs perform no conflicting operations concurrently.

Most of the time this isn't hard.  Result sets are immutable, so you can
share them between threads without problem.  The main rule is:

@li Treat a connection, together with any and all objects related to it, as
a "world" of its own.  You should generally make sure that the same "world"
is never accessed by another thread while you're doing anything non-const
in there.

That means: don't issue a query on a transaction while you're also opening
a subtransaction, don't access a cursor while you may also be committing,
and so on.

In particular, cursors are tricky.  It's easy to perform a non-const
operation without noticing.  So, if you're going to share cursors or
cursor-related objects between threads, lock very conservatively!

Use `pqxx::describe_thread_safety` to find out at runtime what level of
thread safety is implemented in your build and version of libpqxx.  It
returns a `pqxx::thread_safety_model` describing what you can and cannot rely
on.  A command-line utility `tools/pqxxthreadsafety` prints out the same
information.
