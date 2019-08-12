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

#include "config.h"

#ifdef _MSC_VER
/* MSVC complains about strdup being deprecated in favor of _strdup */
#define _CRT_NONSTDC_NO_DEPRECATE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <inttypes.h>

#include <amqp.h>

static void match_string(const char *what, const char *expect,
                         const char *got) {
  if (strcmp(got, expect)) {
    fprintf(stderr, "Expected %s '%s', got '%s'\n", what, expect, got);
    abort();
  }
}

static void match_int(const char *what, int expect, int got) {
  if (got != expect) {
    fprintf(stderr, "Expected %s '%d', got '%d'\n", what, expect, got);
    abort();
  }
}

static void parse_success(const char *url, const char *user,
                          const char *password, const char *host, int port,
                          const char *vhost) {
  char *s = strdup(url);
  struct amqp_connection_info ci;
  int res;

  res = amqp_parse_url(s, &ci);
  if (res) {
    fprintf(stderr, "Expected to successfully parse URL, but didn't: %s (%s)\n",
            url, amqp_error_string2(res));
    abort();
  }

  match_string("user", user, ci.user);
  match_string("password", password, ci.password);
  match_string("host", host, ci.host);
  match_int("port", port, ci.port);
  match_string("vhost", vhost, ci.vhost);

  free(s);
}

static void parse_fail(const char *url) {
  char *s = strdup(url);
  struct amqp_connection_info ci;

  amqp_default_connection_info(&ci);
  if (amqp_parse_url(s, &ci) >= 0) {
    fprintf(stderr, "Expected to fail parsing URL, but didn't: %s\n", url);
    abort();
  }

  free(s);
}

int main(void) {
  /* From the spec */
  parse_success("amqp://user:pass@host:10000/vhost", "user", "pass", "host",
                10000, "vhost");
  parse_success("amqps://user:pass@host:10000/vhost", "user", "pass", "host",
                10000, "vhost");

  parse_success("amqp://user%61:%61pass@ho%61st:10000/v%2fhost", "usera",
                "apass", "hoast", 10000, "v/host");
  parse_success("amqps://user%61:%61pass@ho%61st:10000/v%2fhost", "usera",
                "apass", "hoast", 10000, "v/host");

  parse_success("amqp://", "guest", "guest", "localhost", 5672, "/");
  parse_success("amqps://", "guest", "guest", "localhost", 5671, "/");

  parse_success("amqp://:@/", "", "", "localhost", 5672, "");
  parse_success("amqps://:@/", "", "", "localhost", 5671, "");

  parse_success("amqp://user@", "user", "guest", "localhost", 5672, "/");
  parse_success("amqps://user@", "user", "guest", "localhost", 5671, "/");

  parse_success("amqp://user:pass@", "user", "pass", "localhost", 5672, "/");
  parse_success("amqps://user:pass@", "user", "pass", "localhost", 5671, "/");

  parse_success("amqp://host", "guest", "guest", "host", 5672, "/");
  parse_success("amqps://host", "guest", "guest", "host", 5671, "/");

  parse_success("amqp://:10000", "guest", "guest", "localhost", 10000, "/");
  parse_success("amqps://:10000", "guest", "guest", "localhost", 10000, "/");

  parse_success("amqp:///vhost", "guest", "guest", "localhost", 5672, "vhost");
  parse_success("amqps:///vhost", "guest", "guest", "localhost", 5671, "vhost");

  parse_success("amqp://host/", "guest", "guest", "host", 5672, "");
  parse_success("amqps://host/", "guest", "guest", "host", 5671, "");

  parse_success("amqp://host/%2f", "guest", "guest", "host", 5672, "/");
  parse_success("amqps://host/%2f", "guest", "guest", "host", 5671, "/");

  parse_success("amqp://[::1]", "guest", "guest", "::1", 5672, "/");
  parse_success("amqps://[::1]", "guest", "guest", "::1", 5671, "/");

  /* Various other success cases */
  parse_success("amqp://host:100", "guest", "guest", "host", 100, "/");
  parse_success("amqps://host:100", "guest", "guest", "host", 100, "/");

  parse_success("amqp://[::1]:100", "guest", "guest", "::1", 100, "/");
  parse_success("amqps://[::1]:100", "guest", "guest", "::1", 100, "/");

  parse_success("amqp://host/blah", "guest", "guest", "host", 5672, "blah");
  parse_success("amqps://host/blah", "guest", "guest", "host", 5671, "blah");

  parse_success("amqp://host:100/blah", "guest", "guest", "host", 100, "blah");
  parse_success("amqps://host:100/blah", "guest", "guest", "host", 100, "blah");

  parse_success("amqp://:100/blah", "guest", "guest", "localhost", 100, "blah");
  parse_success("amqps://:100/blah", "guest", "guest", "localhost", 100,
                "blah");

  parse_success("amqp://[::1]/blah", "guest", "guest", "::1", 5672, "blah");
  parse_success("amqps://[::1]/blah", "guest", "guest", "::1", 5671, "blah");

  parse_success("amqp://[::1]:100/blah", "guest", "guest", "::1", 100, "blah");
  parse_success("amqps://[::1]:100/blah", "guest", "guest", "::1", 100, "blah");

  parse_success("amqp://user:pass@host", "user", "pass", "host", 5672, "/");
  parse_success("amqps://user:pass@host", "user", "pass", "host", 5671, "/");

  parse_success("amqp://user:pass@host:100", "user", "pass", "host", 100, "/");
  parse_success("amqps://user:pass@host:100", "user", "pass", "host", 100, "/");

  parse_success("amqp://user:pass@:100", "user", "pass", "localhost", 100, "/");
  parse_success("amqps://user:pass@:100", "user", "pass", "localhost", 100,
                "/");

  parse_success("amqp://user:pass@[::1]", "user", "pass", "::1", 5672, "/");
  parse_success("amqps://user:pass@[::1]", "user", "pass", "::1", 5671, "/");

  parse_success("amqp://user:pass@[::1]:100", "user", "pass", "::1", 100, "/");
  parse_success("amqps://user:pass@[::1]:100", "user", "pass", "::1", 100, "/");

  /* Various failure cases */
  parse_fail("http://www.rabbitmq.com");

  parse_fail("amqp://foo:bar:baz");
  parse_fail("amqps://foo:bar:baz");

  parse_fail("amqp://foo[::1]");
  parse_fail("amqps://foo[::1]");

  parse_fail("amqp://foo[::1]");
  parse_fail("amqps://foo[::1]");

  parse_fail("amqp://foo:[::1]");
  parse_fail("amqps://foo:[::1]");

  parse_fail("amqp://[::1]foo");
  parse_fail("amqps://[::1]foo");

  parse_fail("amqp://foo:1000xyz");
  parse_fail("amqps://foo:1000xyz");

  parse_fail("amqp://foo:1000000");
  parse_fail("amqps://foo:1000000");

  parse_fail("amqp://foo/bar/baz");
  parse_fail("amqps://foo/bar/baz");

  parse_fail("amqp://foo%1");
  parse_fail("amqps://foo%1");

  parse_fail("amqp://foo%1x");
  parse_fail("amqps://foo%1x");

  parse_fail("amqp://foo%xy");
  parse_fail("amqps://foo%xy");

  return 0;
}
