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

#include <stdio.h>
#include <stdlib.h>
#include "impl/misc.h"
#include "anode.h"

int AnodeURI_parse(AnodeURI *parsed_uri,const char *uri_string)
{
  char buf[sizeof(AnodeURI)];
  unsigned long ptr = 0;
  char c;
  char *p1,*p2;

  Anode_zero((void *)parsed_uri,sizeof(AnodeURI));

  /* Get the scheme */
  for(;;) {
    c = *(uri_string++);
    if (!c) {
      parsed_uri->scheme[ptr] = (char)0;
      return ANODE_ERR_INVALID_URI;
    } else if (c == ':') {
      parsed_uri->scheme[ptr] = (char)0;
      break;
    } else {
      parsed_uri->scheme[ptr++] = c;
      if (ptr == sizeof(parsed_uri->scheme))
        return ANODE_ERR_BUFFER_TOO_SMALL;
    }
  }

  if (*uri_string == '/') {
    /* If it starts with /, it's a URL */

    /* Skip double slash */
    if (!(*(++uri_string)))
      return 0; /* Scheme with no path */
    if (*uri_string == '/') {
      if (!(*(++uri_string)))
        return 0; /* Scheme with no path */
    }

    /* Get the host section and put it in buf[] */
    ptr = 0;
    while ((*uri_string)&&(*uri_string != '/')) {
      buf[ptr++] = *(uri_string++);
      if (ptr == sizeof(buf))
        return ANODE_ERR_BUFFER_TOO_SMALL;
    }
    buf[ptr] = (char)0;

    /* Parse host section for host, username, password, and port */
    if (buf[0]) {
      p1 = (char *)Anode_strchr(buf,'@');
      if (p1) {
        *(p1++) = (char)0;
        if (*p1) {
          p2 = (char *)Anode_strchr(buf,':');
          if (p2) {
            *(p2++) = (char)0;
            Anode_str_copy(parsed_uri->password,p2,sizeof(parsed_uri->password));
          }
          Anode_str_copy(parsed_uri->username,buf,sizeof(parsed_uri->username));
        } else return ANODE_ERR_INVALID_URI;
      } else p1 = buf;

      p2 = (char *)Anode_strchr(p1,':');
      if (p2) {
        *(p2++) = (char)0;
        if (*p2)
          parsed_uri->port = (int)strtoul(p2,(char **)0,10);
      }
      Anode_str_copy(parsed_uri->host,p1,sizeof(parsed_uri->host));
    }

    /* Get the path, query, and fragment section and put it in buf[] */
    ptr = 0;
    while ((buf[ptr++] = *(uri_string++))) {
      if (ptr == sizeof(buf))
        return ANODE_ERR_BUFFER_TOO_SMALL;
    }

    /* Parse path section for path, query, and fragment */
    if (buf[0]) {
      p1 = (char *)Anode_strchr(buf,'?');
      if (p1) {
        *(p1++) = (char)0;
        p2 = (char *)Anode_strchr(p1,'#');
        if (p2) {
          *(p2++) = (char)0;
          Anode_str_copy(parsed_uri->fragment,p2,sizeof(parsed_uri->fragment));
        }
        Anode_str_copy(parsed_uri->query,p1,sizeof(parsed_uri->query));
      } else {
        p2 = (char *)Anode_strchr(buf,'#');
        if (p2) {
          *(p2++) = (char)0;
          Anode_str_copy(parsed_uri->fragment,p2,sizeof(parsed_uri->fragment));
        }
      }
      Anode_str_copy(parsed_uri->path,buf,sizeof(parsed_uri->path));
    }
  } else {
    /* Otherwise, it's a URN and what remains is all path */
    ptr = 0;
    while ((parsed_uri->path[ptr++] = *(uri_string++))) {
      if (ptr == sizeof(parsed_uri->path))
        return ANODE_ERR_BUFFER_TOO_SMALL;
    }
  }

  return 0;
}

char *AnodeURI_to_string(const AnodeURI *uri,char *buf,int len)
{
  int i = 0;
  char portbuf[16];
  const char *p;

  p = uri->scheme;
  while (*p) { buf[i++] = *(p++); if (i >= len) return (char *)0; }

  buf[i++] = ':'; if (i >= len) return (char *)0;

  if (uri->host[0]) {
    buf[i++] = '/'; if (i >= len) return (char *)0;
    buf[i++] = '/'; if (i >= len) return (char *)0;

    if (uri->username[0]) {
      p = uri->username;
      while (*p) { buf[i++] = *(p++); if (i >= len) return (char *)0; }
      if (uri->password[0]) {
        buf[i++] = ':'; if (i >= len) return (char *)0;
        p = uri->password;
        while (*p) { buf[i++] = *(p++); if (i >= len) return (char *)0; }
      }
      buf[i++] = '@'; if (i >= len) return (char *)0;
    }

    p = uri->host;
    while (*p) { buf[i++] = *(p++); if (i >= len) return (char *)0; }

    if ((uri->port > 0)&&(uri->port <= 0xffff)) {
      buf[i++] = ':'; if (i >= len) return (char *)0;
      snprintf(portbuf,sizeof(portbuf),"%d",uri->port);
      p = portbuf;
      while (*p) { buf[i++] = *(p++); if (i >= len) return (char *)0; }
    }
  }

  p = uri->path;
  while (*p) { buf[i++] = *(p++); if (i >= len) return (char *)0; }

  if (uri->query[0]) {
    buf[i++] = '?'; if (i >= len) return (char *)0;
    p = uri->query;
    while (*p) { buf[i++] = *(p++); if (i >= len) return (char *)0; }
  }

  if (uri->fragment[0]) {
    buf[i++] = '#'; if (i >= len) return (char *)0;
    p = uri->fragment;
    while (*p) { buf[i++] = *(p++); if (i >= len) return (char *)0; }
  }

  buf[i] = (char)0;

  return buf;
}
