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


#ifndef _INTERCEPT_H
#define _INTERCEPT_H	1

#include <sys/socket.h>

#define IDX_PID			0
#define IDX_TID			sizeof(pid_t)
#define IDX_COUNT		IDX_TID + sizeof(pid_t)
#define IDX_TIME		IDX_COUNT + sizeof(int)
#define IDX_PAYLOAD		IDX_TIME + 20 /* 20 being the length of the timestamp string */

#define BUF_SZ          256
#define PAYLOAD_SZ		223 /* BUF_SZ-IDX_PAYLOAD */

#define ERR_OK          0

/* Userland RPC codes */
#define RPC_UNDEFINED			 	0
#define RPC_CONNECT				 	1
#define RPC_CONNECT_SOCKARG		 	2
#define RPC_SELECT			 		3
#define RPC_POLL				 	4
#define RPC_CLOSE				 	5
#define RPC_READ				 	6
#define RPC_WRITE				 	7
#define RPC_BIND				 	8
#define RPC_ACCEPT			 		9
#define RPC_LISTEN			 		10
#define RPC_SOCKET			 		11
#define RPC_SHUTDOWN		 		12

/* Administration RPC codes */
#define RPC_MAP						20	/* Give the service the value we "see" for the new buffer fd */
#define RPC_MAP_REQ					21  /* A call to determine whether an fd is mapped to the service */
#define RPC_RETVAL					22	/* not RPC per se, but something we should codify */
#define RPC_KILL_INTERCEPT			23  /* Tells the service we need to shut down all connections */

/* Connection statuses */
#define UNSTARTED					0
#define CONNECTING			 		1
#define CONNECTED				 	2
#define SENDING					 	3
#define RECEIVING				 	4
#define SENTV4REQ				 	5
#define GOTV4REQ				 	6
#define SENTV5METHOD			 	7
#define GOTV5METHOD				 	8
#define SENTV5AUTH			 		9
#define GOTV5AUTH				 	10
#define SENTV5CONNECT			 	11
#define GOTV5CONNECT			 	12
#define DONE					 	13
#define FAILED				 		14

/* Flags to indicate what events a
   socket was select()ed for */
#define READ (POLLIN|POLLRDNORM)
#define WRITE (POLLOUT|POLLWRNORM|POLLWRBAND)
#define EXCEPT (POLLRDBAND|POLLPRI)
#define READWRITE (READ|WRITE)
#define READWRITEEXCEPT (READ|WRITE|EXCEPT)


/* for AF_UNIX sockets */
#define MAX_PATH_NAME_SIZE 64

/* bind */
#define BIND_SIG int sockfd, const struct sockaddr *addr, socklen_t addrlen
struct bind_st
{
	int sockfd;
	struct sockaddr addr;
	socklen_t addrlen;
	int __tid;
};

/* connect */
#define CONNECT_SIG int __fd, const struct sockaddr * __addr, socklen_t __len
struct connect_st
{
	int __fd;
	struct sockaddr __addr;
	socklen_t __len;
	int __tid;
};

/* close */
#define CLOSE_SIG int fd
struct close_st
{
	int fd;
};

/* read */
#define DEFAULT_READ_BUFFER_SIZE	1024 * 63
/* read buffer sizes (on test machine) min: 4096    default: 87380   max:6147872 */
#define READ_SIG int __fd, void *__buf, size_t __nbytes
struct read_st
{
	int fd;
	size_t count;
	unsigned char buf[DEFAULT_READ_BUFFER_SIZE];
};

/* write */
#define DEFAULT_WRITE_BUFFER_SIZE	1024 * 63
/* write buffer sizes (on test machine) min: 4096    default: 16384   max:4194304 */
#define WRITE_SIG int __fd, const void *__buf, size_t __n
struct write_st
{
	int fd;
	size_t count;
	char buf[DEFAULT_WRITE_BUFFER_SIZE];
};

#define LISTEN_SIG int sockfd, int backlog
struct listen_st
{
	int sockfd;
	int backlog;
	int __tid;
};

#define SOCKET_SIG int socket_family, int socket_type, int protocol
struct socket_st
{
  int socket_family;
  int socket_type;
  int protocol;
	int __tid;
};

#define ACCEPT4_SIG int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags
#define ACCEPT_SIG int sockfd, struct sockaddr *addr, socklen_t *addrlen
struct accept_st
{
	int sockfd;
	struct sockaddr addr;
	socklen_t addrlen;
	int __tid;
};

#define SHUTDOWN_SIG int socket, int how
struct shutdown_st
{
	int socket;
	int how;
};

#define CONNECT_SOCKARG struct sockaddr *
#define SELECT_SIG int n, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout
#define IOCTL_SIG int __fd, unsigned long int __request, ...
#define FCNTL_SIG int __fd, int __cmd, ...
#define DAEMON_SIG int nochdir, int noclose
#define SETSOCKOPT_SIG int socket, int level, int option_name, const void *option_value, socklen_t option_len
#define GETSOCKOPT_SIG int sockfd, int level, int optname, void *optval, socklen_t *optlen
#define SYSCALL_SIG	long number, ...
#define CLONE_SIG int (*fn)(void *), void *child_stack, int flags, void *arg, ...
#define POLL_SIG struct pollfd *fds, nfds_t nfds, int timeout
#define GETSOCKNAME_SIG int sockfd, struct sockaddr *addr, socklen_t *addrlen

#define DUP2_SIG int oldfd, int newfd
#define DUP3_SIG int oldfd, int newfd, int flags

#endif
