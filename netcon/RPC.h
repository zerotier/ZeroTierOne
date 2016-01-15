#ifndef __RPCLIB_H_
#define __RPCLIB_H_

#include <stdint.h>

#define CANARY_SZ               sizeof(uint64_t)
#define PADDING_SZ              12
#define PADDING                 0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89

#define RPC_PHRASE              "zerotier\0"
#define RPC_PHRASE_SZ           9
#define RPC_TIMESTAMP_SZ        20
// 1st RPC section (metdata)
#define IDX_SIGNAL_PHRASE       0
#define IDX_PID                 IDX_SIGNAL_PHRASE + RPC_PHRASE_SZ
#define IDX_TID                 sizeof(pid_t) + IDX_PID
#define IDX_COUNT               IDX_TID + sizeof(pid_t)
#define IDX_TIME                IDX_COUNT + sizeof(int)
#define IDX_PAYLOAD             IDX_TIME + RPC_TIMESTAMP_SZ
// 2nd RPC section (payload and canary)
#define CMD_ID_IDX              0
#define CANARY_IDX              1
#define STRUCT_IDX              CANARY_IDX+CANARY_SZ

#define BUF_SZ                  512

#define ERR_OK                  0

/* RPC codes */
#define RPC_UNDEFINED           0
#define RPC_CONNECT             1
#define RPC_CONNECT_SOCKARG     2
#define RPC_CLOSE               3
#define RPC_READ                4
#define RPC_WRITE               5
#define RPC_BIND                6
#define RPC_ACCEPT              7
#define RPC_LISTEN              8
#define RPC_SOCKET              9
#define RPC_SHUTDOWN            10
#define RPC_GETSOCKNAME         11
#define RPC_RETVAL              12

#ifdef __cplusplus
extern "C" {
#endif

int get_retval(int);
int rpc_join(const char * sockname);
int rpc_send_command(char *path, int cmd, int forfd, void *data, int len);

int get_new_fd(int sock);
ssize_t sock_fd_write(int sock, int fd);
ssize_t sock_fd_read(int sock, void *buf, ssize_t bufsize, int *fd);

void rpc_mutex_destroy();
void rpc_mutex_init();


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