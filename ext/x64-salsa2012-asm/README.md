Blazingly fast X64 ASM implementation of Salsa20/12
======

This is ripped from the [cnacl](https://github.com/cjdelisle/cnacl) source. The actual code is by Danial J. Bernstein and is in the public domain.

This is included on Linux and Mac 64-bit builds and is significantly faster than the SSE intrinsics or C versions. It's used for packet encode/decode only since its use differs a bit from the regular Salsa20 C++ class. Specifically it lacks the ability to be called on multiple blocks, preferring instead to take a key and a single stream to encrypt and that's it.
