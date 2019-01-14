/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2019  ZeroTier, Inc.  https://www.zerotier.com/
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial closed-source software that incorporates or links
 * directly against ZeroTier software without disclosing the source code
 * of your own application.
 */

#ifndef ZT_POLY1305_HPP
#define ZT_POLY1305_HPP

namespace ZeroTier {

#define ZT_POLY1305_KEY_LEN 32
#define ZT_POLY1305_MAC_LEN 16

/**
 * Poly1305 one-time authentication code
 *
 * This takes a one-time-use 32-byte key and generates a 16-byte message
 * authentication code. The key must never be re-used for a different
 * message.
 *
 * In Packet this is done by using the first 32 bytes of the stream cipher
 * keystream as a one-time-use key. These 32 bytes are then discarded and
 * the packet is encrypted with the next N bytes.
 */
class Poly1305
{
public:
	/**
	 * Compute a one-time authentication code
	 *
	 * @param auth Buffer to receive code -- MUST be 16 bytes in length
	 * @param data Data to authenticate
	 * @param len Length of data to authenticate in bytes
	 * @param key 32-byte one-time use key to authenticate data (must not be reused)
	 */
	static void compute(void *auth,const void *data,unsigned int len,const void *key);
};

} // namespace ZeroTier

#endif
