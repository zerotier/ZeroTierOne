ZeroTier Secure Socket Protocol
======

ZSSP (ZeroTier Secure Socket Protocol) is an implementation of the Noise_IK pattern using FIPS/NIST compliant primitives. After Noise_IK negotiation is complete ZSSP also adds key ratcheting and optional (enabled by default) support for quantum data forward secrecy with Kyber1024.

It's general purpose and could be used with any system but contains a few specific design choices to make it optimal for ZeroTier and easy to distinguish from legacy ZeroTier V1 traffic for backward compatibility.
