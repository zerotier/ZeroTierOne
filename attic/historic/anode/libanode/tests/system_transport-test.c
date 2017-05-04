/* libanode: the Anode C reference implementation
 * Copyright (C) 2009-2010 Adam Ierymenko <adam.ierymenko@gmail.com>
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include "../anode.h"
#include "../impl/thread.h"

static int do_client()
{
  AnodeTransport *st;
  AnodeSocket *udp_sock;
  int run = 1;

  st = AnodeSystemTransport_new(NULL);
  if (!st) {
    printf("FAILED: unable to construct AnodeSystemTransport.\n");
    return -1;
  }
  printf("Created AnodeSystemTransport.\n");

  while (run)
    st->poll(st);
}

static int do_server()
{
  AnodeTransport *st;
  AnodeSocket *udp_sock;
  AnodeSocket *tcp_sock;
  int run = 1;

  st = AnodeSystemTransport_new(NULL);
  if (!st) {
    printf("FAILED: unable to construct AnodeSystemTransport.\n");
    return -1;
  }
  printf("Created AnodeSystemTransport.\n");

  while (run)
    st->poll(st);
}

int main(int argc,char **argv)
{
  if (argc == 2) {
    if (!strcmp(argv[1],"client"))
      return do_client();
    else if (!strcmp(argv[1],"server"))
      return do_server();
  }

  printf("Usage: system_transport-test <client / server>\n");
  return -1;
}
