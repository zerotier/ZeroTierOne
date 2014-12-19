Building ZeroTier One From Source
======

(See RUNNING.md for what to do next.)

Developers note: there is currently no management of dependencies on *nix
platforms, so you should make clean ; make if you change a header. Will
do this eventually.

### Linux and FreeBSD

Just type 'make'. You'll need gcc and g++ installed, but ZeroTier One requires
no other third party libraries beyond the standard libc, libstdc++, and libm.

### MacOS

make

If you are building ext/tap-mac you will need a different version of the
OSX gcc compiler chain than what currently ships (clang). We've got a copy
available here:

http://download.zerotier.com/dev/llvm-g++-Xcode4.6.2.tar.bz2

Un-tar this into ext/ (it's excluded in .gitignore) and then 'make' in
ext/tap-mac/tuntap/src/tap.

Most users should not need to build tap-mac, since a binary is included
in ext/bin.

To build the UI you will need Qt version 5.0 or later. The Qt home must
be symbolically linked into "Qt" in the parent directory of the ZeroTier
One source tree. Then you can type "make mac-ui" and the UI should build.
You can also load the UI in Qt Creator and build/test it that way.

### Windows

There's a Visual Studio 2012 solution file in windows/ that can be used.
I've never tried it with MinGW, but theoretically this should be possible.
