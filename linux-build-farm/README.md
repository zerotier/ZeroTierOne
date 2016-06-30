Dockerized Linux Build Farm
======

This subfolder contains Dockerfiles and a script to build Linux packages for a variety of Linux distributions. It's also an excellent way to test your CPU fans and stress test your disk.

Running `build.sh` with no arguments builds everything. You can run `build.sh` with the name of a distro (e.g. centos-7) to only build that. Both 32 and 64 bit packages are built except where no 32-bit version of the distribution exists.

The `aptly.sh` script creates Debian repositories with *aptly*. You will need it installed. Be careful if you have an existing *aptly* configuration since this may mess with it.
