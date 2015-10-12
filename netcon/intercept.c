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

#include "intercept.h"
#include "common.h"

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

/* defined in unistd.h, but we don't include it because
it conflicts with our overriden symbols for read/write */
#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2

#define BUF_SZ                    1024
#define SERVICE_CONNECT_ATTEMPTS  30

#define ERR_OK          0

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
static char* af_sock_name  = "/tmp/.ztnc_e5cd7a9e1c5311ab";

static int thispid;

/*
*
* Check for forking
*
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
*
* Reads a return value from the service and sets errno (if applicable)
*
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
    else {
      dwr("unable to read connect: return value\n");
      return -1;
    }
  }
}

#define SLEEP_TIME 0

/*------------------------------------------------------------------------------
----------  Unix-domain socket lazy initializer (for fd-transfers)--------------
------------------------------------------------------------------------------*/

/* Sets up the connection pipes and sockets to the service */
int init_service_connection()
{
  usleep(SLEEP_TIME);
  if(!is_initialized)
  {
    struct sockaddr_un addr;
    int tfd = -1;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, af_sock_name, sizeof(addr.sun_path)-1);

    int attempts = 0;
    int conn_err = -1;

    if ( (tfd = realsocket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
      perror("socket error");
      exit(-1);
    }

    while(conn_err < 0 && attempts < SERVICE_CONNECT_ATTEMPTS)
    {
      dwr("trying connection (%d): %s\n", tfd, af_sock_name);
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
  dwr(" -- pid = %d\n", getpid());
	dwr(" -- uid = %d\n", getuid());
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
------------------------- ioctl(), fcntl(), setsockopt()------------------------
------------------------------------------------------------------------------*/

char *cmd_to_str(int cmd)
{
	switch(cmd)
	{
		case F_DUPFD:
			return "F_DUPFD";
		case F_GETFD:
			return "F_GETFD";
		case F_SETFD:
			return "F_SETFD";
		case F_GETFL:
			return "F_GETFL";
		case F_SETFL:
			return "F_SETFL";
		case F_GETLK:
			return "F_GETLK";
		case F_SETLK:
			return "F_SETLK";
		case F_SETLKW:
			return "F_SETLKW";
		default:
			return "?";
	}
	return "?";
}

void arg_to_str(int arg)
{
	if(arg & O_RDONLY) dwr("O_RDONLY ");
	if(arg & O_WRONLY) dwr("O_WRONLY ");
	if(arg & O_RDWR) dwr("O_RDWR ");
	if(arg & O_CREAT) dwr("O_CREAT ");
	if(arg & O_EXCL) dwr("O_EXCL ");
	if(arg & O_NOCTTY) dwr("O_NOCTTY ");
	if(arg & O_TRUNC) dwr("O_TRUNC ");
	if(arg & O_APPEND) dwr("O_APPEND ");
	if(arg & O_ASYNC) dwr("O_ASYNC ");
	if(arg & O_DIRECT) dwr("O_DIRECT ");
	if(arg & O_NOATIME) dwr("O_NOATIME ");
	if(arg & O_NONBLOCK) dwr("O_NONBLOCK ");
	if(arg & O_DSYNC) dwr("O_DSYNC ");
	if(arg & O_SYNC) dwr("O_SYNC ");
}

char* level_to_str(int level)
{
	switch(level)
	{
		case SOL_SOCKET:
			return "SOL_SOCKET";
		case IPPROTO_TCP:
			return "IPPROTO_TCP";
		default:
			return "?";
	}
	return "?";
}

char* option_name_to_str(int opt)
{
	if(opt == SO_DEBUG) return "SO_DEBUG";
	if(opt == SO_BROADCAST) return "SO_BROADCAST";
	if(opt == SO_BINDTODEVICE) return "SO_BINDTODEVICE";
	if(opt == SO_REUSEADDR) return "SO_REUSEADDR";
	if(opt == SO_KEEPALIVE) return "SO_KEEPALIVE";
	if(opt == SO_LINGER) return "SO_LINGER";
	if(opt == SO_OOBINLINE) return "SO_OOBINLINE";
	if(opt == SO_SNDBUF) return "SO_SNDBUF";
	if(opt == SO_RCVBUF) return "SO_RCVBUF";
	if(opt == SO_DONTROUTE) return "SO_DONTROUTEO_ASYNC";
	if(opt == SO_RCVLOWAT) return "SO_RCVLOWAT";
	if(opt == SO_RCVTIMEO) return "SO_RCVTIMEO";
	if(opt == SO_SNDLOWAT) return "SO_SNDLOWAT";
	if(opt == SO_SNDTIMEO)return  "SO_SNDTIMEO";
	return "?";
}

/*------------------------------------------------------------------------------
--------------------------------- setsockopt() ---------------------------------
------------------------------------------------------------------------------*/
/* int socket, int level, int option_name, const void *option_value, socklen_t option_len */
int setsockopt(SETSOCKOPT_SIG)
{
#ifdef DUMMY
  dwr("setsockopt(%d)\n", socket);
  return realsetsockopt(socket, level, option_name, option_value, option_len);

#else
  /* make sure we don't touch any standard outputs */
  if(socket == STDIN_FILENO || socket == STDOUT_FILENO || socket == STDERR_FILENO)
    return(realsetsockopt(socket, level, option_name, option_value, option_len));
  int err = realsetsockopt(socket, level, option_name, option_value, option_len);
  if(err < 0){
    //perror("setsockopt():\n");
  }
  return 0;
#endif
}


/*------------------------------------------------------------------------------
--------------------------------- getsockopt() ---------------------------------
------------------------------------------------------------------------------*/
/* int sockfd, int level, int optname, void *optval, socklen_t *optlen */

int getsockopt(GETSOCKOPT_SIG)
{
#ifdef DUMMY
  dwr("getsockopt(%d)\n", sockfd);
  return realgetsockopt(sockfd, level, optname, optval, optlen);

#else
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
#endif
}


/*------------------------------------------------------------------------------
---------------------------------- shutdown() ----------------------------------
------------------------------------------------------------------------------*/

void shutdown_arg_to_str(int arg)
{
	if(arg & O_RDONLY) dwr("O_RDONLY ");
	if(arg & O_WRONLY) dwr("O_WRONLY ");
	if(arg & O_RDWR) dwr("O_RDWR ");
	if(arg & O_CREAT) dwr("O_CREAT ");
	if(arg & O_EXCL) dwr("O_EXCL ");
	if(arg & O_NOCTTY) dwr("O_NOCTTY ");
	if(arg & O_TRUNC) dwr("O_TRUNC ");
	if(arg & O_APPEND) dwr("O_APPEND ");
	if(arg & O_ASYNC) dwr("O_ASYNC ");
	if(arg & O_DIRECT) dwr("O_DIRECT ");
	if(arg & O_NOATIME) dwr("O_NOATIME ");
	if(arg & O_NONBLOCK) dwr("O_NONBLOCK ");
	if(arg & O_DSYNC) dwr("O_DSYNC ");
	if(arg & O_SYNC) dwr("O_SYNC ");
}

/*------------------------------------------------------------------------------
----------------------------------- socket() -----------------------------------
------------------------------------------------------------------------------*/

void sock_type_to_str(int arg)
{
	if(arg == SOCK_STREAM) printf("SOCK_STREAM ");
  if(arg == SOCK_DGRAM) printf("SOCK_DGRAM ");
  if(arg == SOCK_SEQPACKET) printf("SOCK_SEQPACKET ");
  if(arg == SOCK_RAW) printf("SOCK_RAW ");
  if(arg == SOCK_RDM) printf("SOCK_RDM ");
  if(arg == SOCK_PACKET) printf("SOCK_PACKET ");
  if(arg & SOCK_NONBLOCK) printf("| SOCK_NONBLOCK ");
  if(arg & SOCK_CLOEXEC) printf("| SOCK_CLOEXEC ");
}

void sock_domain_to_str(int domain)
{
  if(domain == AF_UNIX) printf("AF_UNIX ");
  if(domain == AF_LOCAL) printf("AF_LOCAL ");
  if(domain == AF_INET) printf("AF_INET ");
  if(domain == AF_INET6) printf("AF_INET6 ");
  if(domain == AF_IPX) printf("AF_IPX ");
  if(domain == AF_NETLINK) printf("AF_NETLINK ");
  if(domain == AF_X25) printf("AF_X25 ");
  if(domain == AF_AX25) printf("AF_AX25 ");
  if(domain == AF_ATMPVC) printf("AF_ATMPVC ");
  if(domain == AF_APPLETALK) printf("AF_APPLETALK ");
  if(domain == AF_PACKET) printf("AF_PACKET ");
}

/* int socket_family, int socket_type, int protocol
   socket() intercept function */
int socket(SOCKET_SIG)
{
#ifdef DUMMY
  dwr("socket(fam=%d, type=%d, prot=%d)\n", socket_family, socket_type, protocol);
  return realsocket(socket_family, socket_type, protocol);

#else
  char cmd[BUF_SZ];
  fdret_sock = !is_initialized ? init_service_connection() : fdret_sock;

  if(socket_family == AF_LOCAL
    || socket_family == AF_NETLINK
    || socket_family == AF_UNIX) {
    int err = realsocket(socket_family, socket_type, protocol);
    return err;
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
  write(fdret_sock,cmd, BUF_SZ);

  /* get new fd */
  char gmybuf[16];
  ssize_t size = sock_fd_read(fdret_sock, gmybuf, sizeof(gmybuf), &newfd);
  if(size > 0)
  {
    /* send our local-fd number back to service so
     it can complete its mapping table entry */
    memset(cmd, '\0', BUF_SZ);
    cmd[0] = RPC_FD_MAP_COMPLETION;
    memcpy(&cmd[1], &newfd, sizeof(newfd));
    if(newfd > -1) {
      int n_write = write(fdret_sock, cmd, BUF_SZ);
      if(n_write < 0) {
        dwr("Error writing perceived FD to service.\n");
        return get_retval();
      }
      pthread_mutex_unlock(&lock);
      errno = ERR_OK;
      return newfd;
    }
    else { // Try to read retval+errno since we RXed a bad fd
      dwr("Error, service sent bad fd.\n");
      return get_retval();
    }
  }
  else {
    dwr("Error while receiving new FD.\n");
    pthread_mutex_unlock(&lock);
    return get_retval();
  }
  return realsocket(socket_family, socket_type, protocol);
#endif
}

/*------------------------------------------------------------------------------
---------------------------------- connect() -----------------------------------
------------------------------------------------------------------------------*/

/* int __fd, const struct sockaddr * __addr, socklen_t __len
   connect() intercept function */
int connect(CONNECT_SIG)
{
#ifdef DUMMY
  dwr("connect(%d)\n", __fd);
  return realconnect(__fd, __addr, __len);

#else
  /* make sure we don't touch any standard outputs */
  if(__fd == STDIN_FILENO || __fd == STDOUT_FILENO || __fd == STDERR_FILENO)
    return(realconnect(__fd, __addr, __len));
  int sock_type = -1;
  socklen_t sock_type_len = sizeof(sock_type);
  struct sockaddr_in *connaddr;
	connaddr = (struct sockaddr_in *) __addr;

  getsockopt(__fd, SOL_SOCKET, SO_TYPE,
		   (void *) &sock_type, &sock_type_len);

  if(__addr != NULL && (connaddr->sin_family == AF_LOCAL
    || connaddr->sin_family == PF_NETLINK
    || connaddr->sin_family == AF_NETLINK
    || connaddr->sin_family == AF_UNIX)) {
    int err = realconnect(__fd, __addr, __len);
    return err;
  }

  char cmd[BUF_SZ];
  if (realconnect == NULL) {
    dwr("Unresolved symbol: connect()\n");
    return -1;
  }

  /* assemble and route command */
  memset(cmd, '\0', BUF_SZ);
  struct connect_st rpc_st;
  rpc_st.__tid = syscall(SYS_gettid);
  rpc_st.__fd = __fd;
  memcpy(&rpc_st.__addr, __addr, sizeof(struct sockaddr));
  memcpy(&rpc_st.__len, &__len, sizeof(socklen_t));
  cmd[0] = RPC_CONNECT;
  memcpy(&cmd[1], &rpc_st, sizeof(struct connect_st));
  pthread_mutex_lock(&lock);
  write(fdret_sock,cmd, BUF_SZ);

  if(fdret_sock >= 0) {
    int retval;
    char mynewbuf[BUF_SZ];
    memset(&mynewbuf, '\0', sizeof(mynewbuf));
    int n_read = read(fdret_sock, &mynewbuf, sizeof(mynewbuf));
    if(n_read > 0) {
      memcpy(&retval, &mynewbuf[1], sizeof(int));
      pthread_mutex_unlock(&lock);
      return retval;
    }
    else {
      pthread_mutex_unlock(&lock);
      dwr("unable to read connect: return value\n");
    }
  }
  return -1;
#endif
}

/*------------------------------------------------------------------------------
---------------------------------- select() ------------------------------------
------------------------------------------------------------------------------*/

/* int n, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout */
int select(SELECT_SIG)
{
#ifdef DUMMY
  dwr("select(n=%d, <readfds>, <writefds>, <exceptfds>, <timeout>)\n", n);
  return realselect(n, readfds, writefds, exceptfds, timeout);

#else
  return realselect(n, readfds, writefds, exceptfds, timeout);
#endif
}

/*------------------------------------------------------------------------------
----------------------------------- poll() -------------------------------------
------------------------------------------------------------------------------*/

/* struct pollfd *__fds, nfds_t __nfds, int __timeout */
int poll(POLL_SIG)
{
#ifdef DUMMY
  dwr("poll(<ufds>, nfds=%d, timeout=%d)\n", __fds, __timeout);
  return realpoll(__fds, __nfds, __timeout);

#else
  return realpoll(__fds, __nfds, __timeout);
#endif
}

/*------------------------------------------------------------------------------
------------------------------------ bind() ------------------------------------
------------------------------------------------------------------------------*/

/* int sockfd, const struct sockaddr *addr, socklen_t addrlen
   bind() intercept function */
int bind(BIND_SIG)
{
#ifdef DUMMY
    dwr("bind(%d)\n", sockfd);
    return realbind(sockfd, addr, addrlen);

#else
  /* make sure we don't touch any standard outputs */
  if(sockfd == STDIN_FILENO || sockfd == STDOUT_FILENO || sockfd == STDERR_FILENO)
    return(realbind(sockfd, addr, addrlen));

  int sock_type = -1;
  socklen_t sock_type_len = sizeof(sock_type);
  struct sockaddr_in *connaddr;
  connaddr = (struct sockaddr_in *) addr;

  getsockopt(sockfd, SOL_SOCKET, SO_TYPE,
       (void *) &sock_type, &sock_type_len);

  if (addr != NULL && (connaddr->sin_family == AF_LOCAL
    || connaddr->sin_family == PF_NETLINK
    || connaddr->sin_family == AF_NETLINK
    || connaddr->sin_family == AF_UNIX)) {
   return(realbind(sockfd, addr, addrlen));
  }

  char cmd[BUF_SZ];
  if(realbind == NULL) {
    dwr("Unresolved symbol: bind()\n");
    return -1;
  }

  /* Assemble and route command */
  struct bind_st rpc_st;
  rpc_st.sockfd = sockfd;
  rpc_st.__tid = syscall(SYS_gettid);
  memcpy(&rpc_st.addr, addr, sizeof(struct sockaddr));
  memcpy(&rpc_st.addrlen, &addrlen, sizeof(socklen_t));
  cmd[0]=RPC_BIND;
  memcpy(&cmd[1], &rpc_st, sizeof(struct bind_st));
  pthread_mutex_lock(&lock);
  write(fdret_sock, cmd, BUF_SZ);
  pthread_mutex_unlock(&lock);
  errno = ERR_OK;
  return get_retval();
#endif
}


/*------------------------------------------------------------------------------
----------------------------------- accept4() ----------------------------------
------------------------------------------------------------------------------*/


/* int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags */
int accept4(ACCEPT4_SIG)
{
#ifdef DUMMY
  dwr("accept4(%d)\n", sockfd);
  return accept(sockfd, addr, addrlen);

#else
  return accept(sockfd, addr, addrlen);
#endif
}


/*------------------------------------------------------------------------------
----------------------------------- accept() -----------------------------------
------------------------------------------------------------------------------*/

/* int sockfd struct sockaddr *addr, socklen_t *addrlen
   accept() intercept function */
int accept(ACCEPT_SIG)
{
#ifdef DUMMY
    return realaccept(sockfd, addr, addrlen);

#else
  /* make sure we don't touch any standard outputs */
  if(sockfd == STDIN_FILENO || sockfd == STDOUT_FILENO || sockfd == STDERR_FILENO)
    return(realaccept(sockfd, addr, addrlen));

  int sock_type = -1;
  socklen_t sock_type_len = sizeof(sock_type);

  getsockopt(sockfd, SOL_SOCKET, SO_TYPE,
       (void *) &sock_type, &sock_type_len);

  addr->sa_family = AF_INET;
  /* TODO: also get address info */

  char cmd[BUF_SZ];
  if(realaccept == NULL) {
    dwr( "Unresolved symbol: accept()\n");
    return -1;
  }

  char gmybuf[16];
  int new_conn_socket;

  char c[1];
  int n = read(sockfd, c, sizeof(c));
  if(n > 0)
  {
    ssize_t size = sock_fd_read(fdret_sock, gmybuf, sizeof(gmybuf), &new_conn_socket);
    if(size > 0)
    {
      /* Send our local-fd number back to service so it can complete its mapping table */
      memset(cmd, '\0', BUF_SZ);
      cmd[0] = RPC_FD_MAP_COMPLETION;
      memcpy(&cmd[1], &new_conn_socket, sizeof(new_conn_socket));
      pthread_mutex_lock(&lock);
      int n_write = write(fdret_sock, cmd, BUF_SZ);
      if(n_write < 0) {
        dwr("Error sending perceived FD to service. Service might be down.\n");
        errno = ECONNABORTED;
        return -1;
      }
      pthread_mutex_unlock(&lock);
      errno = ERR_OK;
      return new_conn_socket; // OK
    }
    else {
      dwr("Error receiving new FD from service. Service might be down.\n");
      errno = ECONNABORTED;
      return -1;
    }
  }
  dwr("Error reading signal byte from service. Service might be down.\n");
  //errno = EWOULDBLOCK;
  errno = ECONNABORTED;
  return -1;
#endif
}


/*------------------------------------------------------------------------------
------------------------------------- listen()----------------------------------
------------------------------------------------------------------------------*/

/* int sockfd, int backlog
   listen() intercept function */
int listen(LISTEN_SIG)
{
#ifdef DUMMY
    dwr("listen(%d)\n", sockfd);
    return reallisten(sockfd, backlog);

#else

  /* make sure we don't touch any standard outputs */
  if(sockfd == STDIN_FILENO || sockfd == STDOUT_FILENO || sockfd == STDERR_FILENO)
    return(reallisten(sockfd, backlog));

  char cmd[BUF_SZ];
  dwr("listen(%d)\n", sockfd);
  /* Assemble and route command */
  memset(cmd, '\0', BUF_SZ);
  struct listen_st rpc_st;
  rpc_st.sockfd = sockfd;
  rpc_st.backlog = backlog;
  rpc_st.__tid = syscall(SYS_gettid);
  cmd[0] = RPC_LISTEN;
  memcpy(&cmd[1], &rpc_st, sizeof(struct listen_st));
  pthread_mutex_lock(&lock);
  write(fdret_sock,cmd, BUF_SZ);
  pthread_mutex_unlock(&lock);
  errno = ERR_OK;
  return get_retval();
#endif
}
