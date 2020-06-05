ZeroTier Network Hypervisor Core
======

This directory contains the *real* ZeroTier: a completely OS-independent global virtual Ethernet switch engine. This is where the magic happens.

Give it wire packets and it gives you Ethernet packets, and vice versa. The core contains absolutely no actual I/O, port configuration, or other OS-specific code (except Utils::getSecureRandom()). It provides a simple C API via [/include/ZeroTierOne.h](../include/ZeroTierOne.h). It's designed to be small and maximally portable for future use on small embedded and special purpose systems.

Code in here follows these guidelines:

 - Keep it minimal, especially in terms of code footprint and memory use.
 - There should be no OS-dependent code here unless absolutely necessary (e.g. getSecureRandom).
 - If it's not part of the core virtual Ethernet switch it does not belong here.
 - Minimize the use of complex C++ features since at some point we might end up "minus-minus'ing" this code if doing so proves necessary to port to tiny embedded systems.
