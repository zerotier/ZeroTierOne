Configuration tests
===================

Libpqxx comes with support for different build systems: the GNU autotools,
CMake, Visual Studio's "nmake", and raw GNU "make" on Windows.

For several of these build systems, we need to test things like "does this
compiler environment support `std::to_chars` for floating-point types?"

We test these things by trying to compile a particular snippet of code, and
seeing whether that succeeds.

To avoid duplicating those snippets for multiple build systems, we put them
here.  Both the autotools configuration and the CMake configuration can refer to
them that way.

It took a bit of nasty magic to read a C++ source file into m4 and treat it as
a string literal, without macro expansion.  There is every chance that I missed
something, so be prepared for tests failing for unexpected reasons!  Some C++
syntax may end up having an unforeseen meaning in m4, and screw up the handling
of the code snippet.  Re-configure, and read your logs carefully after editing
these snippets.
