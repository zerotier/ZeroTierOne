ARM NEON (32-bit) ASM implementation of Salsa20/12
======

This is from [supercop](http://bench.cr.yp.to/supercop.html) and was originally written by Daniel J. Bernstein. Code is in the public domain like the rest of Salsa20. It's much faster than the naive implementation.

It's included automatically in 32-bit Linux ARM builds. It likely will not work on 64-bit ARM, so it'll need to be ported at least. That will unfortunately keep it out of mobile versions for now since those are all going 64-bit.
