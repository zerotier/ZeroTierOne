#ifndef __RPCLIB_H_
#define __RPCLIB_H_

#define IDX_PID			0
#define IDX_TID			sizeof(pid_t)
#define IDX_COUNT		IDX_TID + sizeof(pid_t)
#define IDX_TIME		IDX_COUNT + sizeof(int)
#define IDX_PAYLOAD		IDX_TIME + 20 /* 20 being the length of the timestamp string */

#define BUF_SZ          256
#define PAYLOAD_SZ		223 /* BUF_SZ-IDX_PAYLOAD */

#define ERR_OK          0

/* RPC codes */
#define RPC_UNDEFINED			 	0
#define RPC_CONNECT				 	1
#define RPC_CONNECT_SOCKARG		 	2
#define RPC_CLOSE				 	3
#define RPC_READ				 	4
#define RPC_WRITE				 	5
#define RPC_BIND				 	6
#define RPC_ACCEPT			 		7
#define RPC_LISTEN			 		8
#define RPC_SOCKET			 		9
#define RPC_SHUTDOWN		 		10
#define RPC_GETSOCKNAME				11

/* Administration RPC codes */
#define RPC_MAP						20	/* Give the service the value we "see" for the new buffer fd */
#define RPC_MAP_REQ					21  /* A call to determine whether an fd is mapped to the service */
#define RPC_RETVAL					22	/* not RPC per se, but something we should codify */
#define RPC_KILL_INTERCEPT			23  /* Tells the service we need to shut down all connections */


#ifdef __cplusplus
extern "C" {
#endif

void rpc_mutex_destroy();
void rpc_mutex_init();

int get_retval(int);
int get_new_fd(int);

int rpc_join(const char * sockname);
int rpc_send_command(int cmd, int rpc_sock, void *data, int len);

ssize_t sock_fd_write(int sock, int fd);
ssize_t sock_fd_read(int sock, void *buf, ssize_t bufsize, int *fd);

/* Structures used for sending commands via RPC mechanism */

struct bind_st {
	int sockfd;
	struct sockaddr_storage addr;
	socklen_t addrlen;
	int __tid;
};

struct connect_st {
	int __fd;
	struct sockaddr_storage __addr;
	socklen_t __len;
	int __tid;
};

struct close_st {
	int fd;
};

struct listen_st {
	int sockfd;
	int backlog;
	int __tid;
};

struct socket_st {
  int socket_family;
  int socket_type;
  int protocol;
	int __tid;
};

struct accept_st {
	int sockfd;
	struct sockaddr_storage addr;
	socklen_t addrlen;
	int __tid;
};

struct shutdown_st {
	int socket;
	int how;
};

struct getsockname_st {
	int sockfd;
	struct sockaddr_storage addr;
	socklen_t addrlen;
};

#ifdef __cplusplus
}
#endif

#endif