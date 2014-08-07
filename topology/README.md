This folder contains the source files to compile the signed network topology dictionary. Users outside ZeroTier won't find this useful except for documentation purposes, since this dictionary must be signed by a valid topology signing key to be considered valid. These keys are hard-coded into the source and distributed with all versions of the app.

A default value for this dictionary is included in node/Defaults.cpp, and the following URL is periodically checked for updates:

http://download.zerotier.com/sys/topology

Obviously nothing prevents OSS users from replacing this topology with their own, changing the hard coded topology signing identity and update URL in Defaults, and signing their own dictionary. But doing so would yield a network that would have a tough(ish) time talking to the main one. Since the main network is a free service, why bother? (Except for building testnets, which ZeroTier already does for internal testing.)

Increasing decentralization via federation or trust network is possible in the future though, provided it can be done without sacrificing stability, security, performance, or ease of use.
