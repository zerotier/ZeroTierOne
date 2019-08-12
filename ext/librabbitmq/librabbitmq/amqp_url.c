/*
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MIT
 *
 * Portions created by Alan Antonuk are Copyright (c) 2012-2013
 * Alan Antonuk. All Rights Reserved.
 *
 * Portions created by VMware are Copyright (c) 2007-2012 VMware, Inc.
 * All Rights Reserved.
 *
 * Portions created by Tony Garnock-Jones are Copyright (c) 2009-2010
 * VMware, Inc. and Tony Garnock-Jones. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * ***** END LICENSE BLOCK *****
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "amqp_private.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void amqp_default_connection_info(struct amqp_connection_info *ci) {
  /* Apply defaults */
  ci->user = "guest";
  ci->password = "guest";
  ci->host = "localhost";
  ci->port = 5672;
  ci->vhost = "/";
  ci->ssl = 0;
}

/* Scan for the next delimiter, handling percent-encodings on the way. */
static char find_delim(char **pp, int colon_and_at_sign_are_delims) {
  char *from = *pp;
  char *to = from;

  for (;;) {
    char ch = *from++;

    switch (ch) {
      case ':':
      case '@':
        if (!colon_and_at_sign_are_delims) {
          *to++ = ch;
          break;
        }

      /* fall through */
      case 0:
      case '/':
      case '?':
      case '#':
      case '[':
      case ']':
        *to = 0;
        *pp = from;
        return ch;

      case '%': {
        unsigned int val;
        int chars;
        int res = sscanf(from, "%2x%n", &val, &chars);

        if (res == EOF || res < 1 || chars != 2 || val > CHAR_MAX)
        /* Return a surprising delimiter to
           force an error. */
        {
          return '%';
        }

        *to++ = (char)val;
        from += 2;
        break;
      }

      default:
        *to++ = ch;
        break;
    }
  }
}

/* Parse an AMQP URL into its component parts. */
int amqp_parse_url(char *url, struct amqp_connection_info *parsed) {
  int res = AMQP_STATUS_BAD_URL;
  char delim;
  char *start;
  char *host;
  char *port = NULL;

  amqp_default_connection_info(parsed);

  /* check the prefix */
  if (!strncmp(url, "amqp://", 7)) {
    /* do nothing */
  } else if (!strncmp(url, "amqps://", 8)) {
    parsed->port = 5671;
    parsed->ssl = 1;
  } else {
    goto out;
  }

  host = start = url += (parsed->ssl ? 8 : 7);
  delim = find_delim(&url, 1);

  if (delim == ':') {
    /* The colon could be introducing the port or the
       password part of the userinfo.  We don't know yet,
       so stash the preceding component. */
    port = start = url;
    delim = find_delim(&url, 1);
  }

  if (delim == '@') {
    /* What might have been the host and port were in fact
       the username and password */
    parsed->user = host;
    if (port) {
      parsed->password = port;
    }

    port = NULL;
    host = start = url;
    delim = find_delim(&url, 1);
  }

  if (delim == '[') {
    /* IPv6 address.  The bracket should be the first
       character in the host. */
    if (host != start || *host != 0) {
      goto out;
    }

    start = url;
    delim = find_delim(&url, 0);

    if (delim != ']') {
      goto out;
    }

    parsed->host = start;
    start = url;
    delim = find_delim(&url, 1);

    /* Closing bracket should be the last character in the
       host. */
    if (*start != 0) {
      goto out;
    }
  } else {
    /* If we haven't seen the host yet, this is it. */
    if (*host != 0) {
      parsed->host = host;
    }
  }

  if (delim == ':') {
    port = start = url;
    delim = find_delim(&url, 1);
  }

  if (port) {
    char *end;
    long portnum = strtol(port, &end, 10);

    if (port == end || *end != 0 || portnum < 0 || portnum > 65535) {
      goto out;
    }

    parsed->port = portnum;
  }

  if (delim == '/') {
    start = url;
    delim = find_delim(&url, 1);

    if (delim != 0) {
      goto out;
    }

    parsed->vhost = start;
    res = AMQP_STATUS_OK;
  } else if (delim == 0) {
    res = AMQP_STATUS_OK;
  }

/* Any other delimiter is bad, and we will return AMQP_STATUS_BAD_AMQP_URL. */

out:
  return res;
}
