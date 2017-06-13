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

#include "impl/aes.h"
#include "impl/ec.h"
#include "impl/misc.h"
#include "impl/types.h"
#include "anode.h"

int AnodeAddress_calc_short_id(
  const AnodeAddress *address,
  AnodeAddressId *short_address_id)
{
  unsigned char digest[16];

  switch(AnodeAddress_get_type(address)) {
    case ANODE_ADDRESS_ANODE_256_40:
      Anode_aes_digest(address->bits,ANODE_ADDRESS_LENGTH_ANODE_256_40,digest);
      break;
    default:
      return ANODE_ERR_ADDRESS_INVALID;
  }

  *((uint64_t *)short_address_id->bits) = ((uint64_t *)digest)[0] ^ ((uint64_t *)digest)[1];

  return 0;
}

int AnodeAddress_get_zone(const AnodeAddress *address,AnodeZone *zone)
{
  switch(AnodeAddress_get_type(address)) {
    case ANODE_ADDRESS_ANODE_256_40:
      *((uint32_t *)&(zone->bits[0])) = *((uint32_t *)&(address->bits[1]));
      return 0;
  }
  return ANODE_ERR_ADDRESS_INVALID;
}

int AnodeAddress_to_string(const AnodeAddress *address,char *buf,int len)
{
  const unsigned char *inptr;
  char *outptr;
  unsigned int i;

  switch(AnodeAddress_get_type(address)) {
    case ANODE_ADDRESS_ANODE_256_40:
      if (len < (((ANODE_ADDRESS_LENGTH_ANODE_256_40 / 5) * 8) + 1))
        return ANODE_ERR_BUFFER_TOO_SMALL;
      inptr = (const unsigned char *)address->bits;
      outptr = buf;
      for(i=0;i<(ANODE_ADDRESS_LENGTH_ANODE_256_40 / 5);++i) {
        Anode_base32_5_to_8(inptr,outptr);
        inptr += 5;
        outptr += 8;
      }
      *outptr = (char)0;
      return ((ANODE_ADDRESS_LENGTH_ANODE_256_40 / 5) * 8);
  }
  return ANODE_ERR_ADDRESS_INVALID;
}

int AnodeAddress_from_string(const char *str,AnodeAddress *address)
{
  const char *blk_start = str;
  const char *ptr = str;
  unsigned int address_len = 0;

  while (*ptr) {
    if ((unsigned long)(ptr - blk_start) == 8) {
      if ((address_len + 5) > sizeof(address->bits))
        return ANODE_ERR_ADDRESS_INVALID;
      Anode_base32_8_to_5(blk_start,(unsigned char *)&(address->bits[address_len]));
      address_len += 5;
      blk_start = ptr;
    }
    ++ptr;
  }

  if (ptr != blk_start)
    return ANODE_ERR_ADDRESS_INVALID;
  if (AnodeAddress_get_type(address) != ANODE_ADDRESS_ANODE_256_40)
    return ANODE_ERR_ADDRESS_INVALID;

  return 0;
}
