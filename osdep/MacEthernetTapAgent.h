/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

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
