
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


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/socket.h>

#include "lwip/ip.h"
#include "lwip/ip_addr.h"
#include "lwip/ip_frag.h"

#ifndef _NETCON_UTILITIES_CPP
#define _NETCON_UTILITIES_CPP

namespace ZeroTier
{
	void clearscreen(){
		fprintf(stderr, "\033[2J");
	}
	void gotoxy(int x,int y) {
	    fprintf(stderr, "%c[%d;%df",0x1B,y,x);
	}

	// Gets the process/path name associated with a pid
	void get_path_from_pid(char* dest, int pid)
	{
	  char ppath[80];
	  sprintf(ppath, "/proc/%d/exe", pid);
		if (readlink (ppath, dest, 80) != -1){
	  }
	}

	// Gets the process/path name associated with a fd
	void get_path_from_fd(char* dest, int pid, int fd)
	{
		char ppfd[80];
		sprintf(ppfd, "/proc/%d/fd/%d", pid, fd);
		if (readlink (ppfd, dest, 80) != -1){
		}
	}
}

#endif
