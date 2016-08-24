## Primary Authors

 * ZeroTier Core and ZeroTier One virtual networking service<br>
   Adam Ierymenko / adam.ierymenko@zerotier.com

 * Java JNI Interface to enable Android application development, and Android app itself (code for that is elsewhere)<br>
   Grant Limberg / glimberg@gmail.com

 * ZeroTier SDK (formerly known as Network Containers)<br>
   Joseph Henry / joseph.henry@zerotier.com

## Third Party Contributors

 * A number of fixes and improvements to the new controller, other stuff.<br>
   Kees Bos / https://github.com/keesbos/

 * Debugging and testing, OpenWRT support fixes.<br>
   Moritz Warning / moritzwarning@web.de

 * Debian GNU/Linux packaging, manual pages, and license compliance edits.<br>
   Ben Finney <ben+zerotier@benfinney.id.au>

 * Several others made smaller contributions, which GitHub tracks here:<br>
   https://github.com/zerotier/ZeroTierOne/graphs/contributors/

## Third-Party Code

These are included in ext/ for platforms that do not have them available in common repositories. Otherwise they may be linked and the package may ship with them as dependencies.

 * LZ4 compression algorithm by Yann Collet

   * Files: ext/lz4/*
   * Home page: http://code.google.com/p/lz4/
   * License grant: BSD attribution

 * http-parser by Joyent, Inc. (many authors)

   * Files: ext/http-parser/*
   * Home page: https://github.com/joyent/http-parser/
   * License grant: MIT/Expat

 * C++11 json (nlohmann/json) by Niels Lohmann

   * Files: ext/json/*
   * Home page: https://github.com/nlohmann/json
   * License grant: MIT

 * TunTapOSX by Mattias Nissler

   * Files: ext/tap-mac/tuntap/*
   * Home page: http://tuntaposx.sourceforge.net/
   * License grant: BSD attribution no-endorsement
   * ZeroTier Modifications: change interface name to zt#, increase max MTU, increase max devices

 * tap-windows6 by the OpenVPN project

   * Files: windows/TapDriver6/*
   * Home page:
       https://github.com/OpenVPN/tap-windows6/
   * License grant: GNU GPL v2
   * ZeroTier Modifications: change name of driver to ZeroTier, add ioctl() to get L2 multicast memberships (source is in ext/ and modifications inherit GPL)

 * Salsa20 stream cipher, Curve25519 elliptic curve cipher, Ed25519
   digital signature algorithm, and Poly1305 MAC algorithm, all by
   Daniel J. Bernstein

   * Files:
       node/Salsa20.hpp
       node/C25519.hpp
       node/Poly1305.hpp
   * Home page: http://cr.yp.to/
   * License grant: public domain

 * MiniUPNPC and libnatpmp by Thomas Bernard

   * Files:
       ext/libnatpmp/*
       ext/miniupnpc/*
   * Home page: http://miniupnp.free.fr/
   * License grant: BSD attribution no-endorsement
