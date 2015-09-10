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


/* Userland RPC codes */
#define RPC_UNDEFINED			 			0
#define RPC_CONNECT				 			1
#define RPC_CONNECT_SOCKARG		 	2
#define RPC_SELECT			 				3
#define RPC_POLL				 				4
#define RPC_CLOSE				 				5
#define RPC_READ				 				6
#define RPC_WRITE				 				7
#define RPC_BIND				 				8
#define RPC_ACCEPT			 				9
#define RPC_LISTEN			 				10
#define RPC_SOCKET			 				11
#define RPC_SHUTDOWN		 				12

/* Administration RPC codes */
#define RPC_FD_MAP_COMPLETION		20	// Give the service the value we "see" for the new buffer fd
#define RPC_RETVAL							21	// not RPC per se, but something we should codify
#define RPC_KILL_INTERCEPT			22  // Tells the service we need to shut down all connections

/* Connection statuses */
#define UNSTARTED								0
#define CONNECTING			 				1
#define CONNECTED				 				2
#define SENDING					 				3
#define RECEIVING				 				4
#define SENTV4REQ				 				5
#define GOTV4REQ				 				6
#define SENTV5METHOD			 			7
#define GOTV5METHOD				 			8
#define SENTV5AUTH			 				9
#define GOTV5AUTH				 				10
#define SENTV5CONNECT			 			11
#define GOTV5CONNECT			 			12
#define DONE					 					13
#define FAILED				 					14

/* Flags to indicate what events a
   socket was select()ed for */
#define READ					 					(POLLIN|POLLRDNORM)
#define WRITE					 					(POLLOUT|POLLWRNORM|POLLWRBAND)
#define EXCEPT				 					(POLLRDBAND|POLLPRI)
#define READWRITE				 				(READ|WRITE)
#define READWRITEEXCEPT			 		(READ|WRITE|EXCEPT)


/* for AF_UNIX sockets */
#define MAX_PATH_NAME_SIZE 64

// bind
#define BIND_SIG int sockfd, const struct sockaddr *addr, socklen_t addrlen
struct bind_st
{
	int sockfd;
	struct sockaddr addr;
	socklen_t addrlen;
	int __tid;
};

// connect
#define CONNECT_SIG int __fd, const struct sockaddr * __addr, socklen_t __len
struct connect_st
{
	int __fd;
	struct sockaddr __addr;
	socklen_t __len;
	int __tid;
};

// close
#define CLOSE_SIG int fd
struct close_st
{
	int fd;
};

// read
#define DEFAULT_READ_BUFFER_SIZE	1024 * 63
// read buffer sizes (on test machine) min: 4096    default: 87380   max:6147872
#define READ_SIG int __fd, void *__buf, size_t __nbytes
struct read_st
{
	int fd;
	size_t count;
	unsigned char buf[DEFAULT_READ_BUFFER_SIZE];
};

#define DEFAULT_WRITE_BUFFER_SIZE	1024 * 63
// write buffer sizes (on test machine) min: 4096    default: 16384   max:4194304
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
#define POLL_SIG struct pollfd *__fds, nfds_t __nfds, int __timeout
#define IOCTL_SIG int __fd, unsigned long int __request, ...
#define FCNTL_SIG int __fd, int __cmd, ...
#define CLONE_SIG int (*fn) (void *arg), void *child_stack, int flags, void *arg
#define DAEMON_SIG int nochdir, int noclose
#define SETSOCKOPT_SIG int socket, int level, int option_name, const void *option_value, socklen_t option_len
#define GETSOCKOPT_SIG int sockfd, int level, int optname, void *optval, socklen_t *optlen


/* LWIP error beautification */
const char *lwiperror(int n)
{
	switch(n)
	{
		case 0:
			return "ERR_OK";
		case -1:
			return "ERR_MEM (out of memory)";
		case -2:
			return "ERR_BUF (buffer error)";
		case -3:
			return "ERR_TIMEOUT (timeout)";
		case -4:
			return "ERR_RTE (routing problem)";
		case -5:
			return "ERR_INPROGRESS (operation in progress)";
		case -6:
			return "ERR_VAL (illegal value)";
		case -7:
			return "ERR_WOULDBLOCK (operation would block)";
		case -8:
			return "ERR_USE (address in use)";
		case -9:
			return "ERR_ISCONN (already connected)";
		case -10:
			return "Fatal: ERR_ABRT (connection aborted)";
		case -11:
			return "Fatal: ERR_RST (connection reset)";
		case -12:
			return "Fatal: ERR_CLSD (connection closed)";
		case -13:
			return "Fatal: ERR_CONN (not connected)";
		case -14:
			return "Fatal: ERR_ARG (illegal argument)";
		case -15:
			return "Fatal: ERR_IF (low level netif error)";
		default:
			return "UNKNOWN_RET_VAL";
	}
}

#endif
