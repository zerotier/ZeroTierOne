This folder contains two spec files which enable building of various RPM packages for ZeroTier.

#zerotier-one.spec.in
This file contains the information to build an RPM from the bash based binary installer of ZeroTier. The resulting RPM cannot be recompiled to different architecture.

#zerotier.spec
This spec file is a “standard” RPM spec file which enables building an SRPM package from the ZeroTier sources. This makes possible to compile the software on different architectures in a standard way. The spec file supports two packages: the standard zerotier and the zerotier-controller too. It supports some of the build options exposed in the original Linux makefile:

> `rpmbuild -ba zerotier.spec` #builds the standard zerotier package, this is what you need in most of the cases

> `rpmbuild -ba zerotier.spec --with controller` #builds the zerotier-controller package

> `rpmbuild -ba zerotier.spec --with debug` #builds the zerotier package with debug enable<>d

> `rpmbuild -ba zerotier.spec --with miniupnpc` #builds the zerotier package with miniupnpc enabled

> `rpmbuild -ba zerotier.spec --with cluster` #builds the zerotier package with cluster enabled


####Build environment preparation
As zerotier is not distributed in tar.gz format at the moment, the %prep section of the spec file takes care about this.



