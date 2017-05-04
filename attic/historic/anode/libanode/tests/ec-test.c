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
#include "../impl/ec.h"
#include "../impl/misc.h"

#define TEST_KEY_LEN 128
#define AnodeEC_key_to_hex(k,b,l) Anode_to_hex((k)->key,(k)->bytes,(b),l)

int main(int argc,char **argv)
{
  struct AnodeECKeyPair pair1;
  struct AnodeECKeyPair pair2;
  struct AnodeECKeyPair pair3;
  unsigned char key[TEST_KEY_LEN];
  char str[16384];

  printf("Creating key pair #1...\n");
  if (!AnodeECKeyPair_generate(&pair1)) {
    printf("Could not create key pair.\n");
    return 1;
  }
  AnodeEC_key_to_hex(&pair1.pub,str,sizeof(str));
  printf("Public:  %s\n",str);
  AnodeEC_key_to_hex(&pair1.priv,str,sizeof(str));
  printf("Private: %s\n\n",str);

  printf("Creating key pair #2...\n");
  if (!AnodeECKeyPair_generate(&pair2)) {
    printf("Could not create key pair.\n");
    return 1;
  }
  AnodeEC_key_to_hex(&pair2.pub,str,sizeof(str));
  printf("Public:  %s\n",str);
  AnodeEC_key_to_hex(&pair2.priv,str,sizeof(str));
  printf("Private: %s\n\n",str);

  printf("Key agreement between public #2 and private #1...\n");
  if (!AnodeECKeyPair_agree(&pair1,&pair2.pub,key,TEST_KEY_LEN)) {
    printf("Agreement failed.\n");
    return 1;
  }
  Anode_to_hex(key,TEST_KEY_LEN,str,sizeof(str));
  printf("Agreed secret: %s\n\n",str);

  printf("Key agreement between public #1 and private #2...\n");
  if (!AnodeECKeyPair_agree(&pair2,&pair1.pub,key,TEST_KEY_LEN)) {
    printf("Agreement failed.\n");
    return 1;
  }
  Anode_to_hex(key,TEST_KEY_LEN,str,sizeof(str));
  printf("Agreed secret: %s\n\n",str);

  printf("Testing key pair init function (init #3 from #2's parts)...\n");
  if (!AnodeECKeyPair_init(&pair3,&(pair2.pub),&(pair2.priv))) {
    printf("Init failed.\n");
    return 1;
  }

  printf("Key agreement between public #1 and private #3...\n");
  if (!AnodeECKeyPair_agree(&pair3,&pair1.pub,key,TEST_KEY_LEN)) {
    printf("Agreement failed.\n");
    return 1;
  }
  Anode_to_hex(key,TEST_KEY_LEN,str,sizeof(str));
  printf("Agreed secret: %s\n\n",str);

  printf("Key agreement between public #1 and private #1...\n");
  if (!AnodeECKeyPair_agree(&pair1,&pair1.pub,key,TEST_KEY_LEN)) {
    printf("Agreement failed.\n");
    return 1;
  }
  Anode_to_hex(key,TEST_KEY_LEN,str,sizeof(str));
  printf("Agreed secret (should not match): %s\n\n",str);

  AnodeECKeyPair_destroy(&pair1);
  AnodeECKeyPair_destroy(&pair2);
  AnodeECKeyPair_destroy(&pair3);

  return 0;
}
