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

#include "Common.h"

void dwr(const char *fmt, ...);

/* defined in intercept and service */
//extern FILE* logfile;
//extern char* logfilename;
//extern flog = -1;

extern pthread_mutex_t loglock;

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


#ifdef NETCON_SERVICE
   void dwr(int, const char *fmt, ...);

   void dwr(const char *fmt, ...)
   {
      int saveerr;
      va_list ap;
      va_start(ap, fmt);
      saveerr = errno;
      dwr(-1, fmt, ap);
      errno = saveerr;
      va_end(ap);
   }

   void dwr(int pid, const char *fmt, ...)
#endif
#ifdef NETCON_INTERCEPT
   void dwr(const char *fmt, ...)
#endif
{
	va_list ap;
	int saveerr;
	char timestring[20];
	time_t timestamp;

	timestamp = time(NULL);
	strftime(timestring, sizeof(timestring), "%H:%M:%S", localtime(&timestamp));

  //if(logfile)
  //  fprintf(logfile, "%s ", timestring);
  fprintf(stderr, "%s ", timestring);

#ifdef NETCON_SERVICE
	if(ns != NULL)
	{
    size_t num_intercepts = ns->intercepts.size();
    size_t num_connections = ns->connections.size();
    //if(logfile)
    //  fprintf(logfile, "[i/c/tid=%3lu|%3lu|%7d]", num_intercepts, num_connections, pid);
    fprintf(stderr, "[i/c/tid=%3lu|%3lu|%7d]", num_intercepts, num_connections, pid);
	}
	else {
    //if(logfile)
    //  fprintf(logfile, "[i/c/tid=%3d|%3d|%7d]", 0, 0, -1);
    fprintf(stderr, "[i/c/tid=%3d|%3d|%7d]", 0, 0, -1);
  }

#endif

#ifdef NETCON_INTERCEPT
  //pthread_mutex_lock(&loglock);
  int pid = getpid();
  //if(logfile)
	//  fprintf(logfile, "[tid=%7d]", pid);
  fprintf(stderr, "[tid=%7d]", pid);
  //pthread_mutex_unlock(&loglock);
#endif

	//if(logfile)
  //  fputs(" ", logfile);
  fputs(" ", stderr);

  /* logfile */
	va_start(ap, fmt);
	saveerr = errno;
	//if(logfile){
  //  vfprintf(logfile, fmt, ap);
  //  fflush(logfile);
  //}
  va_end(ap);

  /* console */
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
	fflush(stderr);
  errno = saveerr;
	va_end(ap);
}
