/* libanode: the Anode C reference implementation
 * Copyright (C) 2009 Adam Ierymenko <adam.ierymenko@gmail.com>
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
#include "../anode.h"
#include "../dictionary.h"

static int got_it = 0;

static void zone_lookup_handler(void *ptr,long zone_id,AnodeZone *zone)
{
  if (zone)
    printf("got %.8lx: %d entries\n",(unsigned long)zone_id & 0xffffffff,((struct AnodeDictionary *)zone)->size);
  else printf("failed.\n");
  got_it = 1;
}

int main(int argc,char **argv)
{
  AnodeTransportEngine transport;

  Anode_init_ip_transport_engine(&transport);

  AnodeZone_lookup(&transport,0,0,&zone_lookup_handler);

  while (!got_it)
    transport.poll(&transport);

  transport.destroy(&transport);

  return 0;
}
