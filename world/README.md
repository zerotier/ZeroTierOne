World Definitions and Generator Code
======

This little bit of code is used to generate world updates. Ordinary users probably will never need this unless they want to test or experiment.

See mkworld.cpp for documentation. To build from this directory:

    c++ -o mkworld ../node/C25519.cpp ../node/Salsa20.cpp ../node/SHA512.cpp ../node/Identity.cpp ../node/Utils.cpp ../node/InetAddress.cpp ../osdep/OSUtils.cpp mkworld.cpp

