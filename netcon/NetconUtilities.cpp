
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


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>

#include "lwip/ip.h"
#include "lwip/ip_addr.h"
#include "lwip/ip_frag.h"

#ifndef _NETCON_UTILITIES_CPP
#define _NETCON_UTILITIES_CPP

namespace ZeroTier
{
	// Functions used to pass file descriptors between processes
	ssize_t sock_fd_write(int sock, int fd)
	{
		ssize_t size;
		struct msghdr msg;
		struct iovec iov;
		char buf = '\0';
		int buflen = 1;

		union
		{
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


	ssize_t sock_fd_read(int sock, void *buf, ssize_t bufsize, int *fd)
	{
	    ssize_t size;

	    if (fd) {
	        struct msghdr msg;
	        struct iovec iov;
	        union
					{
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
	            perror ("recvmsg");
	            exit(1);
	        }
	        cmsg = CMSG_FIRSTHDR(&msg);
	        if (cmsg && cmsg->cmsg_len == CMSG_LEN(sizeof(int))) {
	            if (cmsg->cmsg_level != SOL_SOCKET) {
	                fprintf (stderr, "invalid cmsg_level %d\n",
	                     cmsg->cmsg_level);
	                exit(1);
	            }
	            if (cmsg->cmsg_type != SCM_RIGHTS) {
	                fprintf (stderr, "invalid cmsg_type %d\n",
	                     cmsg->cmsg_type);
	                exit(1);
	            }

	            *fd = *((int *) CMSG_DATA(cmsg));
	        } else
	            *fd = -1;
	    } else {
	        size = read (sock, buf, bufsize);
	        if (size < 0) {
	            perror("read");
	            exit(1);
	        }
	    }
	    return size;
	}
}

#endif
