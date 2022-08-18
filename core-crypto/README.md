# ZeroTier Core Cryptography Library

------

This is mostly just glue to provide a simple consistent API in front of OpenSSL and some platform-specific crypto APIs.

It's thin and simple enough that we can easily create variants of it in the future for e.g. if we need to support some proprietary FIPS module or something.

It also contains a few utilities and helper functions.
