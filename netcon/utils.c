/*
 * ZeroTier One - Network Virtualization Everywhere
 * Copyright (C) 2011-2015  ZeroTier, Inc.
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --
 *
 * ZeroTier may be used and distributed under the terms of the GPLv3, which
 * are available at: http://www.gnu.org/licenses/gpl-3.0.html
 *
 * If you would like to embed ZeroTier into a commercial application or
 * redistribute it in a modified binary form, please contact ZeroTier Networks
 * LLC. Start here: http://www.zerotier.com/
 */

void get_path_from_pid(char* dest, int pid)
{
  char ppath[50];
  sprintf(ppath, "/proc/%d/exe", pid);
  if (readlink (ppath, dest, 50) != -1){
  }
}

void print_ip(int ip)
{
  unsigned char bytes[4];
  bytes[0] = ip & 0xFF;
  bytes[1] = (ip >> 8) & 0xFF;
  bytes[2] = (ip >> 16) & 0xFF;
  bytes[3] = (ip >> 24) & 0xFF;
  printf("%d.%d.%d.%d\n", bytes[0], bytes[1], bytes[2], bytes[3]);
  //return buf;
}


/* --- */


#ifdef NETCON_SERVICE
ip_addr_t convert_ip(struct sockaddr_in * addr)
{
  ip_addr_t conn_addr;
  struct sockaddr_in *ipv4 = addr;
  short a = ip4_addr1(&(ipv4->sin_addr));
  short b = ip4_addr2(&(ipv4->sin_addr));
  short c = ip4_addr3(&(ipv4->sin_addr));
  short d = ip4_addr4(&(ipv4->sin_addr));
  IP4_ADDR(&conn_addr, a,b,c,d);
  return conn_addr;
}

ip_addr_t ip_addr_sin(register struct sockaddr_in *sin) {
   ip_addr_t ip;
   *((struct sockaddr_in*) &ip) = *sin;
   return ip;
}
#endif


/* --- */


#ifdef NETCON_INTERCEPT

typedef unsigned char u8_t;

#define 	ip4_addr1(ipaddr)   (((u8_t*)(ipaddr))[0])
#define 	ip4_addr2(ipaddr)   (((u8_t*)(ipaddr))[1])
#define 	ip4_addr3(ipaddr)   (((u8_t*)(ipaddr))[2])
#define 	ip4_addr4(ipaddr)   (((u8_t*)(ipaddr))[3])

int is_local(struct sockaddr_in* addr)
{
  struct sockaddr_in *ipv4 = addr;
  short a = ip4_addr1(&(ipv4->sin_addr));
  short b = ip4_addr2(&(ipv4->sin_addr));
  short c = ip4_addr3(&(ipv4->sin_addr));
  short d = ip4_addr4(&(ipv4->sin_addr));
  return (a==127 && b == 0 && c == 0 && d == 1);
}
#endif
