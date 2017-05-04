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

#include "anode.h"
#include "impl/aes.h"
#include "impl/misc.h"
#include "impl/types.h"

void Anode_aes_digest(const void *const message,unsigned long message_len,void *const hash)
{
  unsigned char previous_digest[16];
  unsigned char digest[16];
  unsigned char block[32];
  const unsigned char *in = (const unsigned char *)message;
  const unsigned char *end = in + message_len;
  unsigned long block_counter;
  AnodeAesExpandedKey expkey;

  ((uint64_t *)digest)[0] = 0ULL;
  ((uint64_t *)digest)[1] = 0ULL;
  ((uint64_t *)block)[0] = 0ULL;
  ((uint64_t *)block)[1] = 0ULL;
  ((uint64_t *)block)[2] = 0ULL;
  ((uint64_t *)block)[3] = 0ULL;

  /* Davis-Meyer hash function built from block cipher */
  block_counter = 0;
  while (in != end) {
    block[block_counter++] = *(in++);
    if (block_counter == 32) {
      block_counter = 0;
      ((uint64_t *)previous_digest)[0] = ((uint64_t *)digest)[0];
      ((uint64_t *)previous_digest)[1] = ((uint64_t *)digest)[1];
      Anode_aes256_expand_key(block,&expkey);
      Anode_aes256_encrypt(&expkey,digest,digest);
      ((uint64_t *)digest)[0] ^= ((uint64_t *)previous_digest)[0];
      ((uint64_t *)digest)[1] ^= ((uint64_t *)previous_digest)[1];
    }
  }

  /* Davis-Meyer end marker */
  block[block_counter++] = 0x80;
  while (block_counter != 32) block[block_counter++] = 0;
  ((uint64_t *)previous_digest)[0] = ((uint64_t *)digest)[0];
  ((uint64_t *)previous_digest)[1] = ((uint64_t *)digest)[1];
  Anode_aes256_expand_key(block,&expkey);
  Anode_aes256_encrypt(&expkey,digest,digest);
  ((uint64_t *)digest)[0] ^= ((uint64_t *)previous_digest)[0];
  ((uint64_t *)digest)[1] ^= ((uint64_t *)previous_digest)[1];

  /* Merkle-Damgård length padding */
  ((uint64_t *)block)[0] = 0ULL;
  if (sizeof(message_len) >= 8) { /* 32/64 bit? this will get optimized out */
    block[8] = (uint8_t)((uint64_t)message_len >> 56);
    block[9] = (uint8_t)((uint64_t)message_len >> 48);
    block[10] = (uint8_t)((uint64_t)message_len >> 40);
    block[11] = (uint8_t)((uint64_t)message_len >> 32);
  } else ((uint32_t *)block)[2] = 0;
  block[12] = (uint8_t)(message_len >> 24);
  block[13] = (uint8_t)(message_len >> 16);
  block[14] = (uint8_t)(message_len >> 8);
  block[15] = (uint8_t)message_len;
  ((uint64_t *)previous_digest)[0] = ((uint64_t *)digest)[0];
  ((uint64_t *)previous_digest)[1] = ((uint64_t *)digest)[1];
  Anode_aes256_expand_key(block,&expkey);
  Anode_aes256_encrypt(&expkey,digest,digest);
  ((uint64_t *)digest)[0] ^= ((uint64_t *)previous_digest)[0];
  ((uint64_t *)digest)[1] ^= ((uint64_t *)previous_digest)[1];

  ((uint64_t *)hash)[0] = ((uint64_t *)digest)[0];
  ((uint64_t *)hash)[1] = ((uint64_t *)digest)[1];
}
