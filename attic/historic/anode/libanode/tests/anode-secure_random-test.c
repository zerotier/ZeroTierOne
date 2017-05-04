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

#include <stdlib.h>
#include <stdio.h>
#include "../anode.h"
#include "../misc.h"

int main(int argc,char **argv)
{
  unsigned char test[10005];
  unsigned int i;
  AnodeSecureRandom srng;

  AnodeSecureRandom_init(&srng);

  AnodeSecureRandom_gen_bytes(&srng,test,sizeof(test));

  for(i=0;i<sizeof(test);++i) {
    printf("%.2x",(unsigned int)test[i]);
    if ((i % 20) == 19)
      printf("\n");
  }
  printf("\n");
}
