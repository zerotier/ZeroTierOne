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

#include <netinet/in.h>
#include <arpa/inet.h>
#include "impl/misc.h"
#include "impl/types.h"
#include "anode.h"

const AnodeNetworkAddress AnodeNetworkAddress_ANY4 = {
  ANODE_NETWORK_ADDRESS_IPV4,
  { 0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }
};
const AnodeNetworkAddress AnodeNetworkAddress_ANY6 = {
  ANODE_NETWORK_ADDRESS_IPV6,
  { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 ,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }
};
const AnodeNetworkAddress AnodeNetworkAddress_LOCAL4 = {
  ANODE_NETWORK_ADDRESS_IPV4,
  { 127,0,0,1, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }
};
const AnodeNetworkAddress AnodeNetworkAddress_LOCAL6 = {
  ANODE_NETWORK_ADDRESS_IPV6,
  { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 ,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }
};

int AnodeNetworkAddress_to_string(const AnodeNetworkAddress *address,char *buf,int len)
{
  const char *s;

  switch(address->type) {
    case ANODE_NETWORK_ADDRESS_IPV4:
      s = inet_ntop(AF_INET,(const void *)address->bits,buf,len);
      if (s)
        return Anode_strlen(s);
      else return ANODE_ERR_INVALID_ARGUMENT;
      break;
    case ANODE_NETWORK_ADDRESS_IPV6:
      s = inet_ntop(AF_INET6,address->bits,buf,len);
      if (s)
        return Anode_strlen(s);
      else return ANODE_ERR_INVALID_ARGUMENT;
    /*
    case ANODE_NETWORK_ADDRESS_ETHERNET:
      break;
    case ANODE_NETWORK_ADDRESS_USB:
      break;
    case ANODE_NETWORK_ADDRESS_BLUETOOTH:
      break;
    case ANODE_NETWORK_ADDRESS_IPC:
      break;
    case ANODE_NETWORK_ADDRESS_80211S:
      break;
    case ANODE_NETWORK_ADDRESS_SERIAL:
      break;
    */
    case ANODE_NETWORK_ADDRESS_ANODE_256_40:
      return AnodeAddress_to_string((const AnodeAddress *)address->bits,buf,len);
    default:
      return ANODE_ERR_ADDRESS_TYPE_NOT_SUPPORTED;
  }
}

int AnodeNetworkAddress_from_string(const char *str,AnodeNetworkAddress *address)
{
  unsigned int dots = Anode_count_char(str,'.');
  unsigned int colons = Anode_count_char(str,':');

  if ((dots == 3)&&(!colons)) {
    address->type = ANODE_NETWORK_ADDRESS_IPV4;
    if (inet_pton(AF_INET,str,address->bits) > 0)
      return 0;
    else return ANODE_ERR_INVALID_ARGUMENT;
  } else if ((colons)&&(!dots)) {
    address->type = ANODE_NETWORK_ADDRESS_IPV6;
    if (inet_pton(AF_INET6,str,address->bits) > 0)
      return 0;
    else return ANODE_ERR_INVALID_ARGUMENT;
  } else {
    address->type = ANODE_NETWORK_ADDRESS_ANODE_256_40;
    return AnodeAddress_from_string(str,(AnodeAddress *)address->bits);
  }
}

int AnodeNetworkEndpoint_from_sockaddr(const void *sockaddr,AnodeNetworkEndpoint *endpoint)
{
  switch(((struct sockaddr_storage *)sockaddr)->ss_family) {
    case AF_INET:
      *((uint32_t *)endpoint->address.bits) = (uint32_t)(((struct sockaddr_in *)sockaddr)->sin_addr.s_addr);
      endpoint->port = (int)ntohs(((struct sockaddr_in *)sockaddr)->sin_port);
      return 0;
    case AF_INET6:
      Anode_memcpy(endpoint->address.bits,((struct sockaddr_in6 *)sockaddr)->sin6_addr.s6_addr,16);
      endpoint->port = (int)ntohs(((struct sockaddr_in6 *)sockaddr)->sin6_port);
      return 0;
    default:
      return ANODE_ERR_INVALID_ARGUMENT;
  }
}

int AnodeNetworkEndpoint_to_sockaddr(const AnodeNetworkEndpoint *endpoint,void *sockaddr,int sockaddr_len)
{
  switch(endpoint->address.type) {
    case ANODE_NETWORK_ADDRESS_IPV4:
      if (sockaddr_len < (int)sizeof(struct sockaddr_in))
        return ANODE_ERR_BUFFER_TOO_SMALL;
      Anode_zero(sockaddr,sizeof(struct sockaddr_in));
      ((struct sockaddr_in *)sockaddr)->sin_family = AF_INET;
      ((struct sockaddr_in *)sockaddr)->sin_port = htons((uint16_t)endpoint->port);
      ((struct sockaddr_in *)sockaddr)->sin_addr.s_addr = *((uint32_t *)endpoint->address.bits);
      return 0;
    case ANODE_NETWORK_ADDRESS_IPV6:
      if (sockaddr_len < (int)sizeof(struct sockaddr_in6))
        return ANODE_ERR_BUFFER_TOO_SMALL;
      Anode_zero(sockaddr,sizeof(struct sockaddr_in6));
      ((struct sockaddr_in6 *)sockaddr)->sin6_family = AF_INET6;
      ((struct sockaddr_in6 *)sockaddr)->sin6_port = htons((uint16_t)endpoint->port);
      Anode_memcpy(((struct sockaddr_in6 *)sockaddr)->sin6_addr.s6_addr,endpoint->address.bits,16);
      return 0;
    default:
      return ANODE_ERR_INVALID_ARGUMENT;
  }
}
