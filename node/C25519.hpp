/*
 * ZeroTier One - Global Peer to Peer Ethernet
 * Copyright (C) 2012-2013  ZeroTier Networks LLC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

#ifndef _ZT_C25519_HPP
#define _ZT_C25519_HPP

#include "Array.hpp"

namespace ZeroTier {

/**
 * C25519 elliptic curve key agreement and signing
 */
class C25519
{
public:
	typedef Array<unsigned char,64> Public; // crypto key, signing key
	typedef Array<unsigned char,96> Private; // crypto key, signing key (64 bytes)
	typedef struct {
		Public pub;
		Private priv;
	} Pair;

	/**
	 * Generate a C25519 elliptic curve key pair
	 */
	static Pair generate();

	/**
	 * Perform C25519 ECC key agreement
	 *
	 * Actual key bytes are generated from one or more SHA-512 digests of
	 * the raw result of key agreement.
	 *
	 * @param mine My key pair including secret
	 * @param their Their public key
	 * @param keybuf Buffer to fill
	 * @param keylen Number of key bytes to generate
	 */
	static void agree(const Pair &mine,const Public &their,void *keybuf,unsigned int keylen);
};

} // namespace ZeroTier

#endif
