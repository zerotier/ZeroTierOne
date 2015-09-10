
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>

#ifndef _NETCON_UTILITIES_CPP
#define _NETCON_UTILITIES_CPP



ip_addr_t convert_ip(struct sockaddr_in * addr)
{
  ip_addr_t conn_addr;
  struct sockaddr_in *ipv4 = addr;
  short a = ip4_addr1(&(ipv4->sin_addr));
  short b = ip4_addr2(&(ipv4->sin_addr));
  short c = ip4_addr3(&(ipv4->sin_addr));
  short d = ip4_addr4(&(ipv4->sin_addr));
  IP4_ADDR(&conn_addr, a,b,c,d);
  return conn_addr;
}

ip_addr_t ip_addr_sin(register struct sockaddr_in *sin) {
   ip_addr_t ip;
   *((struct sockaddr_in*) &ip) = *sin;
   return ip;
}

// Functions used to pass file descriptors between processes

ssize_t sock_fd_write(int sock, int fd);
ssize_t sock_fd_read(int sock, void *buf, ssize_t bufsize, int *fd);

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

#endif
