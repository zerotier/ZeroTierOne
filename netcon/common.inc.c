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

#include <stdio.h>
#include <netdb.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/syscall.h>

#ifndef _COMMON_H
#define _COMMON_H  1

#define DEBUG_LEVEL     4

#define MSG_TRANSFER    1 // RX/TX specific statements
#define MSG_ERROR       2 // Errors
#define MSG_INFO        3 // Information which is generally useful to any user
#define MSG_DEBUG       4 // Information which is only useful to someone debugging
#define MSG_DEBUG_EXTRA 5 // If nothing in your world makes sense

#ifdef NETCON_INTERCEPT

void print_addr(struct sockaddr *addr)
{
  char *s = NULL;
  switch(addr->sa_family) {
      case AF_INET: {
          struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
          s = malloc(INET_ADDRSTRLEN);
          inet_ntop(AF_INET, &(addr_in->sin_addr), s, INET_ADDRSTRLEN);
          break;
      }
      case AF_INET6: {
          struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)addr;
          s = malloc(INET6_ADDRSTRLEN);
          inet_ntop(AF_INET6, &(addr_in6->sin6_addr), s, INET6_ADDRSTRLEN);
          break;
      }
      default:
          break;
  }
  fprintf(stderr, "IP address: %s\n", s);
  free(s);
}
#endif

#ifdef NETCON_SERVICE
  namespace ZeroTier {
#endif
  void dwr(int level, const char *fmt, ... )
  {
    if(level > DEBUG_LEVEL)
        return;
    int saveerr;
    saveerr = errno;
    va_list ap;
    va_start(ap, fmt);
  #ifdef VERBOSE // So we can cut out some clutter in the strace output while debugging
    char timestring[20];
    time_t timestamp;
    timestamp = time(NULL);
    strftime(timestring, sizeof(timestring), "%H:%M:%S", localtime(&timestamp));
    pid_t tid = syscall(SYS_gettid);
    fprintf(stderr, "%s [tid=%7d] ", timestring, tid);
  #endif
    vfprintf(stderr, fmt, ap);
    fflush(stderr);

    errno = saveerr;
    va_end(ap);
  }
#ifdef NETCON_SERVICE
}
#endif

#endif
