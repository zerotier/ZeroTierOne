/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
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


/* Maximum number of socket connections allowed in service */
#define MAX_SOCKETS               1024
/* Maximum number of harnesses allowed to be connected to this service */
#define MAX_INTERCEPTS            256

#define SELECT_PERF_WARNING       1000

#define CMD_SIZE                  32
#define DATA_IDX                  CMD_SIZE
#define CMD_IDX                   0
#define BUF_SZ                    1024

/* number or times an intercept will attempt to connect to the service */
#define SERVICE_CONNECT_ATTEMPTS  3

/* how long we wait when polling various I/O sources in the core I/O loop */
#define INTERCEPT_POLL_TIMEOUT    0

/* how often we want lwIP to poll our callback.
 - We use this to resume pending data transfers */
#define APPLICATION_POLL_FREQ     1

/* for passing applications as arguments to the service */
#define APP_ARGUMENT_LENGTH       128


#define DEFAULT_RPC_PIPE_IDX      0

/* display various types of detectable lwip/service/intercept problems */
#define NETCON_WARNINGS           1

typedef int rpc_len_type;
