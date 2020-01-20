# Authors and Third Party Code Licensing Information

## Primary Authors

 * ZeroTier Core and ZeroTier One virtual networking service<br>
   Adam Ierymenko / adam.ierymenko@zerotier.com
   Joseph Henry / joseph.henry@zerotier.com (QoS and multipath)

 * Java JNI Interface to enable Android application development, and Android app itself (code for that is elsewhere)<br>
   Grant Limberg / glimberg@gmail.com

 * ZeroTier SDK / libzt<br>
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

ZeroTier includes the following third party code, either in ext/ or incorporated into the ZeroTier core. This third party code remains licensed under its original license and is not subject to ZeroTier's BSL license.

 * LZ4 compression algorithm by Yann Collet

   * Files: node/LZ4.cpp
   * Home page: http://code.google.com/p/lz4/
   * License: BSD 2-clause

 * C++11 json (nlohmann/json) by Niels Lohmann

   * Files: ext/json/*
   * Home page: https://github.com/nlohmann/json
   * License: MIT

 * tap-windows6 by the OpenVPN project

   * Files: windows/TapDriver6/*
   * Home page: https://github.com/OpenVPN/tap-windows6/
   * License: GNU GPL v2

 * Salsa20 stream cipher, Curve25519 elliptic curve cipher, Ed25519 digital signature algorithm, and Poly1305 MAC algorithm, all by Daniel J. Bernstein

   * Files: node/Salsa20.* node/C25519.* node/Poly1305.*
   * Home page: http://cr.yp.to/
   * License: public domain

 * cpp-httplib by yhirose

   * Files: ext/cpp-httplib/*
   * Home page: https://github.com/yhirose/cpp-httplib
   * License: MIT
