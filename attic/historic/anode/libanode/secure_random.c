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
#include "impl/aes.h"
#include "impl/misc.h"
#include "anode.h"

#ifdef WINDOWS
#include <windows.h>
#include <wincrypt.h>
#endif

struct AnodeSecureRandomImpl
{
  AnodeAesExpandedKey key;
  unsigned char state[ANODE_AES_BLOCK_SIZE];
  unsigned char block[ANODE_AES_BLOCK_SIZE];
  unsigned int ptr;
};

AnodeSecureRandom *AnodeSecureRandom_new()
{
  unsigned char keybuf[ANODE_AES_KEY_SIZE + ANODE_AES_BLOCK_SIZE + ANODE_AES_BLOCK_SIZE];
  unsigned int i;
  struct AnodeSecureRandomImpl *srng;

#ifdef WINDOWS
  HCRYPTPROV hProv;
  if (CryptAcquireContext(&hProv,NULL,NULL,PROV_RSA_FULL,CRYPT_VERIFYCONTEXT|CRYPT_SILENT)) {
    CryptGenRandom(hProv,sizeof(keybuf),keybuf);
    CryptReleaseContext(hProv,0);
  }
#else
  FILE *urandf = fopen("/dev/urandom","rb");
  if (urandf) {
    fread((void *)keybuf,sizeof(keybuf),1,urandf);
    fclose(urandf);
  }
#endif

  for(i=0;i<sizeof(keybuf);++i)
    keybuf[i] ^= (unsigned char)(Anode_rand() >> 5);

  srng = malloc(sizeof(struct AnodeSecureRandomImpl));
  Anode_aes256_expand_key(keybuf,&srng->key);
  for(i=0;i<ANODE_AES_BLOCK_SIZE;++i)
    srng->state[i] = keybuf[ANODE_AES_KEY_SIZE + i];
  for(i=0;i<ANODE_AES_BLOCK_SIZE;++i)
    srng->block[i] = keybuf[ANODE_AES_KEY_SIZE + ANODE_AES_KEY_SIZE + i];
  srng->ptr = ANODE_AES_BLOCK_SIZE;

  return (AnodeSecureRandom *)srng;
}

void AnodeSecureRandom_gen_bytes(AnodeSecureRandom *srng,void *buf,long count)
{
  long i,j;

  for(i=0;i<count;++i) {
    if (((struct AnodeSecureRandomImpl *)srng)->ptr == ANODE_AES_BLOCK_SIZE) {
      Anode_aes256_encrypt(&((struct AnodeSecureRandomImpl *)srng)->key,((struct AnodeSecureRandomImpl *)srng)->state,((struct AnodeSecureRandomImpl *)srng)->state);
      for(j=0;j<ANODE_AES_KEY_SIZE;++j)
        ((struct AnodeSecureRandomImpl *)srng)->block[j] ^= ((struct AnodeSecureRandomImpl *)srng)->state[j];
      ((struct AnodeSecureRandomImpl *)srng)->ptr = 0;
    }
    ((unsigned char *)buf)[i] = ((struct AnodeSecureRandomImpl *)srng)->block[((struct AnodeSecureRandomImpl *)srng)->ptr++];
  }
}

void AnodeSecureRandom_delete(AnodeSecureRandom *srng)
{
  free(srng);
}
