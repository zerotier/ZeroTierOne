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
#include "impl/types.h"
#include "impl/ec.h"
#include "impl/misc.h"
#include "anode.h"

int AnodeIdentity_generate(AnodeIdentity *identity,const AnodeZone *zone,enum AnodeAddressType type)
{
  struct AnodeECKeyPair kp;

  switch(type) {
    case ANODE_ADDRESS_ANODE_256_40:
      if (!AnodeECKeyPair_generate(&kp))
        return ANODE_ERR_OUT_OF_MEMORY;

      identity->address.bits[0] = (unsigned char)ANODE_ADDRESS_ANODE_256_40;

      identity->address.bits[1] = zone->bits[0];
      identity->address.bits[2] = zone->bits[1];
      identity->address.bits[3] = zone->bits[2];
      identity->address.bits[4] = zone->bits[3];

      identity->address.bits[5] = 0;
      identity->address.bits[6] = 0;

      Anode_memcpy((void *)&(identity->address.bits[7]),(const void *)kp.pub.key,ANODE_EC_PUBLIC_KEY_BYTES);
      Anode_memcpy((void *)identity->secret,(const void *)kp.priv.key,kp.priv.bytes);

      AnodeAddress_calc_short_id(&identity->address,&identity->address_id);

      AnodeECKeyPair_destroy(&kp);

      return 0;
  }

  return ANODE_ERR_INVALID_ARGUMENT;
}

int AnodeIdentity_to_string(const AnodeIdentity *identity,char *dest,int dest_len)
{
  char hexbuf[128];
  char strbuf[128];
  int n;

  if ((n = AnodeAddress_to_string(&identity->address,strbuf,sizeof(strbuf))) <= 0)
    return n;

  switch(AnodeAddress_get_type(&identity->address)) {
    case ANODE_ADDRESS_ANODE_256_40:
      Anode_to_hex((const unsigned char *)identity->secret,ANODE_ADDRESS_SECRET_LENGTH_ANODE_256_40,hexbuf,sizeof(hexbuf));
      n = snprintf(dest,dest_len,"ANODE-256-40:%s:%s",strbuf,hexbuf);
      if (n >= dest_len)
        return ANODE_ERR_BUFFER_TOO_SMALL;
      return n;
  }

  return ANODE_ERR_INVALID_ARGUMENT;
}

int AnodeIdentity_from_string(AnodeIdentity *identity,const char *str)
{
  char buf[1024];
  char *id_name;
  char *address;
  char *secret;
  int ec;

  Anode_str_copy(buf,str,sizeof(buf));

  id_name = buf;
  if (!id_name) return 0;
  if (!*id_name) return 0;
  address = (char *)Anode_strchr(id_name,':');
  if (!address) return 0;
  if (!*address) return 0;
  *(address++) = (char)0;
  secret = (char *)Anode_strchr(address,':');
  if (!secret) return 0;
  if (!*secret) return 0;
  *(secret++) = (char)0;

  if (Anode_strcaseeq("ANODE-256-40",id_name)) {
    if ((ec = AnodeAddress_from_string(address,&identity->address)))
      return ec;
    if (Anode_strlen(secret) != (ANODE_ADDRESS_SECRET_LENGTH_ANODE_256_40 * 2))
      return ANODE_ERR_INVALID_ARGUMENT;
    Anode_from_hex(secret,(unsigned char *)identity->secret,sizeof(identity->secret));
    AnodeAddress_calc_short_id(&identity->address,&identity->address_id);
    return 0;
  }

  return ANODE_ERR_INVALID_ARGUMENT;
}
