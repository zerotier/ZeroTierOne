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

#ifndef _SPARK_WRAPPER_H
#define _SPARK_WRAPPER_H

#include <openssl/sha.h>
#include "../libanode/aes128.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Spark uses SHA-256 with hash length 32 */
#define SPARK_HASH_LENGTH 32

// Wrap a segment for forward propagation
static inline void Spark_wrap(void *data,unsigned long len,void *plaintext_hash_buf,void *global_hash_buf)
{
  unsigned char expkey[ANODE_AES128_EXP_KEY_SIZE];

  SHA256((const unsigned char *)data,len,(unsigned char *)plaintext_hash_buf);

  Anode_aes128_expand_key(expkey,(const unsigned char *)plaintext_hash_buf);
  Anode_aes128_cfb_encrypt(expkey,((const unsigned char *)plaintext_hash_buf) + 16,(unsigned char *)data,len);

  SHA256((const unsigned char *)data,len,(unsigned char *)global_hash_buf);
}

// Unwrap a segment and check its integrity
static inline int Spark_unwrap(void *data,unsigned long len,const void *plaintext_hash)
{
  unsigned char expkey[ANODE_AES128_EXP_KEY_SIZE];
  unsigned char check_hash[32];
  unsigned long i;

  Anode_aes128_expand_key(expkey,(const unsigned char *)plaintext_hash);
  Anode_aes128_cfb_decrypt(expkey,((const unsigned char *)plaintext_hash) + 16,(unsigned char *)data,len);

  SHA256((const unsigned char *)data,len,check_hash);

  for(i=0;i<32;++i) {
    if (check_hash[i] != ((const unsigned char *)plaintext_hash)[i])
      return 0;
  }
  return 1;
}

#ifdef __cplusplus
}
#endif

#endif
