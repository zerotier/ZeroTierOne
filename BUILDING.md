Building ZeroTier One from Source
======

(See RUNNING.md for what to do next.)

Developers note: there is currently no management of dependencies on *nix platforms, so you should 'make clean ; make' if you change a header (.h or .hpp) file. Need to fix eventually.

### Linux and BSD

Just type 'make' (or 'gmake' on BSD). You will need gcc/g++ or clang/clang++ installed. No other third party libraries are needed.

Very old versions of Linux or BSD may run into problems. On Linux our primary build container is CentOS 6 (for backward compatibility), and on FreeBSD we've only tested with FreeBSD 10.

### Macintosh

Just type 'make'. You'll need Xcode and the command line tools installed of course.

To install the tap driver, 'sudo make install-mac-tap' will place it in the default ZeroTier home directory. We ship a signed tap driver binary for ZeroTier One that should work for everyone.

If you do for some reason want to build ext/tap-mac you will need an older version of the OSX gcc compiler chain than what currently ships (clang) to build a properly backward compatibile image. A copy is hosted here:

http://download.zerotier.com/dev/llvm-g++-Xcode4.6.2.tar.bz2

Un-tar this into ext/ (it's excluded in .gitignore) and then 'make' in ext/tap-mac/tuntap/src/tap.

We really don't recommend messing with this build yourself unless you think there's a bug in the driver and want to try to fix it. So far the driver has been completely stable for a very long time and the old binaries continue to work flawlessly on (last we checked) 10.10.

### Windows

There's a Visual Studio 2012 solution file in windows/ that can be used. I've never tried it with MinGW, but theoretically this should be possible.

Windows is, of course, harder to build than any other platform.

### Debug builds

The Unix/Mac makefiles support a ZT_DEBUG=1 option ('make ZT_DEBUG=1') to build a version that will print a whole bunch of TRACE output to stderr as it runs. This also builds the binary with debug rather than production optimized flags.
