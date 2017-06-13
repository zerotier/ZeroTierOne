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

#include "aes.h"

void Anode_cmac_aes256(
  const AnodeAesExpandedKey *expkey,
  const unsigned char *restrict data,
  unsigned long data_len,
  unsigned char *restrict mac)
{
  unsigned char cbc[16];
  unsigned char pad[16];
  const unsigned char *restrict pos = data;
  unsigned long i;
  unsigned long remaining = data_len;
  unsigned char c;

  ((uint64_t *)((void *)cbc))[0] = 0ULL;
  ((uint64_t *)((void *)cbc))[1] = 0ULL;

  while (remaining >= 16) {
    ((uint64_t *)((void *)cbc))[0] ^= ((uint64_t *)((void *)pos))[0];
    ((uint64_t *)((void *)cbc))[1] ^= ((uint64_t *)((void *)pos))[1];
    pos += 16;
    if (remaining > 16)
      Anode_aes256_encrypt(expkey,cbc,cbc);
    remaining -= 16;
  }

  ((uint64_t *)((void *)pad))[0] = 0ULL;
  ((uint64_t *)((void *)pad))[1] = 0ULL;
  Anode_aes256_encrypt(expkey,pad,pad);

  c = pad[0] & 0x80;
  for(i=0;i<15;++i)
    pad[i] = (pad[i] << 1) | (pad[i + 1] >> 7);
  pad[15] <<= 1;
  if (c)
    pad[15] ^= 0x87;

  if (remaining||(!data_len)) {
    for(i=0;i<remaining;++i)
      cbc[i] ^= *(pos++);
    cbc[remaining] ^= 0x80;

    c = pad[0] & 0x80;
    for(i=0;i<15;++i)
      pad[i] = (pad[i] << 1) | (pad[i + 1] >> 7);
    pad[15] <<= 1;
    if (c)
      pad[15] ^= 0x87;
  }

  ((uint64_t *)((void *)mac))[0] = ((uint64_t *)((void *)pad))[0] ^ ((uint64_t *)((void *)cbc))[0];
  ((uint64_t *)((void *)mac))[1] = ((uint64_t *)((void *)pad))[1] ^ ((uint64_t *)((void *)cbc))[1];

  Anode_aes256_encrypt(expkey,mac,mac);
}
