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

#include <stdio.h>
#include <netdb.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <fcntl.h>


#define DEBUG_LEVEL 0

#define MSG_WARNING     4
#define MSG_ERROR       1 // Errors
#define MSG_INFO        2 // Information which is generally useful to any user
#define MSG_DEBUG       3 // Information which is only useful to someone debugging
#define MSG_DEBUG_EXTRA 4 // If nothing in your world makes sense

#ifdef NETCON_INTERCEPT

static pthread_mutex_t loglock;

void print_addr(struct sockaddr *addr)
{
  char *s = NULL;
  switch(addr->sa_family) {
      case AF_INET: {
          struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
          s = malloc(INET_ADDRSTRLEN);
          inet_ntop(AF_INET, &(addr_in->sin_addr), s, INET_ADDRSTRLEN);
          break;
      }
      case AF_INET6: {
          struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)addr;
          s = malloc(INET6_ADDRSTRLEN);
          inet_ntop(AF_INET6, &(addr_in6->sin6_addr), s, INET6_ADDRSTRLEN);
          break;
      }
      default:
          break;
  }
  fprintf(stderr, "IP address: %s\n", s);
  free(s);
}
#endif

#ifdef NETCON_SERVICE
  namespace ZeroTier {
#endif
  void dwr(int level, const char *fmt, ... )
  {
    if(level > DEBUG_LEVEL)
        return;
    int saveerr;
    saveerr = errno;
    va_list ap;
    va_start(ap, fmt);
  #ifdef VERBOSE // So we can cut out some clutter in the strace output while debugging
    char timestring[20];
    time_t timestamp;
    timestamp = time(NULL);
    strftime(timestring, sizeof(timestring), "%H:%M:%S", localtime(&timestamp));
    pid_t pid = getpid();
    fprintf(stderr, "%s [pid=%7d] ", timestring, pid);
  #endif
    vfprintf(stderr, fmt, ap);
    fflush(stderr);

    errno = saveerr;
    va_end(ap);
  }
#ifdef NETCON_SERVICE
}
#endif

static ssize_t sock_fd_write(int sock, int fd);
static ssize_t sock_fd_read(int sock, void *buf, ssize_t bufsize, int *fd);

static ssize_t sock_fd_write(int sock, int fd)
{
	ssize_t size;
	struct msghdr msg;
	struct iovec iov;
	char buf = '\0';
	int buflen = 1;

	union {
        struct cmsghdr  cmsghdr;
		char control[CMSG_SPACE(sizeof (int))];
	} cmsgu;
	struct cmsghdr *cmsg;

	iov.iov_base = &buf;
	iov.iov_len = buflen;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	if (fd != -1) {
		  msg.msg_control = cmsgu.control;
		  msg.msg_controllen = sizeof(cmsgu.control);
		  cmsg = CMSG_FIRSTHDR(&msg);
		  cmsg->cmsg_len = CMSG_LEN(sizeof (int));
		  cmsg->cmsg_level = SOL_SOCKET;
		  cmsg->cmsg_type = SCM_RIGHTS;
		  *((int *) CMSG_DATA(cmsg)) = fd;
	} else {
		  msg.msg_control = NULL;
		  msg.msg_controllen = 0;
	}

	size = sendmsg(sock, &msg, 0);
	if (size < 0)
		  perror ("sendmsg");
	return size;
}

static ssize_t sock_fd_read(int sock, void *buf, ssize_t bufsize, int *fd)
{
  ssize_t size;
  if (fd) {
    struct msghdr msg;
    struct iovec iov;
    union {
      struct cmsghdr cmsghdr;
      char control[CMSG_SPACE(sizeof (int))];
    } cmsgu;
    struct cmsghdr *cmsg;

    iov.iov_base = buf;
    iov.iov_len = bufsize;

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsgu.control;
    msg.msg_controllen = sizeof(cmsgu.control);
    size = recvmsg (sock, &msg, 0);
    if (size < 0) {
      dwr(MSG_DEBUG, "sock_fd_read(): recvmsg: Error\n");
      return -1;
    }
    cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg && cmsg->cmsg_len == CMSG_LEN(sizeof(int))) {
      if (cmsg->cmsg_level != SOL_SOCKET) {
        fprintf (stderr, "invalid cmsg_level %d\n",cmsg->cmsg_level);
        return -1;
      }
      if (cmsg->cmsg_type != SCM_RIGHTS) {
          fprintf (stderr, "invalid cmsg_type %d\n",cmsg->cmsg_type);
          return -1;
      }

      *fd = *((int *) CMSG_DATA(cmsg));
    } else *fd = -1;
  } else {
    size = read (sock, buf, bufsize);
    if (size < 0) {
      dwr(MSG_DEBUG, "sock_fd_read(): read: Error\n");
      return -1;
    }
  }
  return size;
}
