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

#ifndef ZT_MACETHERNETTAPAGENT_H
#define ZT_MACETHERNETTAPAGENT_H

#define ZT_MACETHERNETTAPAGENT_EXIT_CODE_SUCCESS 0
#define ZT_MACETHERNETTAPAGENT_EXIT_CODE_INVALID_REQUEST -1
#define ZT_MACETHERNETTAPAGENT_EXIT_CODE_UNABLE_TO_CREATE -2
#define ZT_MACETHERNETTAPAGENT_EXIT_CODE_READ_ERROR -3

#define ZT_MACETHERNETTAPAGENT_STDIN_CMD_PACKET 0
#define ZT_MACETHERNETTAPAGENT_STDIN_CMD_IFCONFIG 1
#define ZT_MACETHERNETTAPAGENT_STDIN_CMD_EXIT 2

#define ZT_MACETHERNETTAPAGENT_DEFAULT_SYSTEM_PATH "/Library/Application Support/ZeroTier/One/MacEthernetTapAgent"

#endif
