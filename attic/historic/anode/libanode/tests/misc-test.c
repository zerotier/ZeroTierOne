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
#include <time.h>
#include <sys/time.h>
#include "../misc.h"

int main(int argc,char **argv)
{
  const char *base32TestStr = "asdf";
  char *fields[16];
  char buf[1024];
  char buf2[1024];
  char buf3[4096];
  unsigned int i;
  unsigned long tmpl,tmpl2;
  unsigned long long tmp64;

  srand(time(0));

  Anode_base32_5_to_8((const unsigned char *)base32TestStr,buf);
  printf("Base32 from test string: %s\n",buf);
  Anode_base32_8_to_5("MFZWIZQA",(unsigned char *)buf2);
  printf("Test string from Base32 (upper case): %s\n",buf2);
  Anode_base32_8_to_5("mfzwizqa",(unsigned char *)buf2);
  printf("Test string from Base32 (lower case): %s\n",buf2);
  printf("Testing variable length encoding/decoded with pad5 functions...\n");
  for(i=0;i<1024;++i) {
    tmpl = rand() % (sizeof(buf) - 8);
    if (!tmpl)
      tmpl = 1;
    for(tmpl2=0;tmpl2<tmpl;++tmpl2)
      buf[tmpl2] = (buf2[tmpl2] = (char)(rand() >> 3));
    if (!Anode_base32_encode_pad5(buf2,tmpl,buf3,sizeof(buf3))) {
      printf("Failed (encode failed).\n");
      return 1;
    }
    memset(buf2,0,sizeof(buf2));
    if (!Anode_base32_decode_pad5(buf3,buf2,sizeof(buf2))) {
      printf("Failed (decode failed).\n");
      return 1;
    }
    if (memcmp(buf,buf2,tmpl)) {
      printf("Failed (compare failed).\n");
      return 1;
    }
  }

  printf("Anode_htonll(0x0102030405060708) == 0x%.16llx\n",tmp64 = Anode_htonll(0x0102030405060708ULL));
  printf("Anode_ntohll(0x%.16llx) == 0x%.16llx\n",tmp64,Anode_ntohll(tmp64));
  if (Anode_ntohll(tmp64) != 0x0102030405060708ULL) {
    printf("Failed.\n");
    return 1;
  }

  strcpy(buf,"foo bar baz");
  Anode_trim(buf);
  printf("Testing string trim: 'foo bar baz' -> '%s'\n",buf);
  strcpy(buf,"foo bar baz  ");
  Anode_trim(buf);
  printf("Testing string trim: 'foo bar baz  ' -> '%s'\n",buf);
  strcpy(buf,"  foo bar baz");
  Anode_trim(buf);
  printf("Testing string trim: '  foo bar baz' -> '%s'\n",buf);
  strcpy(buf,"  foo bar baz  ");
  Anode_trim(buf);
  printf("Testing string trim: '  foo bar baz  ' -> '%s'\n",buf);
  strcpy(buf,"");
  Anode_trim(buf);
  printf("Testing string trim: '' -> '%s'\n",buf);
  strcpy(buf,"  ");
  Anode_trim(buf);
  printf("Testing string trim: '  ' -> '%s'\n",buf);

  printf("Testing string split.\n");
  strcpy(buf,"66.246.138.121,5323,0");
  i = Anode_split(buf,';',fields,16);
  if (i != 1) {
    printf("Failed.\n");
    return 1;
  } else printf("Fields: %s\n",fields[0]);
  strcpy(buf,"a;b;c");
  i = Anode_split(buf,';',fields,16);
  if (i != 3) {
    printf("Failed.\n");
    return 1;
  } else printf("Fields: %s %s %s\n",fields[0],fields[1],fields[2]);
  strcpy(buf,";;");
  i = Anode_split(buf,';',fields,16);
  if (i != 3) {
    printf("Failed.\n");
    return 1;
  } else printf("Fields: %s %s %s\n",fields[0],fields[1],fields[2]);
  strcpy(buf,"a;b;");
  i = Anode_split(buf,';',fields,16);
  if (i != 3) {
    printf("Failed.\n");
    return 1;
  } else printf("Fields: %s %s %s\n",fields[0],fields[1],fields[2]);
  strcpy(buf,"a;;c");
  i = Anode_split(buf,';',fields,16);
  if (i != 3) {
    printf("Failed.\n");
    return 1;
  } else printf("Fields: %s %s %s\n",fields[0],fields[1],fields[2]);
  strcpy(buf,";;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;");
  i = Anode_split(buf,';',fields,16);
  if (i != 16) {
    printf("Failed.\n");
    return 1;
  }
  strcpy(buf,"");
  i = Anode_split(buf,';',fields,16);
  if (i != 0) {
    printf("Failed.\n");
    return 1;
  }
  printf("Passed.\n");

  return 0;
}
