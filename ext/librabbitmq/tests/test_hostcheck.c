/*
 * Copyright 2014 Michael Steinert
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "amqp_hostcheck.h"

#include <stdio.h>
#include <stdlib.h>

static void hostcheck_success(const char *match_pattern, const char *url) {
  int ok;

  ok = amqp_hostcheck(match_pattern, url);
  if (!ok) {
    fprintf(stderr, "Expected hostname check to pass, but didn't: %s (%s)\n",
            url, match_pattern);
    abort();
  }

  fprintf(stdout, "ok: [success] %s, %s\n", url, match_pattern);
}

static void hostcheck_fail(const char *match_pattern, const char *url) {
  int ok;

  ok = amqp_hostcheck(match_pattern, url);
  if (ok) {
    fprintf(stderr, "Expected hostname check to fail, but didn't: %s (%s)\n",
            url, match_pattern);
    abort();
  }

  fprintf(stdout, "ok: [fail] %s, %s\n", url, match_pattern);
}

int main(void) {
  hostcheck_success("www.rabbitmq.com", "www.rabbitmq.com");
  hostcheck_success("www.rabbitmq.com", "wWw.RaBbItMq.CoM");
  hostcheck_success("*.rabbitmq.com", "wWw.RaBbItMq.CoM");
  hostcheck_fail("rabbitmq.com", "www.rabbitmq.com");
  hostcheck_success("*.rabbitmq.com", "www.rabbitmq.com");
  hostcheck_fail("*.com", "www.rabbitmq.com");
  hostcheck_fail("*.rabbitmq.com", "long.url.rabbitmq.com");
  hostcheck_success("*.url.rabbitmq.com", "long.url.rabbitmq.com");

  return 0;
}
