# ZeroTier Cryptography Library

------

Most of this library is just glue to provide a simple safe API around things like OpenSSL or OS-specific crypto APIs.

It is very important that this library is only linked to OpenSSL versions greater than 1.1.0. 1.1.0 introduced no-hassle threadsafety which we take advantage of. If we want a version prior to 1.1.0 we will have to add conditional threadsafety code.
