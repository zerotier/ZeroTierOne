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

#include "Intercept.h"

#include "common.inc.c"

#ifdef CHECKS
  #include <sys/resource.h>
  #include <linux/net.h> /* for NPROTO */
  #define SOCK_MAX (SOCK_PACKET + 1)
  #define SOCK_TYPE_MASK 0xf
#endif

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

#define SERVICE_CONNECT_ATTEMPTS  30
#define RPC_FD                    1023

static pthread_mutex_t lock;
static ssize_t sock_fd_read(int sock, void *buf, ssize_t bufsize, int *fd);

void handle_error(char *name, char *info, int err)
{
#ifdef ERRORS_ARE_FATAL
  if(err < 0) {
    dwr(MSG_DEBUG,"handle_error(%s)=%d: FATAL: %s\n", name, err, info);
    exit(-1);
  }
#endif
#ifdef VERBOSE
  dwr(MSG_DEBUG,"%s()=%d\n", name, err);
#endif
}

/*------------------------------------------------------------------------------
------------------- Intercept<--->Service Comm mechanisms-----------------------
------------------------------------------------------------------------------*/

static int is_initialized = 0;
static int fdret_sock; /* used for fd-transfers */
static int newfd; /* used for "this_end" socket */
static int thispid = -1;
static int instance_count = 0;

/*
 * Check for forking
 */
static void checkpid()
{
  /* Do noting if not configured (sanity check -- should never get here in this case) */
  if (!getenv("ZT_NC_NETWORK"))
    return;

  if (thispid != getpid()) {
    dwr(MSG_DEBUG, "checkpid(): clone/fork detected. Re-initializing this instance.\n");
    set_up_intercept();
    fdret_sock = init_service_connection();
    thispid = getpid();
  }
}


/*
 * Reads a return value from the service and sets errno (if applicable)
 */
static int get_retval()
{
  dwr(MSG_DEBUG,"get_retval()\n");
  if(fdret_sock >= 0) {
    int retval;
    int sz = sizeof(char) + sizeof(retval) + sizeof(errno);
    char retbuf[BUF_SZ];
    memset(&retbuf, '\0', sz);
    int n_read = read(fdret_sock, &retbuf, sz);
    if(n_read > 0) {
      memcpy(&retval, &retbuf[1], sizeof(retval));
      memcpy(&errno, &retbuf[1+sizeof(retval)], sizeof(errno));
      return retval;
    }
  }
  dwr(MSG_DEBUG,"unable to read return value\n");
  return -1;
}

/* Reads a new file descriptor from the service */
static int get_new_fd(int oversock)
{
  char buf[BUF_SZ];
  int newfd;
  ssize_t size = sock_fd_read(oversock, buf, sizeof(buf), &newfd);
  if(size > 0){
    dwr(MSG_DEBUG, "get_new_fd(): RX: fd = (%d) over (%d)\n", newfd, oversock);
    return newfd;
  }
  dwr(MSG_ERROR, "get_new_fd(): ERROR: unable to read fd over (%d)\n", oversock);
  return -1;
}

#ifdef VERBOSE
  static unsigned long rpc_count = 0;
#endif

/* Sends an RPC command to the service */
static int send_cmd(int rpc_fd, char *cmd)
{
  pthread_mutex_lock(&lock);
  char metabuf[BUF_SZ]; // portion of buffer which contains RPC metadata for debugging
#ifdef VERBOSE
  /*
  #define IDX_PID       0
  #define IDX_TID       sizeof(pid_t)
  #define IDX_COUNT     IDX_TID + sizeof(pid_t)
  #define IDX_TIME      IDX_COUNT + sizeof(int)
  #define IDX_CMD       IDX_TIME + 20 // 20 being the length of the timestamp string
  #define IDX_PAYLOAD   IDX_TIME + sizeof(char)
  */
  /* [pid_t] [pid_t] [rpc_count] [int] [...] */
  memset(metabuf, '\0', BUF_SZ);
  pid_t pid = syscall(SYS_getpid);
  pid_t tid = syscall(SYS_gettid);
  rpc_count++;
  char timestring[20];
  time_t timestamp;
  timestamp = time(NULL);
  strftime(timestring, sizeof(timestring), "%H:%M:%S", localtime(&timestamp));
  memcpy(&metabuf[IDX_PID],     &pid,         sizeof(pid_t)      ); /* pid       */
  memcpy(&metabuf[IDX_TID],     &tid,         sizeof(pid_t)      ); /* tid       */
  memcpy(&metabuf[IDX_COUNT],   &rpc_count,   sizeof(rpc_count)  ); /* rpc_count */
  memcpy(&metabuf[IDX_TIME],    &timestring,   20                ); /* timestamp */
#endif
  /* Combine command flag+payload with RPC metadata */
  memcpy(&metabuf[IDX_PAYLOAD], cmd, PAYLOAD_SZ);
  //usleep(100000);
  int n_write = write(rpc_fd, &metabuf, BUF_SZ);
  if(n_write < 0){
    dwr(MSG_DEBUG,"Error writing command to service (CMD = %d)\n", cmd[0]);
    errno = 0;
  }

  int ret = ERR_OK;

  if(n_write > 0) {
    if(cmd[0]==RPC_SOCKET) {
    	ret = get_new_fd(fdret_sock);
    }
    if(cmd[0]==RPC_MAP) {
      ret = n_write;
    }
    if(cmd[0]==RPC_MAP_REQ || cmd[0]==RPC_CONNECT || cmd[0]==RPC_BIND) {
    	ret = get_retval();
    }
    if(cmd[0]==RPC_LISTEN || cmd[0]==RPC_GETSOCKNAME) {
    	/* Do Nothing */
    }
  }
  else {
    ret = -1;
  }
  pthread_mutex_unlock(&lock);
  return ret;
}



/* Check whether the socket is mapped to the service or not. We
need to know if this is a regular AF_LOCAL socket or an end of a socketpair
that the service uses. We don't want to keep state in the intercept, so
we simply ask the service via an RPC */
static int is_mapped_to_service(int sockfd)
{
  dwr(MSG_DEBUG,"is_mapped_to_service()\n");
  char cmd[BUF_SZ];
  memset(cmd, '\0', BUF_SZ);
  cmd[0] = RPC_MAP_REQ;
  memcpy(&cmd[1], &sockfd, sizeof(sockfd));
  return send_cmd(fdret_sock, cmd);
}

/*------------------------------------------------------------------------------
----------  Unix-domain socket lazy initializer (for fd-transfers)--------------
------------------------------------------------------------------------------*/

/* Sets up the connection pipes and sockets to the service */
static int init_service_connection()
{
  struct sockaddr_un addr;
  int tfd = -1, attempts = 0, conn_err = -1;
  const char *network_id;
  char af_sock_name[1024];

  network_id = getenv("ZT_NC_NETWORK");
  if (!network_id)
    return -1;
  strncpy(af_sock_name,network_id,sizeof(af_sock_name));
  instance_count++;

  dwr(MSG_DEBUG,"init_service_connection()\n");

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, af_sock_name, sizeof(addr.sun_path)-1);
  if((tfd = realsocket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    return -1;

  while(conn_err < 0 && attempts < SERVICE_CONNECT_ATTEMPTS) {
    conn_err = realconnect(tfd, (struct sockaddr*)&addr, sizeof(addr));
    if(conn_err < 0) {
      dwr(MSG_DEBUG,"re-attempting connection in %ds\n", 1+attempts);
      sleep(1);
    }
    else {
      dwr(MSG_DEBUG,"AF_UNIX connection established: %d\n", tfd);
      is_initialized = 1;
      int newtfd = realdup2(tfd, RPC_FD-instance_count);
      dwr(MSG_DEBUG,"dup'd to rpc_fd = %d\n", newtfd);
      close(tfd);
      return newtfd;
    }
    attempts++;
  }
  return -1;
}

/*------------------------------------------------------------------------------
------------------------  ctors and dtors (and friends)-------------------------
------------------------------------------------------------------------------*/

static void my_dest(void) __attribute__ ((destructor));
static void my_dest(void) {
  dwr(MSG_DEBUG,"closing connections to service...\n");
  pthread_mutex_destroy(&lock);
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
  if(pthread_mutex_init(&lock, NULL) != 0) {
    dwr(MSG_ERROR, "error while initializing service call mutex\n");
  }
  if(pthread_mutex_init(&loglock, NULL) != 0) {
    dwr(MSG_ERROR, "error while initializing log mutex mutex\n");
  }
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
  /*
  if(is_mapped_to_service(socket) < 0) { // First, check if the service manages this
    return realsetsockopt(socket, level, option_name, option_value, option_len);
  }
  */
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
  /*
  if(is_mapped_to_service(sockfd) < 0) { // First, check if the service manages this
    return realgetsockopt(sockfd, level, optname, optval, optlen);
  }
  */
  int err = realgetsockopt(sockfd, level, optname, optval, optlen);
  /* TODO: this condition will need a little more intelligence later on
   -- we will need to know if this fd is a local we are spoofing, or a true local */
  if(optname == SO_TYPE)
  {
    int* val = (int*)optval;
    *val = 2;
    optval = (void*)val;
  }
  if(err < 0){
    perror("setsockopt():\n");
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
  if(realsocket == NULL){
    dwr(MSG_ERROR, "socket(): SYMBOL NOT FOUND.\n");
    return -1;
  }
  dwr(MSG_DEBUG,"socket():\n");
  int err;
#ifdef CHECKS
  /* Check that type makes sense */
  int flags = socket_type & ~SOCK_TYPE_MASK;
  if (flags & ~(SOCK_CLOEXEC | SOCK_NONBLOCK)) {
      errno = EINVAL;
      handle_error("socket", "", -1);
      return -1;
  }
  socket_type &= SOCK_TYPE_MASK;
  /* Check protocol is in range */
  if (socket_family < 0 || socket_family >= NPROTO){
    errno = EAFNOSUPPORT;
    handle_error("socket", "", -1);
    return -1;
  }
  if (socket_type < 0 || socket_type >= SOCK_MAX) {
    errno = EINVAL;
    handle_error("socket", "", -1);
    return -1;
  }
  /* Check that we haven't hit the soft-limit file descriptors allowed */
  /* FIXME: Find number of open fds
  struct rlimit rl;
  getrlimit(RLIMIT_NOFILE, &rl);
  if(sockfd >= rl.rlim_cur){
    errno = EMFILE;
    return -1;
  }
  */
  /* TODO: detect ENFILE condition */
#endif
  char cmd[BUF_SZ];
  fdret_sock = !is_initialized ? init_service_connection() : fdret_sock;
  if(fdret_sock < 0) {
    dwr(MSG_DEBUG,"BAD service connection. exiting.\n");
    handle_error("socket", "", -1);
    exit(-1);
  }
  if(socket_family == AF_LOCAL
    || socket_family == AF_NETLINK
    || socket_family == AF_UNIX) {
      int err = realsocket(socket_family, socket_type, protocol);
      dwr(MSG_DEBUG,"realsocket, err = %d\n", err);
      handle_error("socket", "", err);
      return err;
  }
  /* Assemble and send RPC */
  struct socket_st rpc_st;
  rpc_st.socket_family = socket_family;
  rpc_st.socket_type = socket_type;
  rpc_st.protocol = protocol;
  rpc_st.__tid = syscall(SYS_gettid);
  memset(cmd, '\0', BUF_SZ);
  cmd[0] = RPC_SOCKET;
  memcpy(&cmd[1], &rpc_st, sizeof(struct socket_st));

  /* send command and get new fd */
  newfd = send_cmd(fdret_sock, cmd);
  if(newfd > 0)
  {
    dwr(MSG_DEBUG,"sending fd = %d to Service over (%d)\n", newfd, fdret_sock);
    /* send our local-fd number back to service so
     it can complete its mapping table entry */
    memset(cmd, '\0', BUF_SZ);
    cmd[0] = RPC_MAP;
    memcpy(&cmd[1], &newfd, sizeof(newfd));
  	/* send fd mapping and get confirmation */
  	err = send_cmd(fdret_sock, cmd);

	if(err > -1) {
	  errno = ERR_OK;
	  dwr(MSG_DEBUG, "RXd fd confirmation. Mapped!\n");
      return newfd; /* Mapping complete, everything is OK */
    }
    else{
    	dwr(MSG_DEBUG,"Error, service sent bad fd.\n");
    	return err; /* Mapping failed */
    }
  }
  else {
    dwr(MSG_DEBUG,"Error while receiving new fd.\n");
    return newfd;
  }
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
  /* print_addr(__addr); */
  struct sockaddr_in *connaddr;
  connaddr = (struct sockaddr_in *) __addr;

#ifdef CHECKS
  /* Check that this is a valid fd */
  if(fcntl(__fd, F_GETFD) < 0) {
    errno = EBADF;
    handle_error("connect", "EBADF", -1);
    return -1;
  }
  /* Check that it is a socket */
  int sock_type;
  socklen_t sock_type_len = sizeof(sock_type);
  if(getsockopt(__fd, SOL_SOCKET, SO_TYPE, (void *) &sock_type, &sock_type_len) < 0) {
    errno = ENOTSOCK;
    handle_error("connect", "ENOTSOCK", -1);
    return -1;
  }
  /* Check family */
  if (connaddr->sin_family < 0 || connaddr->sin_family >= NPROTO){
    errno = EAFNOSUPPORT;
    handle_error("connect", "EAFNOSUPPORT", -1);
    return -1;
  }
  /* FIXME: Check that address is in user space, return EFAULT ? */
#endif

  /* make sure we don't touch any standard outputs */
  if(__fd == STDIN_FILENO || __fd == STDOUT_FILENO || __fd == STDERR_FILENO){
    if (realconnect == NULL) {
      handle_error("connect", "Unresolved symbol [connect]", -1);
      exit(-1);
    }
    return(realconnect(__fd, __addr, __len));
  }

  if(__addr != NULL && (connaddr->sin_family == AF_LOCAL
    || connaddr->sin_family == PF_NETLINK
    || connaddr->sin_family == AF_NETLINK
    || connaddr->sin_family == AF_UNIX)) {
    int err = realconnect(__fd, __addr, __len);
    perror("connect():");
    /* handle_error("connect", "Cannot connect to local socket", err); */
    return err;
  }

  /* Assemble and send RPC */
  char cmd[BUF_SZ];
  memset(cmd, '\0', BUF_SZ);
  struct connect_st rpc_st;
  rpc_st.__tid = syscall(SYS_gettid);
  rpc_st.__fd = __fd;
  memcpy(&rpc_st.__addr, __addr, sizeof(struct sockaddr));
  memcpy(&rpc_st.__len, &__len, sizeof(socklen_t));
  cmd[0] = RPC_CONNECT;
  memcpy(&cmd[1], &rpc_st, sizeof(struct connect_st));
  return send_cmd(fdret_sock, cmd);
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
  /* print_addr(addr); */
#ifdef CHECKS
  /* Check that this is a valid fd */
  if(fcntl(sockfd, F_GETFD) < 0) {
    errno = EBADF;
    handle_error("bind", "EBADF", -1);
    return -1;
  }
  /* Check that it is a socket */
  int opt = -1;
  socklen_t opt_len;
  if(getsockopt(sockfd, SOL_SOCKET, SO_TYPE, (void *) &opt, &opt_len) < 0) {
    errno = ENOTSOCK;
    handle_error("bind", "ENOTSOCK", -1);
    return -1;
  }
#endif

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
  /* Assemble and send RPC */
  char cmd[BUF_SZ];
  struct bind_st rpc_st;
  rpc_st.sockfd = sockfd;
  rpc_st.__tid = syscall(SYS_gettid);
  memcpy(&rpc_st.addr, addr, sizeof(struct sockaddr));
  memcpy(&rpc_st.addrlen, &addrlen, sizeof(socklen_t));
  cmd[0]=RPC_BIND;
  memcpy(&cmd[1], &rpc_st, sizeof(struct bind_st));
  return send_cmd(fdret_sock, cmd);
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
#ifdef CHECKS
  if (flags & ~(SOCK_CLOEXEC | SOCK_NONBLOCK)) {
    errno = EINVAL;
    return -1;
  }
#endif
  int newfd = accept(sockfd, addr, addrlen);
  if(newfd > 0) {
    if(flags & SOCK_CLOEXEC)
      fcntl(newfd, F_SETFL, FD_CLOEXEC);
    if(flags & SOCK_NONBLOCK)
      fcntl(newfd, F_SETFL, O_NONBLOCK);
  }
  handle_error("accept4", "", newfd);
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
#ifdef CHECKS
  /* Check that this is a valid fd */
  if(fcntl(sockfd, F_GETFD) < 0) {
    return -1;
    errno = EBADF;
    dwr(MSG_DEBUG,"EBADF\n");
    handle_error("accept", "EBADF", -1);
    return -1;
  }
  /* Check that it is a socket */
  int opt;
  socklen_t opt_len;
  if(getsockopt(sockfd, SOL_SOCKET, SO_TYPE, (void *) &opt, &opt_len) < 0) {
    errno = ENOTSOCK;
    dwr(MSG_DEBUG,"ENOTSOCK\n");
    handle_error("accept", "ENOTSOCK", -1);
    return -1;
  }
  /* Check that this socket supports accept() */
  if(!(opt && (SOCK_STREAM | SOCK_SEQPACKET))) {
    errno = EOPNOTSUPP;
    dwr(MSG_DEBUG,"EOPNOTSUPP\n");
    handle_error("accept", "EOPNOTSUPP", -1);
    return -1;
  }
  /* Check that we haven't hit the soft-limit file descriptors allowed */
  struct rlimit rl;
  getrlimit(RLIMIT_NOFILE, &rl);
  if(sockfd >= rl.rlim_cur){
    errno = EMFILE;
    dwr(MSG_DEBUG,"EMFILE\n");
    handle_error("accept", "EMFILE", -1);
    return -1;
  }
  /* Check address length */
  if(addrlen < 0) {
    errno = EINVAL;
    dwr(MSG_DEBUG,"EINVAL\n");
    handle_error("accept", "EINVAL", -1);
    return -1;
  }
#endif

  /* redirect calls for standard I/O descriptors to kernel */
  if(sockfd == STDIN_FILENO || sockfd == STDOUT_FILENO || sockfd == STDERR_FILENO){
    dwr(MSG_DEBUG,"realaccept():\n");
    return(realaccept(sockfd, addr, addrlen));
  }

  if(addr)
    addr->sa_family = AF_INET;
    /* TODO: also get address info */

  char cmd[BUF_SZ];

  /* The following line is required for libuv/nodejs to accept connections properly,
  however, this has the side effect of causing certain webservers to max out the CPU
  in an accept loop */
  fcntl(sockfd, F_SETFL, SOCK_NONBLOCK);
  int new_conn_socket = get_new_fd(sockfd);

  if(new_conn_socket > 0)
  {
    dwr(MSG_DEBUG, "accept(): RX: fd = (%d) over (%d)\n", new_conn_socket, fdret_sock);
    /* Send our local-fd number back to service so it can complete its mapping table */
    memset(cmd, '\0', BUF_SZ);
    cmd[0] = RPC_MAP;
    memcpy(&cmd[1], &new_conn_socket, sizeof(new_conn_socket));

    dwr(MSG_DEBUG, "accept(): sending perceived fd (%d) to service.\n", new_conn_socket);
    int n_write = send_cmd(fdret_sock, cmd);

    if(n_write < 0) {
      errno = ECONNABORTED;
      handle_error("accept", "ECONNABORTED - Error sending perceived FD to service", -1);
      return -1;
    }
    errno = ERR_OK;
    dwr(MSG_DEBUG,"accept()=%d\n", new_conn_socket);
    return new_conn_socket; /* OK */
  }
  errno = EAGAIN; /* necessary? */
  handle_error("accept", "EAGAIN - Error reading signal byte from service", -1);
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

  #ifdef CHECKS
  /* Check that this is a valid fd */
  if(fcntl(sockfd, F_GETFD) < 0) {
    errno = EBADF;
    handle_error("listen", "EBADF", -1);
    return -1;
  }
  /* Check that it is a socket */
  if(getsockopt(sockfd, SOL_SOCKET, SO_TYPE, (void *) &sock_type, &sock_type_len) < 0) {
    errno = ENOTSOCK;
    handle_error("listen", "ENOTSOCK", -1);
    return -1;
  }
  /* Check that this socket supports accept() */
  if(!(sock_type && (SOCK_STREAM | SOCK_SEQPACKET))) {
    errno = EOPNOTSUPP;
    handle_error("listen", "EOPNOTSUPP", -1);
    return -1;
  }
  #endif

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
  char cmd[BUF_SZ];
  memset(cmd, '\0', BUF_SZ);
  struct listen_st rpc_st;
  rpc_st.sockfd = sockfd;
  rpc_st.backlog = backlog;
  rpc_st.__tid = syscall(SYS_gettid);
  cmd[0] = RPC_LISTEN;
  memcpy(&cmd[1], &rpc_st, sizeof(struct listen_st));
  return send_cmd(fdret_sock, cmd);
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
  checkpid();
  return err;
}

/*------------------------------------------------------------------------------
------------------------------------- close() ----------------------------------
------------------------------------------------------------------------------*/

/* int fd */
int close(CLOSE_SIG)
{
  dwr(MSG_DEBUG, "close(%d)\n", fd);
  if(realclose == NULL){
    checkpid(); // Add for nginx support, remove for apache support.
    dwr(MSG_ERROR, "close(%d): SYMBOL NOT FOUND.\n", fd);
    return -1;
  }
  if(fd == fdret_sock)
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
    if(oldfd == fdret_sock) {
    dwr(MSG_DEBUG,"client application attempted to dup2 RPC socket (%d). This is not allowed.\n", oldfd);
    errno = EBADF;
    return -1;
  }
  //if(oldfd != STDIN_FILENO && oldfd != STDOUT_FILENO && oldfd != STDERR_FILENO)
  //  if(newfd != STDIN_FILENO && newfd != STDOUT_FILENO && newfd != STDERR_FILENO)
      return realdup2(oldfd, newfd);
  return -1;
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
#ifdef DEBUG
  /* Only do this check if we want to debug the intercept, otherwise, dont mess with
   the client application's logging methods */
  if(newfd == STDIN_FILENO || newfd == STDOUT_FILENO || newfd == STDERR_FILENO)
    return newfd; /* FIXME: This is to prevent httpd from dup'ing over our stderr
                   and preventing us from debugging */
  else
#endif
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
  /* return realgetsockname(sockfd, addr, addrlen); */
  /* assemble command */
  char cmd[BUF_SZ];
  struct getsockname_st rpc_st;
  rpc_st.sockfd = sockfd;
  memcpy(&rpc_st.addr, addr, sizeof(struct sockaddr));
  memcpy(&rpc_st.addrlen, &addrlen, sizeof(socklen_t));
  cmd[0] = RPC_GETSOCKNAME;
  memcpy(&cmd[1], &rpc_st, sizeof(struct getsockname_st));

  send_cmd(fdret_sock, cmd);

  char addrbuf[sizeof(struct sockaddr)];
  memset(addrbuf, '\0', sizeof(struct sockaddr));
  read(fdret_sock, &addrbuf, sizeof(struct sockaddr)); /* read address from service */
  memcpy(addr, addrbuf, sizeof(struct sockaddr));
  *addrlen = sizeof(struct sockaddr);

  struct sockaddr_in *connaddr;
  connaddr = (struct sockaddr_in *) &addr;

  int ip = connaddr->sin_addr.s_addr;
  unsigned char d[4];
  d[0] = ip & 0xFF;
  d[1] = (ip >>  8) & 0xFF;
  d[2] = (ip >> 16) & 0xFF;
  d[3] = (ip >> 24) & 0xFF;

  int port = connaddr->sin_port;
  dwr(MSG_ERROR, " handle_getsockname(): returning address: %d.%d.%d.%d: %d\n", d[0],d[1],d[2],d[3], port);
  return 0;
}

/*------------------------------------------------------------------------------
------------------------------------ syscall() ---------------------------------
------------------------------------------------------------------------------*/

long syscall(SYSCALL_SIG){
  if(realsyscall == NULL){
    dwr(MSG_ERROR, "syscall(): SYMBOL NOT FOUND.\n");
    return -1;
  }
  dwr(MSG_DEBUG_EXTRA,"syscall(%u, ...):\n", number);

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
    if(err == -EBADF)
      err = -EAGAIN;
    return err;
  }
#endif
  return realsyscall(number,a,b,c,d,e,f);
}
