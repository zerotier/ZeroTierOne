You are reading this file because you want to build a new copy of the LwIP library for
use in ZeroTier.

Subdirectories:

 ports/ -- contains ports for various architectures (for our purposes, unix)

In order for the Network Containers feature to work in ZeroTier, a copy of the LwIP libary
is needed since we dynamically load it into memory. You can build a new copy of the libary 
by going to /contrib/ports/unix/proj/lib and running make.

This will generate: liblwip.so

You can enable LwIP debug traces by adding the flag -DLWIP_DEBUG
See additional debug info here: http://lwip.wikia.com/wiki/Debugging_lwIP 
