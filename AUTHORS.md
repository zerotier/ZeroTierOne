## Primary Authors

 * ZeroTier protocol design and core network virtualization engine, ZeroTier One service, React web UI, packaging for most platforms, kitchen sink...<br>
   Adam Ierymenko / adam.ierymenko@zerotier.com

 * Java JNI Interface to enable Android application development, and Android app itself (code for that is elsewhere)<br>
   Grant Limberg / glimberg@gmail.com

 * Network Containers for Linux, iOS, Android<br>
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

 * LZ4 compression algorithm by Yann Collet

   * Files: ext/lz4/*
   * Home page: http://code.google.com/p/lz4/
   * License grant: BSD attribution

 * http-parser by Joyent, Inc. (many authors)

   * Files: ext/http-parser/*
   * Home page: https://github.com/joyent/http-parser/
   * License grant: MIT/Expat

 * json-parser by James McLaughlin

   * Files: ext/json-parser/*
   * Home page: https://github.com/udp/json-parser/
   * License grant: BSD attribution

 * TunTapOSX by Mattias Nissler

   * Files: ext/tap-mac/tuntap/*
   * Home page: http://tuntaposx.sourceforge.net/
   * License grant: BSD attribution no-endorsement

 * tap-windows and tap-windows6 by the OpenVPN project

   * Files: windows/TapDriver6/*
   * Home page:
       https://github.com/OpenVPN/tap-windows/
       https://github.com/OpenVPN/tap-windows6/
   * License grant: GNU GPL v2

 * Salsa20 stream cipher, Curve25519 elliptic curve cipher, Ed25519
   digital signature algorithm, and Poly1305 MAC algorithm, all by
   Daniel J. Bernstein

   * Files:
       node/Salsa20.hpp
       node/C25519.hpp
       node/Poly1305.hpp
   * Home page: http://cr.yp.to/
   * License grant: public domain

 * MiniUPNPC by Thomas Bernard

   * Files:
       ext/libnatpmp/*
       ext/miniupnpc/*
   * Home page: http://miniupnp.free.fr/
   * License grant: BSD attribution no-endorsement
