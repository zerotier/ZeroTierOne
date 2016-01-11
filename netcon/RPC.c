#include <stdio.h>
#include <unistd.h>
#include <sys/un.h>
#include <pthread.h>
#include <errno.h>
#include <sys/syscall.h>

#include <fcntl.h>
#include <stdint.h>

#include <sys/socket.h>
#include <strings.h>
#include "RPC.h"

#define RPC_FD 1023
#define SERVICE_CONNECT_ATTEMPTS 30

static int instance_count;
static int rpc_count;

static pthread_mutex_t lock;
void rpc_mutex_init() {
  if(pthread_mutex_init(&lock, NULL) != 0) {
    fprintf(stderr, "error while initializing service call mutex\n");
  }
}
void rpc_mutex_destroy() {
  pthread_mutex_destroy(&lock);
}

/* 
 * Reads a new file descriptor from the service 
 */
int get_new_fd(int sock)
{
  char buf[BUF_SZ];
  int newfd;
  ssize_t size = sock_fd_read(sock, buf, sizeof(buf), &newfd);
  if(size > 0){
    return newfd;
  }
  fprintf(stderr, "get_new_fd(): Error, unable to read fd over (%d)\n", sock);
  return -1;
}

/*
 * Reads a return value from the service and sets errno (if applicable)
 */
int get_retval(int rpc_sock)
{
  if(rpc_sock >= 0) {
    int retval;
    int sz = sizeof(char) + sizeof(retval) + sizeof(errno);
    char retbuf[BUF_SZ];
    memset(&retbuf, 0, sz);
    int n_read = read(rpc_sock, &retbuf, sz);
    if(n_read > 0) {
      memcpy(&retval, &retbuf[1], sizeof(retval));
      memcpy(&errno, &retbuf[1+sizeof(retval)], sizeof(errno));
      return retval;
    }
  }
  return -1;
}

int rpc_join(const char * sockname)
{
	struct sockaddr_un addr;
	int conn_err = -1, attempts = 0;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, sockname, sizeof(addr.sun_path)-1);

	int sock;
	if((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0){
		fprintf(stderr, "Error while creating RPC socket\n");
		return -1;
	}
	while((conn_err != 0) && (attempts < SERVICE_CONNECT_ATTEMPTS)){
		if((conn_err = connect(sock, (struct sockaddr*)&addr, sizeof(addr))) != 0) {
			fprintf(stderr, "Error while connecting to RPC socket. Re-attempting...\n");
			sleep(1);
		}
		else {
			//int newfd = dup2(sock, RPC_FD-instance_count);
			//close(sock);
			return sock;
		}
		attempts++;
	}
	return -1;
}

/*
 * Send a command to the service 
 */
int rpc_send_command(int cmd, int forfd, void *data, int len)
{
  pthread_mutex_lock(&lock);
  char c, padding[] = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89};
  char cmdbuf[BUF_SZ], magic[TOKEN_SIZE], metabuf[BUF_SZ];
  memcpy(magic+MAGIC_SIZE, padding, TOKEN_SIZE);
  uint64_t magic_num;

  // ephemeral RPC socket used only for this command
  int rpc_sock = rpc_join("/root/dev/ztest/nc_e5cd7a9e1c3511dd");
  // Generate token
  int fdrand = open("/dev/urandom", O_RDONLY);
  read(fdrand, &magic, MAGIC_SIZE);
  memcpy(&magic_num, magic, MAGIC_SIZE);  
  cmdbuf[CMD_ID_IDX] = cmd;
  memcpy(&cmdbuf[MAGIC_IDX], &magic_num, MAGIC_SIZE);
  memcpy(&cmdbuf[STRUCT_IDX], data, len);

  // Format: [sig_byte] + [cmd_id] + [magic] + [meta] + [payload]

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
  memset(metabuf, 0, BUF_SZ);
  pid_t pid = syscall(SYS_getpid);
  pid_t tid = syscall(SYS_gettid);
  rpc_count++;
  char timestring[20];
  time_t timestamp;
  timestamp = time(NULL);
  strftime(timestring, sizeof(timestring), "%H:%M:%S", localtime(&timestamp));

  metabuf[IDX_SIGNAL_BYTE] = 'R';
  
  memcpy(&metabuf[IDX_PID],     &pid,         sizeof(pid_t)      ); /* pid       */
  memcpy(&metabuf[IDX_TID],     &tid,         sizeof(pid_t)      ); /* tid       */
  memcpy(&metabuf[IDX_COUNT],   &rpc_count,   sizeof(rpc_count)  ); /* rpc_count */
  memcpy(&metabuf[IDX_TIME],    &timestring,   20                ); /* timestamp */
#endif
  /* Combine command flag+payload with RPC metadata */
  memcpy(&metabuf[IDX_PAYLOAD], cmdbuf, len + 1 + MAGIC_SIZE);
  
  // Write RPC
  int n_write = write(rpc_sock, &metabuf, BUF_SZ);
  if(n_write < 0) {
    fprintf(stderr, "Error writing command to service (CMD = %d)\n", cmdbuf[CMD_ID_IDX]);
    errno = 0;
  }
  // Write token to corresponding data stream
  read(rpc_sock, &c, 1);
  if(c == 'z' && n_write > 0 && forfd > -1){
    int w = send(forfd, &magic, TOKEN_SIZE, 0);
  }
  // Process response from service
  int ret = ERR_OK;
  if(n_write > 0) {
    if(cmdbuf[CMD_ID_IDX]==RPC_SOCKET) {
      pthread_mutex_unlock(&lock);
      return rpc_sock; // Used as new socket
    }
    if(cmdbuf[CMD_ID_IDX]==RPC_CONNECT
      || cmdbuf[CMD_ID_IDX]==RPC_BIND
      || cmdbuf[CMD_ID_IDX]==RPC_LISTEN) {
    	ret = get_retval(rpc_sock);
    }
    if(cmdbuf[CMD_ID_IDX]==RPC_GETSOCKNAME) {
      pthread_mutex_unlock(&lock);
      return rpc_sock; // Don't close rpc here, we'll use it to read getsockopt_st
    }
  }
  else
    ret = -1;
  close(rpc_sock); // We're done with this RPC socket, close it (if type-R)
  pthread_mutex_unlock(&lock);
  return ret;
}

/* 
 * Send file descriptor 
 */
ssize_t sock_fd_write(int sock, int fd)
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
/* 
 * Read a file descriptor 
 */
ssize_t sock_fd_read(int sock, void *buf, ssize_t bufsize, int *fd)
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
      fprintf(stderr, "sock_fd_read(): recvmsg: Error\n");
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
      fprintf(stderr, "sock_fd_read(): read: Error\n");
      return -1;
    }
  }
  return size;
}
