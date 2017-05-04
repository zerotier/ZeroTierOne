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
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <netdb.h>
#include "dns_txt.h"

#ifndef C_IN
#define C_IN ns_c_in
#endif
#ifndef T_TXT
#define T_TXT ns_t_txt
#endif

static volatile int Anode_resolver_initialized = 0;

int Anode_sync_resolve_txt(const char *host,char *txt,unsigned int txt_len)
{
  unsigned char answer[16384],*pptr,*end;
  char name[16384];
  int len,explen,i;

  if (!Anode_resolver_initialized) {
    Anode_resolver_initialized = 1;
    res_init();
  }

  /* Do not taunt happy fun ball. */

  len = res_search(host,C_IN,T_TXT,answer,sizeof(answer));
  if (len > 12) {
    pptr = answer + 12;
    end = answer + len;

    explen = dn_expand(answer,end,pptr,name,sizeof(name));
    if (explen > 0) {
      pptr += explen;

      if ((pptr + 2) >= end) return 2;
      if (ntohs(*((uint16_t *)pptr)) == T_TXT) {
        pptr += 4;
        if (pptr >= end) return 2;

        explen = dn_expand(answer,end,pptr,name,sizeof(name));
        if (explen > 0) {
          pptr += explen;

          if ((pptr + 2) >= end) return 2;
          if (ntohs(*((uint16_t *)pptr)) == T_TXT) {
            pptr += 10;
            if (pptr >= end) return 2;

            len = *(pptr++);
            if (len <= 0) return 2;
            if ((pptr + len) > end) return 2;

            if (txt_len < (len + 1))
              return 4;
            else {
              for(i=0;i<len;++i)
                txt[i] = pptr[i];
              txt[len] = (char)0;
              return 0;
            }
          }
        }
      }
    }
  }

  return 1;
}

