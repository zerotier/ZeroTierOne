
#ifndef _COMMON_H_
#define _COMMON_H_

//#include <unistd.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <assert.h>
#include <signal.h>
#include <sys/socket.h>


void set_log_options(int, char *, int);
void show_msg(int level, char *, ...);
unsigned int resolve_ip(char *, int, int);


#define MSGERR    0
#define MSGDEBUG  1


#endif
