This folder contains two spec files which enable building of various RPM packages for ZeroTier.

#zerotier-one.spec.in
This file contains the information to build an RPM from the bash based binary installer of ZeroTier. The resulting RPM cannot be recompiled to different architectures.

#zerotier.spec
This spec file is a “standard” RPM spec file. It fits to the common rpmbuild process, SRPM and differnt architectures are supported too. The spec file can be used to build two packages: the standard zerotier and the zerotier-controller. It supports some of the build options exposed in the original Linux makefile:

> `rpmbuild -ba zerotier.spec` #builds the standard zerotier package, this is what you need in most of the cases

> `rpmbuild -ba zerotier.spec --with controller` #builds the zerotier-controller package

> `rpmbuild -ba zerotier.spec --with debug` #builds the zerotier package with debug enable<>d

> `rpmbuild -ba zerotier.spec --with miniupnpc` #builds the zerotier package with miniupnpc enabled

> `rpmbuild -ba zerotier.spec --with cluster` #builds the zerotier package with cluster enabled


####Build environment preparation
As zerotier is not distributed in tar.gz format at the moment, the %prep section of the spec file takes care about the prepartion of an rpmbuild compatible tar.gz.



