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

/* Name used in err msgs    */
char *progname = "";

#include <stdio.h>
#include <dlfcn.h>
#include <strings.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>

#include <netinet/in.h>
#include <net/if.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>

#include <poll.h>
#include <pthread.h>
#include <unistd.h>

/* For NPs */
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>

/* for mmap */
#include <sys/mman.h>

#ifdef USE_SOCKS_DNS
  #include <resolv.h>
#endif

#include "Intercept.h"
#include "Common.h"

#ifdef CHECKS
  //#include <sys/time.h>
  #include <sys/resource.h>
  #include <linux/net.h> /* for NPROTO */

  #define SOCK_MAX (SOCK_PACKET + 1)
  #define SOCK_TYPE_MASK 0xf
#endif

/* Global Declarations */
#ifdef USE_SOCKS_DNS
static int (*realresinit)(void);
#endif
static int (*realconnect)(CONNECT_SIG);
static int (*realselect)(SELECT_SIG);
static int (*realpoll)(POLL_SIG);
static int (*realbind)(BIND_SIG);
static int (*realaccept)(ACCEPT_SIG);
static int (*reallisten)(LISTEN_SIG);
static int (*realsocket)(SOCKET_SIG);
static int (*realsetsockopt)(SETSOCKOPT_SIG);
static int (*realgetsockopt)(GETSOCKOPT_SIG);
static int (*realaccept4)(ACCEPT4_SIG);

/* Exported Function Prototypes */
void my_init(void);
int connect(CONNECT_SIG);
int select(SELECT_SIG);
int poll(POLL_SIG);
int close(CLOSE_SIG);
int bind(BIND_SIG);
int accept(ACCEPT_SIG);
int listen(LISTEN_SIG);
int socket(SOCKET_SIG);
int setsockopt(SETSOCKOPT_SIG);
int getsockopt(GETSOCKOPT_SIG);
int accept4(ACCEPT4_SIG);

#ifdef USE_SOCKS_DNS
  int res_init(void);
#endif

int connect_to_service(void);
int init_service_connection();
void dwr(const char *fmt, ...);
void load_symbols(void);
void set_up_intercept();
int checkpid();


#define BUF_SZ                    32
#define SERVICE_CONNECT_ATTEMPTS  30
#define ERR_OK                    0

ssize_t sock_fd_read(int sock, void *buf, ssize_t bufsize, int *fd);

/* threading */
pthread_mutex_t lock;
pthread_mutex_t loglock;


/*------------------------------------------------------------------------------
------------------- Intercept<--->Service Comm mechanisms-----------------------
------------------------------------------------------------------------------*/

static int is_initialized = 0;
static int fdret_sock; // used for fd-transfers
static int newfd; // used for "this_end" socket
static int thispid;
static char* af_sock_name  = "/tmp/.ztnc_e5cd7a9e1c5311ab";

/*
 * Check for forking
 */
int checkpid() {
  if(thispid != getpid()) {
    printf("clone/fork detected. re-initializing this instance.\n");
    set_up_intercept();
    fdret_sock = init_service_connection();
    thispid = getpid();
  }
  return 0;
}

/*
 * Sends an RPC command to the service
 */
void send_command(int rpc_fd, char *cmd)
{
  int n_write = write(rpc_fd, cmd, BUF_SZ);
  if(n_write < 0){
    dwr("Error writing command to service (CMD = %d)\n", cmd[0]);
    errno = 0;
  }
}

/*
 * Reads a return value from the service and sets errno (if applicable)
 */
int get_retval()
{
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
  dwr("unable to read connect: return value\n");
  return -1;
}


/*------------------------------------------------------------------------------
----------  Unix-domain socket lazy initializer (for fd-transfers)--------------
------------------------------------------------------------------------------*/

/* Sets up the connection pipes and sockets to the service */
int init_service_connection()
{
  if(!is_initialized)
  {
    struct sockaddr_un addr;
    int tfd = -1, attempts = 0, conn_err = -1;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, af_sock_name, sizeof(addr.sun_path)-1);

    if ( (tfd = realsocket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
      perror("socket error");
      exit(-1);
    }
    while(conn_err < 0 && attempts < SERVICE_CONNECT_ATTEMPTS)
    {
      //dwr("trying connection (%d): %s\n", tfd, af_sock_name);
      conn_err = realconnect(tfd, (struct sockaddr*)&addr, sizeof(addr));

      if(conn_err < 0) {
        dwr("re-attempting connection in %ds\n", 1+attempts);
        sleep(1);
      }
      else {
        dwr("AF_UNIX connection established: %d\n", tfd);
        is_initialized = 1;
        return tfd;
      }
      attempts++;
    }
  }
  return -1;
}

/*------------------------------------------------------------------------------
------------------------  ctors and dtors (and friends)-------------------------
------------------------------------------------------------------------------*/

void my_dest(void) __attribute__ ((destructor));
void my_dest(void) {

  dwr("closing connections to service...\n");
  close(fdret_sock);
  pthread_mutex_destroy(&lock);
}


void load_symbols(void)
{
#ifdef USE_OLD_DLSYM
  void *lib;
#endif
 /* possibly add check to beginning of each method to avoid needing to cll the constructor */
  if(thispid == getpid()) {
    dwr("detected duplicate call to global ctor (pid=%d).\n", thispid);
  }
  //dwr(" -- pid = %d\n", getpid());
	//dwr(" -- uid = %d\n", getuid());
  thispid = getpid();

#ifndef USE_OLD_DLSYM
  realconnect = dlsym(RTLD_NEXT, "connect");
  realbind = dlsym(RTLD_NEXT, "bind");
  realaccept = dlsym(RTLD_NEXT, "accept");
  reallisten = dlsym(RTLD_NEXT, "listen");
  realsocket = dlsym(RTLD_NEXT, "socket");
  realbind = dlsym(RTLD_NEXT, "bind");
  realpoll = dlsym(RTLD_NEXT, "poll");
  realselect = dlsym(RTLD_NEXT, "select");
	realsetsockopt = dlsym(RTLD_NEXT, "setsockopt");
  realgetsockopt = dlsym(RTLD_NEXT, "getsockopt");
  realaccept4 = dlsym(RTLD_NEXT, "accept4");
#ifdef USE_SOCKS_DNS
  realresinit = dlsym(RTLD_NEXT, "res_init");
#endif

#else
  lib = dlopen(LIBCONNECT, RTLD_LAZY);
  realconnect = dlsym(lib, "connect");
  realbind = dlsym(lib, "bind");
  realaccept = dlsym(lib, "accept");
  reallisten = dlsym(lib, "listen");
  realsocket = dlsym(lib, "socket");
	realpoll = dlsym(lib, "poll");
  realselect = dlsym(lib, "select");
	realsetsockopt = dlsym(lib, "setsockopt");
  realgetsockopt = dlsym(lib, "getsockopt");
  realaccept4 = dlsym(lib), "accept4");
#ifdef USE_SOCKS_DNS
  realresinit = dlsym(lib, "res_init");
#endif
  dlclose(lib);
  lib = dlopen(LIBC, RTLD_LAZY);
  dlclose(lib);
#endif
}

/* Private Function Prototypes */
void _init(void) __attribute__ ((constructor));
void _init(void) {
  set_up_intercept();
}

/* get symbols and initialize mutexes */
void set_up_intercept()
{
  load_symbols();
  if(pthread_mutex_init(&lock, NULL) != 0) {
    printf("error while initializing service call mutex\n");
  }
  if(pthread_mutex_init(&loglock, NULL) != 0) {
    printf("error while initializing log mutex mutex\n");
  }
}


/*------------------------------------------------------------------------------
--------------------------------- setsockopt() ---------------------------------
------------------------------------------------------------------------------*/
/* int socket, int level, int option_name, const void *option_value, socklen_t option_len */
int setsockopt(SETSOCKOPT_SIG)
{
  /* make sure we don't touch any standard outputs */
  if(socket == STDIN_FILENO || socket == STDOUT_FILENO || socket == STDERR_FILENO)
    return(realsetsockopt(socket, level, option_name, option_value, option_len));
  int err = realsetsockopt(socket, level, option_name, option_value, option_len);
  if(err < 0){
    //perror("setsockopt():\n");
  }
  return 0;
}


/*------------------------------------------------------------------------------
--------------------------------- getsockopt() ---------------------------------
------------------------------------------------------------------------------*/
/* int sockfd, int level, int optname, void *optval, socklen_t *optlen */

int getsockopt(GETSOCKOPT_SIG)
{
  // make sure we don't touch any standard outputs
  int err = realgetsockopt(sockfd, level, optname, optval, optlen);

  // FIXME: this condition will need a little more intelligence later on
  // -- we will need to know if this fd is a local we are spoofing, or a true local
  if(optname == SO_TYPE)
  {
    int* val = (int*)optval;
    *val = 2;
    optval = (void*)val;
  }
  if(err < 0){
    //perror("setsockopt():\n");
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
  int err;
#ifdef CHECKS
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
  /* Check that we haven't hit the soft-limit file descriptors allowed */
  /* FIXME: Find number of open fds
  struct rlimit rl;
  getrlimit(RLIMIT_NOFILE, &rl);
  if(sockfd >= rl.rlim_cur){
    errno = EMFILE;
    return -1;
  }
  */
  /* FIXME: detect ENFILE condition */
#endif

  char cmd[BUF_SZ];
  fdret_sock = !is_initialized ? init_service_connection() : fdret_sock;

  if(socket_family == AF_LOCAL
    || socket_family == AF_NETLINK
    || socket_family == AF_UNIX) {
    return realsocket(socket_family, socket_type, protocol);
  }

  /* Assemble and route command */
  struct socket_st rpc_st;
  rpc_st.socket_family = socket_family;
  rpc_st.socket_type = socket_type;
  rpc_st.protocol = protocol;
  rpc_st.__tid = syscall(SYS_gettid);

  memset(cmd, '\0', BUF_SZ);
  cmd[0] = RPC_SOCKET;
  memcpy(&cmd[1], &rpc_st, sizeof(struct socket_st));
  pthread_mutex_lock(&lock);
  send_command(fdret_sock, cmd);

  /* get new fd */
  char rbuf[16];
  ssize_t sz = sock_fd_read(fdret_sock, rbuf, sizeof(rbuf), &newfd);
  if(sz > 0)
  {
    /* send our local-fd number back to service so
     it can complete its mapping table entry */
    memset(cmd, '\0', BUF_SZ);
    cmd[0] = RPC_FD_MAP_COMPLETION;
    memcpy(&cmd[1], &newfd, sizeof(newfd));
    if(newfd > -1) {
      send_command(fdret_sock, cmd);
      pthread_mutex_unlock(&lock);
      errno = ERR_OK; // OK
      return newfd;
    }
    else { // Try to read retval+errno since we RXed a bad fd
      dwr("Error, service sent bad fd.\n");
      err = get_retval();
      pthread_mutex_unlock(&lock);
      return err;
    }
  }
  else {
    dwr("Error while receiving new FD.\n");
    err = get_retval();
    pthread_mutex_unlock(&lock);
    return err;
  }
}

/*------------------------------------------------------------------------------
---------------------------------- connect() -----------------------------------
------------------------------------------------------------------------------*/

/* int __fd, const struct sockaddr * __addr, socklen_t __len
   connect() intercept function */
int connect(CONNECT_SIG)
{
  struct sockaddr_in *connaddr;
  connaddr = (struct sockaddr_in *) __addr;

#ifdef CHECKS
  /* Check that this is a valid fd */
  if(fcntl(__fd, F_GETFD) < 0) {
    return -1;
    errno = EBADF;
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
#endif

  /* make sure we don't touch any standard outputs */
  if(__fd == STDIN_FILENO || __fd == STDOUT_FILENO || __fd == STDERR_FILENO){
    if (realconnect == NULL) {
      dwr("Unresolved symbol: connect(). Library is exiting.\n");
      exit(-1);
    }
    return(realconnect(__fd, __addr, __len));
  }

  if(__addr != NULL && (connaddr->sin_family == AF_LOCAL
    || connaddr->sin_family == PF_NETLINK
    || connaddr->sin_family == AF_NETLINK
    || connaddr->sin_family == AF_UNIX)) {
    int err = realconnect(__fd, __addr, __len);
    return err;
  }

  /* assemble and route command */
  int err;
  char cmd[BUF_SZ];
  memset(cmd, '\0', BUF_SZ);
  struct connect_st rpc_st;
  rpc_st.__tid = syscall(SYS_gettid);
  rpc_st.__fd = __fd;
  memcpy(&rpc_st.__addr, __addr, sizeof(struct sockaddr));
  memcpy(&rpc_st.__len, &__len, sizeof(socklen_t));
  cmd[0] = RPC_CONNECT;
  memcpy(&cmd[1], &rpc_st, sizeof(struct connect_st));
  pthread_mutex_lock(&lock);
  send_command(fdret_sock, cmd);
  err = get_retval();
  pthread_mutex_unlock(&lock);
  return err;
}

/*------------------------------------------------------------------------------
---------------------------------- select() ------------------------------------
------------------------------------------------------------------------------*/

/* int n, fd_set *readfds, fd_set *writefds,
fd_set *exceptfds, struct timeval *timeout */
int select(SELECT_SIG)
{
  return realselect(n, readfds, writefds, exceptfds, timeout);
}

/*------------------------------------------------------------------------------
----------------------------------- poll() -------------------------------------
------------------------------------------------------------------------------*/

/* struct pollfd *__fds, nfds_t __nfds, int __timeout */
int poll(POLL_SIG)
{
  return realpoll(__fds, __nfds, __timeout);
}

/*------------------------------------------------------------------------------
------------------------------------ bind() ------------------------------------
------------------------------------------------------------------------------*/

/* int sockfd, const struct sockaddr *addr, socklen_t addrlen
   bind() intercept function */
int bind(BIND_SIG)
{
#ifdef CHECKS
  /* Check that this is a valid fd */
  if(fcntl(sockfd, F_GETFD) < 0) {
    return -1;
    errno = EBADF;
  }
  /* Check that it is a socket */
  int sock_type = -1;
  socklen_t sock_type_len = sizeof(sock_type);
  if(getsockopt(sockfd, SOL_SOCKET, SO_TYPE, (void *) &sock_type, &sock_type_len) < 0) {
    errno = ENOTSOCK;
    return -1;
  }
#endif

  int err;
  /* make sure we don't touch any standard outputs */
  if(sockfd == STDIN_FILENO || sockfd == STDOUT_FILENO || sockfd == STDERR_FILENO)
    return(realbind(sockfd, addr, addrlen));

  /* If local, just use normal syscall */
  struct sockaddr_in *connaddr;
  connaddr = (struct sockaddr_in *) addr;

  if (addr != NULL && (connaddr->sin_family == AF_LOCAL
    || connaddr->sin_family == PF_NETLINK
    || connaddr->sin_family == AF_NETLINK
    || connaddr->sin_family == AF_UNIX))
  {
      if(realbind == NULL) {
        dwr("Unresolved symbol: bind(). Library is exiting.\n");
        exit(-1);
      }
      return(realbind(sockfd, addr, addrlen));
  }
  /* Assemble and route command */
  char cmd[BUF_SZ];
  struct bind_st rpc_st;
  rpc_st.sockfd = sockfd;
  rpc_st.__tid = syscall(SYS_gettid);
  memcpy(&rpc_st.addr, addr, sizeof(struct sockaddr));
  memcpy(&rpc_st.addrlen, &addrlen, sizeof(socklen_t));
  cmd[0]=RPC_BIND;
  memcpy(&cmd[1], &rpc_st, sizeof(struct bind_st));
  pthread_mutex_lock(&lock);
  send_command(fdret_sock, cmd);
  err = get_retval();
  pthread_mutex_unlock(&lock);
  errno = ERR_OK;
  return err;
}


/*------------------------------------------------------------------------------
----------------------------------- accept4() ----------------------------------
------------------------------------------------------------------------------*/


/* int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags */
int accept4(ACCEPT4_SIG)
{
#ifdef CHECKS
  if (flags & ~(SOCK_CLOEXEC | SOCK_NONBLOCK)) {
    errno = EINVAL;
    return -1;
  }
#endif
  return accept(sockfd, addr, addrlen);
}


/*------------------------------------------------------------------------------
----------------------------------- accept() -----------------------------------
------------------------------------------------------------------------------*/

/* int sockfd struct sockaddr *addr, socklen_t *addrlen
   accept() intercept function */
int accept(ACCEPT_SIG)
{
#ifdef CHECKS
  /* Check that this is a valid fd */
  if(fcntl(sockfd, F_GETFD) < 0) {
    return -1;
    errno = EBADF;
  }
  /* Check that it is a socket */
  int sock_type;
  socklen_t sock_type_len = sizeof(sock_type);
  if(getsockopt(sockfd, SOL_SOCKET, SO_TYPE, (void *) &sock_type, &sock_type_len) < 0) {
    errno = ENOTSOCK;
    return -1;
  }
  /* Check that this socket supports accept() */
  if(!(sock_type && (SOCK_STREAM | SOCK_SEQPACKET))) {
    errno = EOPNOTSUPP;
    return -1;
  }
  /* Check that we haven't hit the soft-limit file descriptors allowed */
  struct rlimit rl;
  getrlimit(RLIMIT_NOFILE, &rl);
  if(sockfd >= rl.rlim_cur){
    errno = EMFILE;
    return -1;
  }
#endif

  /* make sure we don't touch any standard outputs */
  if(sockfd == STDIN_FILENO || sockfd == STDOUT_FILENO || sockfd == STDERR_FILENO)
    return(realaccept(sockfd, addr, addrlen));

  addr->sa_family = AF_INET;
  /* TODO: also get address info */

  char cmd[BUF_SZ];
  if(realaccept == NULL) {
    dwr( "Unresolved symbol: accept()\n");
    return -1;
  }

  char rbuf[16], c[1];
  int new_conn_socket;
  int n = read(sockfd, c, sizeof(c)); // Read signal byte
  if(n > 0)
  {
    ssize_t size = sock_fd_read(fdret_sock, rbuf, sizeof(rbuf), &new_conn_socket);
    if(size > 0) {
      /* Send our local-fd number back to service so it can complete its mapping table */
      memset(cmd, '\0', BUF_SZ);
      cmd[0] = RPC_FD_MAP_COMPLETION;
      memcpy(&cmd[1], &new_conn_socket, sizeof(new_conn_socket));
      pthread_mutex_lock(&lock);
      int n_write = write(fdret_sock, cmd, BUF_SZ);
      if(n_write < 0) {
        dwr("Error sending perceived FD to service.\n");
        errno = ECONNABORTED; // FIXME: Closest match, service unreachable
        return -1;
      }
      pthread_mutex_unlock(&lock);
      //errno = ERR_OK;
      return new_conn_socket; // OK
    }
    else {
      dwr("Error receiving new FD from service.\n");
      errno = ECONNABORTED; // FIXME: Closest match, service unreachable
      return -1;
    }
  }
  dwr("Error reading signal byte from service.\n");
  //errno = EWOULDBLOCK;
  errno = ECONNABORTED; // FIXME: Closest match, service unreachable
  return -1;
}


/*------------------------------------------------------------------------------
------------------------------------- listen()----------------------------------
------------------------------------------------------------------------------*/

/* int sockfd, int backlog
   listen() intercept function */
int listen(LISTEN_SIG)
{
  #ifdef CHECKS
  /* Check that this is a valid fd */
  if(fcntl(sockfd, F_GETFD) < 0) {
    return -1;
    errno = EBADF;
  }
  /* Check that it is a socket */
  int sock_type;
  socklen_t sock_type_len = sizeof(sock_type);
  if(getsockopt(sockfd, SOL_SOCKET, SO_TYPE, (void *) &sock_type, &sock_type_len) < 0) {
    errno = ENOTSOCK;
    return -1;
  }
  /* Check that this socket supports accept() */
  if(!(sock_type && (SOCK_STREAM | SOCK_SEQPACKET))) {
    errno = EOPNOTSUPP;
    return -1;
  }
  #endif

  int err;

  /* make sure we don't touch any standard outputs */
  if(sockfd == STDIN_FILENO || sockfd == STDOUT_FILENO || sockfd == STDERR_FILENO)
    return(reallisten(sockfd, backlog));

  char cmd[BUF_SZ];
  /* Assemble and route command */
  memset(cmd, '\0', BUF_SZ);
  struct listen_st rpc_st;
  rpc_st.sockfd = sockfd;
  rpc_st.backlog = backlog;
  rpc_st.__tid = syscall(SYS_gettid);
  cmd[0] = RPC_LISTEN;
  memcpy(&cmd[1], &rpc_st, sizeof(struct listen_st));
  pthread_mutex_lock(&lock);
  send_command(fdret_sock, cmd);
  err = get_retval();
  pthread_mutex_unlock(&lock);
  errno = ERR_OK;
  return err;
}
