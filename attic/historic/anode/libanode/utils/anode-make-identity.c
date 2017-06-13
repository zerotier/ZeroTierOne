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
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "../anode.h"
#include "../impl/misc.h"
#include "../impl/types.h"

int main(int argc,char **argv)
{
  char str[1024];
  AnodeZone zone;
  AnodeIdentity identity;

  if (argc < 2) {
    printf("Usage: anode-make-identity <32-bit zone ID hex>\n");
    return 0;
  }

  *((uint32_t *)zone.bits) = htonl((uint32_t)strtoul(argv[1],(char **)0,16));

  if (AnodeIdentity_generate(&identity,&zone,ANODE_ADDRESS_ANODE_256_40)) {
    fprintf(stderr,"Error: identity key pair generation failed (check build settings).\n");
    return 1;
  }
  if (AnodeIdentity_to_string(&identity,str,sizeof(str)) <= 0) {
    fprintf(stderr,"Error: internal error converting identity to string.\n");
    return -1;
  }

  printf("%s\n",str);

  return 0;
}
