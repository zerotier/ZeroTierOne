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

#ifndef _ANODE_AES_H
#define _ANODE_AES_H

#include <openssl/aes.h>
#include "types.h"

/* This just glues us to OpenSSL's built-in AES-256 implementation */

#define ANODE_AES_BLOCK_SIZE 16
#define ANODE_AES_KEY_SIZE 32

typedef AES_KEY AnodeAesExpandedKey;

#define Anode_aes256_expand_key(k,ek) AES_set_encrypt_key((const unsigned char *)(k),256,(AES_KEY *)(ek))

/* Note: in and out can be the same thing */
#define Anode_aes256_encrypt(ek,in,out) AES_encrypt((const unsigned char *)(in),(unsigned char *)(out),(const AES_KEY *)(ek))

/* Note: iv is modified */
static inline void Anode_aes256_cfb_encrypt(
  const AnodeAesExpandedKey *expkey,
  const unsigned char *in,
  unsigned char *out,
  unsigned char *iv,
  unsigned long len)
{
  int tmp = 0;
  AES_cfb128_encrypt(in,out,len,(const AES_KEY *)expkey,iv,&tmp,AES_ENCRYPT);
}
static inline void Anode_aes256_cfb_decrypt(
  const AnodeAesExpandedKey *expkey,
  const unsigned char *in,
  unsigned char *out,
  unsigned char *iv,
  unsigned long len)
{
  int tmp = 0;
  AES_cfb128_encrypt(in,out,len,(const AES_KEY *)expkey,iv,&tmp,AES_DECRYPT);
}

/* CMAC message authentication code */
void Anode_cmac_aes256(
  const AnodeAesExpandedKey *expkey,
  const unsigned char *restrict data,
  unsigned long data_len,
  unsigned char *restrict mac);

#endif
