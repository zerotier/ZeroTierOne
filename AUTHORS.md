## Primary Authors

 * ZeroTier Core and ZeroTier One virtual networking service<br>
   Adam Ierymenko / adam.ierymenko@zerotier.com
   Joseph Henry / joseph.henry@zerotier.com (QoS and multipath)

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

ZeroTier includes the following third party code, either in ext/ or incorporated into the ZeroTier core.

 * LZ4 compression algorithm by Yann Collet

   * Files: node/Packet.cpp (bundled within anonymous namespace)
   * Home page: http://code.google.com/p/lz4/
   * License grant: BSD 2-clause

 * http-parser by Joyent, Inc. (many authors)

   * Files: ext/http-parser/*
   * Home page: https://github.com/joyent/http-parser/
   * License grant: MIT/Expat

 * C++11 json (nlohmann/json) by Niels Lohmann

   * Files: ext/json/*
   * Home page: https://github.com/nlohmann/json
   * License grant: MIT

 * tap-windows6 by the OpenVPN project

   * Files: windows/TapDriver6/*
   * Home page: https://github.com/OpenVPN/tap-windows6/
   * License grant: GNU GPL v2
   * ZeroTier Modifications: change name of driver to ZeroTier, add ioctl() to get L2 multicast memberships (source is in ext/ and modifications inherit GPL)

 * Salsa20 stream cipher, Curve25519 elliptic curve cipher, Ed25519 digital signature algorithm, and Poly1305 MAC algorithm, all by Daniel J. Bernstein

   * Files: node/Salsa20.* node/C25519.* node/Poly1305.*
   * Home page: http://cr.yp.to/
   * License grant: public domain
   * ZeroTier Modifications: slight cryptographically-irrelevant modifications for inclusion into ZeroTier core

 * MiniUPNPC and libnatpmp by Thomas Bernard

   * Files: ext/libnatpmp/* ext/miniupnpc/*
   * Home page: http://miniupnp.free.fr/
   * License grant: BSD attribution no-endorsement
