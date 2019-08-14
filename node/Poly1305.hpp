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
 * Compute a one-time authentication code
 *
 * @param auth Buffer to receive code -- MUST be 16 bytes in length
 * @param data Data to authenticate
 * @param len Length of data to authenticate in bytes
 * @param key 32-byte one-time use key to authenticate data (must not be reused)
 */
void poly1305(void *auth,const void *data,unsigned int len,const void *key);

} // namespace ZeroTier

#endif
