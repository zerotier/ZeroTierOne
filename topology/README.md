This folder contains the source files to compile the signed network topology dictionary.
Users outside ZeroTier won't find this useful except for documentation purposes, since
this dictionary must be signed by a valid topology signing key to be considered valid. These
keys are hard-coded into the source and distributed with all versions of the app.

A default value for this dictionary is included in node/Defaults.cpp, and the following
URL is periodically checked for updates:

http://download.zerotier.com/sys/topology
