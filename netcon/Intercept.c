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

#ifdef USE_GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <dlfcn.h>
#include <strings.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <pwd.h>
#include <errno.h>
#include <linux/errno.h>
#include <stdarg.h>
#include <netdb.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <sys/resource.h>
#include <linux/net.h> /* for NPROTO */

#define SOCK_MAX (SOCK_PACKET + 1)
#define SOCK_TYPE_MASK 0xf

#include "Intercept.h"
#include "RPC.h"
#include "common.inc.c"

/* Global Declarations */
static int (*realconnect)(CONNECT_SIG);
static int (*realbind)(BIND_SIG);
static int (*realaccept)(ACCEPT_SIG);
static int (*reallisten)(LISTEN_SIG);
static int (*realsocket)(SOCKET_SIG);
static int (*realsetsockopt)(SETSOCKOPT_SIG);
static int (*realgetsockopt)(GETSOCKOPT_SIG);
static int (*realaccept4)(ACCEPT4_SIG);
static long (*realsyscall)(SYSCALL_SIG);
static int (*realclose)(CLOSE_SIG);
static int (*realclone)(CLONE_SIG);
static int (*realdup2)(DUP2_SIG);
static int (*realdup3)(DUP3_SIG);
static int (*realgetsockname)(GETSOCKNAME_SIG);

/* Exported Function Prototypes */
void my_init(void);
int connect(CONNECT_SIG);
int bind(BIND_SIG);
int accept(ACCEPT_SIG);
int listen(LISTEN_SIG);
int socket(SOCKET_SIG);
int setsockopt(SETSOCKOPT_SIG);
int getsockopt(GETSOCKOPT_SIG);
int accept4(ACCEPT4_SIG);
long syscall(SYSCALL_SIG);
int close(CLOSE_SIG);
int clone(CLONE_SIG);
int dup2(DUP2_SIG);
int dup3(DUP3_SIG);
int getsockname(GETSOCKNAME_SIG);

static int init_service_connection();
static void load_symbols(void);
static void set_up_intercept();

/*------------------------------------------------------------------------------
------------------- Intercept<--->Service Comm mechanisms ----------------------
------------------------------------------------------------------------------*/

static int rpcfd = -1; /* used for fd-transfers */
static int thispid = -1;
static int instance_count = 0;

static int connected_to_service() {
  return rpcfd == -1 ? 0 : 1;
}

/* Check whether the socket is mapped to the service or not. We
need to know if this is a regular AF_LOCAL socket or an end of a socketpair
that the service uses. We don't want to keep state in the intercept, so
we simply ask the service via an RPC */
static int is_mapped_to_service(int sockfd)
{
  if(rpcfd < 0)
    return 0; /* no connection obviously implies no mapping */
  dwr(MSG_DEBUG,"is_mapped_to_service()\n");
  return rpc_send_command(RPC_MAP_REQ, rpcfd, &sockfd, sizeof(sockfd));
}

/* Sets up the connection pipes and sockets to the service */
static int init_service_connection()
{
  const char *network_id;
  char rpcname[1024];
  network_id = getenv("ZT_NC_NETWORK");
  /* Do noting if not configured (sanity check -- should never get here in this case) */
  if (!network_id){
    fprintf(stderr, "init_service_connection(): ZT_NC_NETWORK not set.\n");
    exit(0);
  }
  if((rpcfd < 0 && instance_count==0) || thispid != getpid())
    rpc_mutex_init();

  strncpy(rpcname,network_id,sizeof(rpcname));
  instance_count++;
  return rpc_join(rpcname);
}

/*------------------------------------------------------------------------------
------------------------ ctors and dtors (and friends) ------------------------
------------------------------------------------------------------------------*/

static void my_dest(void) __attribute__ ((destructor));
static void my_dest(void) {
  dwr(MSG_DEBUG,"closing connections to service...\n");
  rpc_mutex_destroy();
}

static void load_symbols(void)
{
  if(thispid == getpid()) {
    dwr(MSG_DEBUG,"detected duplicate call to global constructor (pid=%d).\n", thispid);
  }
  thispid = getpid();

  realconnect = dlsym(RTLD_NEXT, "connect");
  realbind = dlsym(RTLD_NEXT, "bind");
  realaccept = dlsym(RTLD_NEXT, "accept");
  reallisten = dlsym(RTLD_NEXT, "listen");
  realsocket = dlsym(RTLD_NEXT, "socket");
  realbind = dlsym(RTLD_NEXT, "bind");
  realsetsockopt = dlsym(RTLD_NEXT, "setsockopt");
  realgetsockopt = dlsym(RTLD_NEXT, "getsockopt");
  realaccept4 = dlsym(RTLD_NEXT, "accept4");
  realclone = dlsym(RTLD_NEXT, "clone");
  realclose = dlsym(RTLD_NEXT, "close");
  realsyscall = dlsym(RTLD_NEXT, "syscall");
  realdup2 = dlsym(RTLD_NEXT, "dup2");
  realdup3 = dlsym(RTLD_NEXT, "dup3");
  realgetsockname = dlsym(RTLD_NEXT, "getsockname");
}

/* Private Function Prototypes */
static void _init(void) __attribute__ ((constructor));
static void _init(void) { set_up_intercept(); }

/* get symbols and initialize mutexes */
static void set_up_intercept()
{
  if (!getenv("ZT_NC_NETWORK"))
    return;
  /* Hook/intercept Posix net API symbols */
  load_symbols();
}

/*------------------------------------------------------------------------------
--------------------------------- setsockopt() ---------------------------------
------------------------------------------------------------------------------*/

/* int socket, int level, int option_name, const void *option_value, socklen_t option_len */
int setsockopt(SETSOCKOPT_SIG)
{
  if(realsetsockopt == NULL){
    dwr(MSG_ERROR, "setsockopt(): SYMBOL NOT FOUND.\n");
    return -1;
  }
  dwr(MSG_DEBUG,"setsockopt(%d)\n", socket);
  
  /* return(realsetsockopt(socket, level, option_name, option_value, option_len)); */
  if(level == SOL_IPV6 && option_name == IPV6_V6ONLY)
    return 0;
  if(level == SOL_IP && option_name == IP_TTL)
    return 0;
  if(level == IPPROTO_TCP || (level == SOL_SOCKET && option_name == SO_KEEPALIVE))
    return 0;
  /* make sure we don't touch any standard outputs */
  if(socket == STDIN_FILENO || socket == STDOUT_FILENO || socket == STDERR_FILENO)
    return(realsetsockopt(socket, level, option_name, option_value, option_len));
  int err = realsetsockopt(socket, level, option_name, option_value, option_len);
  if(err < 0){
    perror("setsockopt():\n");
  }
  return 0;
}

/*------------------------------------------------------------------------------
--------------------------------- getsockopt() ---------------------------------
------------------------------------------------------------------------------*/

/* int sockfd, int level, int optname, void *optval, socklen_t *optlen */
int getsockopt(GETSOCKOPT_SIG)
{
  if(realgetsockopt == NULL){
    dwr(MSG_ERROR, "getsockopt(): SYMBOL NOT FOUND.\n");
    return -1;
  }
  dwr(MSG_DEBUG,"getsockopt(%d)\n", sockfd);

  if(is_mapped_to_service(sockfd) <= 0) { // First, check if the service manages this
    return realgetsockopt(sockfd, level, optname, optval, optlen);
  }

  //int err = realgetsockopt(sockfd, level, optname, optval, optlen);
  /* TODO: this condition will need a little more intelligence later on
   -- we will need to know if this fd is a local we are spoofing, or a true local */

  if(optname == SO_TYPE) {
    int* val = (int*)optval;
    *val = 2;
    optval = (void*)val;
  }
  return 0;
}


/*------------------------------------------------------------------------------
----------------------------------- socket() -----------------------------------
------------------------------------------------------------------------------*/

/* int socket_family, int socket_type, int protocol
   socket() intercept function */
int socket(SOCKET_SIG)
{  
  if(realsocket == NULL)
    set_up_intercept();
  
  dwr(MSG_DEBUG,"socket():\n");
  int newfd = -1;
  /* Check that type makes sense */
  int flags = socket_type & ~SOCK_TYPE_MASK;
  if (flags & ~(SOCK_CLOEXEC | SOCK_NONBLOCK)) {
      errno = EINVAL;
      return -1;
  }
  socket_type &= SOCK_TYPE_MASK;
  /* Check protocol is in range */
  if (socket_family < 0 || socket_family >= NPROTO){
    errno = EAFNOSUPPORT;
    return -1;
  }
  if (socket_type < 0 || socket_type >= SOCK_MAX) {
    errno = EINVAL;
    return -1;
  }
  /* TODO: detect ENFILE condition */

  if(socket_family == AF_LOCAL
    || socket_family == AF_NETLINK
    || socket_family == AF_UNIX) {
      int err = realsocket(socket_family, socket_type, protocol);
      dwr(MSG_DEBUG,"realsocket() = %d\n", err);
      return err;
  }

  rpcfd = !connected_to_service() ? init_service_connection() : rpcfd;
  if(rpcfd < 0) {
    dwr(MSG_DEBUG,"BAD service connection. exiting.\n");
    exit(-1);
  }

  /* Assemble and send RPC */
  struct socket_st rpc_st;
  rpc_st.socket_family = socket_family;
  rpc_st.socket_type = socket_type;
  rpc_st.protocol = protocol;
  rpc_st.__tid = syscall(SYS_gettid);

  newfd = rpc_send_command(RPC_SOCKET, rpcfd, &rpc_st, sizeof(struct socket_st));
  if(newfd > 0)
  {
    dwr(MSG_DEBUG,"sending fd = %d to Service over (%d)\n", newfd, rpcfd);
    /* send our local-fd number back to service so
     it can complete its mapping table entry */
  	/* send fd mapping and get confirmation */
  	if(rpc_send_command(RPC_MAP, rpcfd, &newfd, sizeof(newfd)) > -1) {
  	  errno = ERR_OK;
  	  dwr(MSG_DEBUG, "RXd fd confirmation. Mapped!\n");
      return newfd; /* Mapping complete, everything is OK */
    }
  }
  dwr(MSG_DEBUG,"Error while receiving new fd.\n");
  return -1;
}

/*------------------------------------------------------------------------------
---------------------------------- connect() -----------------------------------
------------------------------------------------------------------------------*/

/* int __fd, const struct sockaddr * __addr, socklen_t __len
   connect() intercept function */
int connect(CONNECT_SIG)
{
  if(realconnect == NULL){
    dwr(MSG_ERROR, "connect(): SYMBOL NOT FOUND.\n");
    return -1;
  }
  dwr(MSG_DEBUG,"connect(%d):\n", __fd);
  struct sockaddr_in *connaddr;
  connaddr = (struct sockaddr_in *) __addr;

  /* Check that this is a valid fd */
  if(fcntl(__fd, F_GETFD) < 0) {
    errno = EBADF;
    return -1;
  }
  /* Check that it is a socket */
  int sock_type;
  socklen_t sock_type_len = sizeof(sock_type);
  if(getsockopt(__fd, SOL_SOCKET, SO_TYPE, (void *) &sock_type, &sock_type_len) < 0) {
    errno = ENOTSOCK;
    return -1;
  }
  /* Check family */
  if (connaddr->sin_family < 0 || connaddr->sin_family >= NPROTO){
    errno = EAFNOSUPPORT;
    return -1;
  }
  /* FIXME: Check that address is in user space, return EFAULT ? */

  /* make sure we don't touch any standard outputs */
  if(__fd == STDIN_FILENO || __fd == STDOUT_FILENO || __fd == STDERR_FILENO)
    return(realconnect(__fd, __addr, __len));

  if(__addr != NULL && (connaddr->sin_family == AF_LOCAL
    || connaddr->sin_family == PF_NETLINK
    || connaddr->sin_family == AF_NETLINK
    || connaddr->sin_family == AF_UNIX)) {
    int err = realconnect(__fd, __addr, __len);
    //perror("connect():");
    return err;
  }

  /* Assemble and send RPC */
  struct connect_st rpc_st;
  rpc_st.__tid = syscall(SYS_gettid);
  rpc_st.__fd = __fd;
  memcpy(&rpc_st.__addr, __addr, sizeof(struct sockaddr_storage));
  memcpy(&rpc_st.__len, &__len, sizeof(socklen_t));

  return rpc_send_command(RPC_CONNECT, rpcfd, &rpc_st, sizeof(struct connect_st));
}

/*------------------------------------------------------------------------------
------------------------------------ bind() ------------------------------------
------------------------------------------------------------------------------*/

/* int sockfd, const struct sockaddr *addr, socklen_t addrlen
   bind() intercept function */
int bind(BIND_SIG)
{
  if(realbind == NULL){
    dwr(MSG_ERROR, "bind(): SYMBOL NOT FOUND.\n");
    return -1;
  }
  dwr(MSG_DEBUG,"bind(%d):\n", sockfd);
  /* Check that this is a valid fd */
  if(fcntl(sockfd, F_GETFD) < 0) {
    errno = EBADF;
    return -1;
  }
  /* Check that it is a socket */
  int opt = -1;
  socklen_t opt_len;
  if(getsockopt(sockfd, SOL_SOCKET, SO_TYPE, (void *) &opt, &opt_len) < 0) {
    errno = ENOTSOCK;
    return -1;
  }

  /* make sure we don't touch any standard outputs */
  if(sockfd == STDIN_FILENO || sockfd == STDOUT_FILENO || sockfd == STDERR_FILENO)
    return(realbind(sockfd, addr, addrlen));

  /* If local, just use normal syscall */
  struct sockaddr_in *connaddr;
  connaddr = (struct sockaddr_in *)addr;

  if(connaddr->sin_family == AF_LOCAL
    || connaddr->sin_family == AF_NETLINK
    || connaddr->sin_family == AF_UNIX) {
      int err = realbind(sockfd, addr, addrlen);
      dwr(MSG_DEBUG,"realbind, err = %d\n", err);
      return err;
  }

  int port = connaddr->sin_port;
  int ip = connaddr->sin_addr.s_addr;
  unsigned char d[4];
  d[0] = ip & 0xFF;
  d[1] = (ip >>  8) & 0xFF;
  d[2] = (ip >> 16) & 0xFF;
  d[3] = (ip >> 24) & 0xFF;
  dwr(MSG_DEBUG, "bind(): %d.%d.%d.%d: %d\n", d[0],d[1],d[2],d[3], ntohs(port));

  /* Assemble and send RPC */
  struct bind_st rpc_st;
  rpc_st.sockfd = sockfd;
  rpc_st.__tid = syscall(SYS_gettid);
  memcpy(&rpc_st.addr, addr, sizeof(struct sockaddr_storage));
  memcpy(&rpc_st.addrlen, &addrlen, sizeof(socklen_t));

  return rpc_send_command(RPC_BIND, rpcfd, &rpc_st, sizeof(struct bind_st));
}

/*------------------------------------------------------------------------------
----------------------------------- accept4() ----------------------------------
------------------------------------------------------------------------------*/


/* int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags */
int accept4(ACCEPT4_SIG)
{
  if(realaccept4 == NULL){
    dwr(MSG_ERROR, "accept4(): SYMBOL NOT FOUND.\n");
    return -1;
  }
  dwr(MSG_DEBUG,"accept4(%d):\n", sockfd);
  if ((flags & SOCK_CLOEXEC))
    fcntl(sockfd, F_SETFL, FD_CLOEXEC);
  if ((flags & SOCK_NONBLOCK))
    fcntl(sockfd, F_SETFL, O_NONBLOCK);
  int newfd = accept(sockfd, addr, addrlen);
  return newfd;
}

/*------------------------------------------------------------------------------
----------------------------------- accept() -----------------------------------
------------------------------------------------------------------------------*/

/* int sockfd struct sockaddr *addr, socklen_t *addrlen
   accept() intercept function */
int accept(ACCEPT_SIG)
{
  if(realaccept == NULL){
    dwr(MSG_ERROR, "accept(): SYMBOL NOT FOUND.\n");
    return -1;
  }
  dwr(MSG_DEBUG,"accept(%d):\n", sockfd);
  /* Check that this is a valid fd */
  if(fcntl(sockfd, F_GETFD) < 0) {
    return -1;
    errno = EBADF;
    dwr(MSG_DEBUG,"EBADF\n");
    return -1;
  }
  /* Check that it is a socket */
  int opt;
  socklen_t opt_len;
  if(getsockopt(sockfd, SOL_SOCKET, SO_TYPE, (void *) &opt, &opt_len) < 0) {
    errno = ENOTSOCK;
    dwr(MSG_DEBUG,"ENOTSOCK\n");
    return -1;
  }
  /* Check that this socket supports accept() */
  if(!(opt && (SOCK_STREAM | SOCK_SEQPACKET))) {
    errno = EOPNOTSUPP;
    dwr(MSG_DEBUG,"EOPNOTSUPP\n");
    return -1;
  }
  /* Check that we haven't hit the soft-limit file descriptors allowed */
  struct rlimit rl;
  getrlimit(RLIMIT_NOFILE, &rl);
  if(sockfd >= rl.rlim_cur){
    errno = EMFILE;
    dwr(MSG_DEBUG,"EMFILE\n");
    return -1;
  }
  /* Check address length */
  if(addrlen < 0) {
    errno = EINVAL;
    dwr(MSG_DEBUG,"EINVAL\n");
    return -1;
  }

  /* redirect calls for standard I/O descriptors to kernel */
  if(sockfd == STDIN_FILENO || sockfd == STDOUT_FILENO || sockfd == STDERR_FILENO){
    dwr(MSG_DEBUG,"realaccept():\n");
    return(realaccept(sockfd, addr, addrlen));
  }

  if(addr)
    addr->sa_family = AF_INET;
    /* TODO: also get address info */

  /* The following line is required for libuv/nodejs to accept connections properly,
  however, this has the side effect of causing certain webservers to max out the CPU
  in an accept loop */
  //fcntl(sockfd, F_SETFL, SOCK_NONBLOCK);
  int new_conn_socket = get_new_fd(sockfd);

  if(new_conn_socket > 0)
  {
    dwr(MSG_DEBUG, "accept(): RX: fd = (%d) over (%d)\n", new_conn_socket, rpcfd);
    /* Send our local-fd number back to service so it can complete its mapping table */
    dwr(MSG_DEBUG, "accept(): sending perceived fd (%d) to service.\n", new_conn_socket);
    rpc_send_command(RPC_MAP, rpcfd, &new_conn_socket, sizeof(new_conn_socket));
    dwr(MSG_DEBUG,"accept()=%d\n", new_conn_socket);
    errno = ERR_OK;
    return new_conn_socket; /* OK */
  }
  dwr(MSG_DEBUG, "accept(): EAGAIN - Error reading signal byte from service");
  errno = EAGAIN;
  return -EAGAIN;
}


/*------------------------------------------------------------------------------
------------------------------------- listen()----------------------------------
------------------------------------------------------------------------------*/

/* int sockfd, int backlog */
int listen(LISTEN_SIG)
{
  if(reallisten == NULL){
    dwr(MSG_ERROR, "listen(): SYMBOL NOT FOUND.\n");
    return -1;
  }
  dwr(MSG_DEBUG,"listen(%d):\n", sockfd);
  int sock_type;
  socklen_t sock_type_len = sizeof(sock_type);

  /* Check that this is a valid fd */
  if(fcntl(sockfd, F_GETFD) < 0) {
    errno = EBADF;
    return -1;
  }
  /* Check that it is a socket */
  if(getsockopt(sockfd, SOL_SOCKET, SO_TYPE, (void *) &sock_type, &sock_type_len) < 0) {
    errno = ENOTSOCK;
    return -1;
  }
  /* Check that this socket supports accept() */
  if(!(sock_type && (SOCK_STREAM | SOCK_SEQPACKET))) {
    errno = EOPNOTSUPP;
    return -1;
  }

  /* make sure we don't touch any standard outputs */
  if(sockfd == STDIN_FILENO || sockfd == STDOUT_FILENO || sockfd == STDERR_FILENO)
    return(reallisten(sockfd, backlog));

  if(is_mapped_to_service(sockfd) < 0) {
    /* We now know this socket is not one of our socketpairs */
    int err = reallisten(sockfd, backlog);
    dwr(MSG_DEBUG,"reallisten()=%d\n", err);
    return err;
  }

  /* Assemble and send RPC */
  struct listen_st rpc_st;
  rpc_st.sockfd = sockfd;
  rpc_st.backlog = backlog;
  rpc_st.__tid = syscall(SYS_gettid);

  return rpc_send_command(RPC_LISTEN, rpcfd, &rpc_st, sizeof(struct listen_st));
}

/*------------------------------------------------------------------------------
-------------------------------------- clone() ---------------------------------
------------------------------------------------------------------------------*/

/* int (*fn)(void *), void *child_stack, int flags, void *arg, ... */
int clone(CLONE_SIG)
{
  if(realclone == NULL){
    dwr(MSG_ERROR, "clone(): SYMBOL NOT FOUND.\n");
    return -1;
  }
  dwr(MSG_DEBUG,"clone()\n");
  int err = realclone(fn, child_stack, flags, arg);
  init_service_connection();
  return err;
}

/*------------------------------------------------------------------------------
------------------------------------- close() ----------------------------------
------------------------------------------------------------------------------*/

/* int fd */
int close(CLOSE_SIG)
{
  dwr(MSG_DEBUG, "close(%d)\n", fd);
  if(realclose == NULL)
    init_service_connection();    
  if(fd == rpcfd)
    return -1; /* TODO: Ignore request to shut down our rpc fd, this is *almost always* safe */
  if(fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO)
    return realclose(fd);
  return -1;
}

/*------------------------------------------------------------------------------
-------------------------------------- dup2() ----------------------------------
------------------------------------------------------------------------------*/

/* int oldfd, int newfd */
int dup2(DUP2_SIG)
{
  if(realdup2 == NULL){
    dwr(MSG_ERROR, "dup2(): SYMBOL NOT FOUND.\n");
    return -1;
  }
  dwr(MSG_DEBUG,"dup2(%d, %d)\n", oldfd, newfd);
    if(oldfd == rpcfd) {
    dwr(MSG_DEBUG,"client application attempted to dup2 RPC socket (%d). This is not allowed.\n", oldfd);
    errno = EBADF;
    return -1;
  }
  return realdup2(oldfd, newfd);
}

/*------------------------------------------------------------------------------
-------------------------------------- dup3() ----------------------------------
------------------------------------------------------------------------------*/

/* int oldfd, int newfd, int flags */
int dup3(DUP3_SIG)
{
  if(realdup3 == NULL){
    dwr(MSG_ERROR, "dup3(): SYMBOL NOT FOUND.\n");
    return -1;
  }
  dwr(MSG_DEBUG,"dup3(%d, %d, %d)\n", oldfd, newfd, flags);
  return realdup3(oldfd, newfd, flags);
}

/*------------------------------------------------------------------------------
-------------------------------- getsockname() ---------------------------------
------------------------------------------------------------------------------*/

/* define GETSOCKNAME_SIG int sockfd, struct sockaddr *addr, socklen_t *addrlen */
int getsockname(GETSOCKNAME_SIG)
{
  if (realgetsockname == NULL) {
    dwr(MSG_ERROR, "getsockname(): SYMBOL NOT FOUND. \n");
    return -1;
  }
  dwr(MSG_DEBUG, "getsockname(%d)\n", sockfd);
  if(!is_mapped_to_service(sockfd))
    return realgetsockname(sockfd, addr, addrlen);

  /* This is kind of a hack as it stands -- assumes sockaddr is sockaddr_in
   * and is an IPv4 address. */

  /* assemble and send command */
  struct getsockname_st rpc_st;
  rpc_st.sockfd = sockfd;
  memcpy(&rpc_st.addr, addr, *addrlen);
  memcpy(&rpc_st.addrlen, &addrlen, sizeof(socklen_t));
  rpc_send_command(RPC_GETSOCKNAME, rpcfd, &rpc_st, sizeof(struct getsockname_st));

  /* read address info from service */
  char addrbuf[sizeof(struct sockaddr_storage)];
  memset(&addrbuf, 0, sizeof(struct sockaddr_storage));
  read(rpcfd, &addrbuf, sizeof(struct sockaddr_storage));
  struct sockaddr_storage sock_storage;
  memcpy(&sock_storage, addrbuf, sizeof(struct sockaddr_storage));
  *addrlen = sizeof(struct sockaddr_in);
  memcpy(addr, &sock_storage, (*addrlen > sizeof(sock_storage)) ? sizeof(sock_storage) : *addrlen);
  addr->sa_family = AF_INET;
  return 0;
}

/*------------------------------------------------------------------------------
------------------------------------ syscall() ---------------------------------
------------------------------------------------------------------------------*/

long syscall(SYSCALL_SIG){

  //dwr(MSG_DEBUG_EXTRA,"syscall(%u, ...):\n", number);

  va_list ap;
  uintptr_t a,b,c,d,e,f;
  va_start(ap, number);
  a=va_arg(ap, uintptr_t);
  b=va_arg(ap, uintptr_t);
  c=va_arg(ap, uintptr_t);
  d=va_arg(ap, uintptr_t);
  e=va_arg(ap, uintptr_t);
  f=va_arg(ap, uintptr_t);
  va_end(ap);

  if(realsyscall == NULL)
    return -1;

#if defined(__i386__)
  /* TODO: Implement for 32-bit systems: syscall(__NR_socketcall, 18, args);
  args[0] = (unsigned long) fd;
  args[1] = (unsigned long) addr;
  args[2] = (unsigned long) addrlen;
  args[3] = (unsigned long) flags;
  */
#else
  if(number == __NR_accept4) {
    int sockfd = a;
    struct sockaddr * addr = (struct sockaddr*)b;
    socklen_t * addrlen = (socklen_t*)c;
    int flags = d;
    int old_errno = errno;
    int err = accept4(sockfd, addr, addrlen, flags);
    errno = old_errno;
    err = err == -EBADF ? -EAGAIN : err;
    return err;
  }
#endif
  return realsyscall(number,a,b,c,d,e,f);
}
