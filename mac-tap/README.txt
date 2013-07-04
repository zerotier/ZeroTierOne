This is a hack of tuntaposx. It's here for two reasons:

1) There seem to be issues with large MTUs in the original tuntap code,
   so we set up our zt0 tap with the correct ZeroTier MTU as the default.

2) Lots of other mac products (VPNs, etc.) ship their own tap device
   drivers that like to conflict with one another. This gives us no
   choice but to play along. But we call our tap device zt0, which means
   it won't conflict with everyone else's tap0.

3) It's nice to call the device zt0, same as Linux, for consistency across
   *nix platforms. Mac does not seem to support interface renaming.

This will be placed in the ZeroTier home as a kext and is auto-loaded by the
ZeroTier One binary if /dev/zt0 is not found. It can also be auto-updated.

See this page for the original:

http://tuntaposx.sourceforge.net
