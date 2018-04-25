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
#include "../dictionary.h"

static const char *HASH_TESTS[16] = {
  "test",
  "testt",
  "",
  "foo",
  "fooo",
  "1",
  "2",
  "3",
  "4",
  "11",
  "22",
  "33",
  "44",
  "adklfjklejrer",
  "erngnetbekjrq",
  "erklerqqqqre"
};

int diterate(void *arg,const char *key,const char *value)
{
  printf("  %s: %s\n",key ? key : "(null)",value ? value : "(null)");
  return 1;
}

int main(int argc,char **argv)
{
  char tmp[1024];
  char fuzzparam1[16],fuzzparam2[16],fuzzparam3[16];
  struct AnodeDictionary d;
  unsigned int i,j,k,cs;

  srandom(time(0));

  printf("Trying out hash function a little...\n");
  for(i=0;i<16;++i)
    printf("  %s: %u\n",HASH_TESTS[i],(unsigned int)AnodeDictionary__get_bucket(HASH_TESTS[i]));

  for(cs=0;cs<2;++cs) {
    printf("\nTesting with case sensitivity = %d\n",cs);
    AnodeDictionary_init(&d,cs);

    printf("\nTesting dictionary by adding and retrieving some keys...\n");
    AnodeDictionary_put(&d,"test1","This is the first test");
    AnodeDictionary_put(&d,"test2","This is the second test");
    AnodeDictionary_put(&d,"test3","This is the third test (lower case)");
    AnodeDictionary_put(&d,"TEST3","This is the third test (UPPER CASE)");
    AnodeDictionary_iterate(&d,(void *)0,&diterate);
    if (d.size != (cs ? 4 : 3)) {
      printf("Failed (size).\n");
      return 1;
    }

    AnodeDictionary_clear(&d);
    if (d.size||(AnodeDictionary_get(&d,"test1"))) {
      printf("Failed (clear).\n");
      return 1;
    }

    printf("\nTesting read, trial 1: simple key=value with unterminated line\n");
    strcpy(tmp,"foo=bar\nbar=baz\ntest1=Happy happy joyjoy!\ntest2=foobarbaz\nlinewithnocr=thisworked");  
    AnodeDictionary_read(&d,tmp,"\r\n","=","",'\\',0,0);
    printf("Results:\n");
    AnodeDictionary_iterate(&d,(void *)0,&diterate);
    AnodeDictionary_clear(&d);

    printf("\nTesting read, trial 2: key=value with escape chars, escaped CRs\n");
    strcpy(tmp,"foo=bar\r\nbar==baz\nte\\=st1=\\=Happy happy joyjoy!\ntest2=foobarbaz\\\nfoobarbaz on next line\r\n");  
    AnodeDictionary_read(&d,tmp,"\r\n","=","",'\\',0,0);
    printf("Results:\n");
    AnodeDictionary_iterate(&d,(void *)0,&diterate);
    AnodeDictionary_clear(&d);

    printf("\nTesting read, trial 3: HTTP header-like dictionary\n");
    strcpy(tmp,"Host: some.host.net\r\nX-Some-Header: foo bar\r\nX-Some-Other-Header: y0y0y0y0y0\r\n");  
    AnodeDictionary_read(&d,tmp,"\r\n",": ","",0,0,0);
    printf("Results:\n");
    AnodeDictionary_iterate(&d,(void *)0,&diterate);
    AnodeDictionary_clear(&d);

    printf("\nTesting read, trial 4: single line key/value\n");
    strcpy(tmp,"Header: one line only");
    AnodeDictionary_read(&d,tmp,"\r\n",": ","",0,0,0);
    printf("Results:\n");
    AnodeDictionary_iterate(&d,(void *)0,&diterate);
    AnodeDictionary_clear(&d);

    printf("\nFuzzing dictionary reader...\n"); fflush(stdout);
    for(i=0;i<200000;++i) {
      j = random() % (sizeof(tmp) - 1);
      for(k=0;k<j;++k) {
        tmp[k] = (char)((unsigned int)random() >> 3);
        if (!tmp[k]) tmp[k] = 1;
      }
      tmp[j] = (char)0;

      j = random() % (sizeof(fuzzparam1) - 1);
      for(k=0;k<j;++k) {
        fuzzparam1[k] = (char)((unsigned int)random() >> 3);
        if (!fuzzparam1[k]) fuzzparam1[k] = 1;
      }
      fuzzparam1[j] = (char)0;

      j = random() % (sizeof(fuzzparam2) - 1);
      for(k=0;k<j;++k) {
        fuzzparam1[k] = (char)((unsigned int)random() >> 3);
        if (!fuzzparam2[k]) fuzzparam2[k] = 1;
      }
      fuzzparam2[j] = (char)0;

      j = random() % (sizeof(fuzzparam3) - 1);
      for(k=0;k<j;++k) {
        fuzzparam3[k] = (char)((unsigned int)random() >> 3);
        if (!fuzzparam3[k]) fuzzparam3[k] = 1;
      }
      fuzzparam3[j] = (char)0;

      AnodeDictionary_read(&d,tmp,fuzzparam1,fuzzparam2,fuzzparam3,random() & 3,random() & 1,random() & 1);
      AnodeDictionary_clear(&d);
    }

    AnodeDictionary_destroy(&d);
  }

  return 0;
}
