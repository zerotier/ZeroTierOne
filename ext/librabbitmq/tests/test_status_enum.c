/*
 * Copyright 2015 Alan Antonuk
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

#include "amqp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check_errorstrings(amqp_status_enum start, amqp_status_enum end) {
  int i;
  for (i = start; i > end; --i) {
    const char* err = amqp_error_string2(i);
    if (0 == strcmp(err, "(unknown error)")) {
      printf("amqp_status_enum value %s%X", i < 0 ? "-" : "", (unsigned)i);
      abort();
    }
  }
}

int main(void) {
  check_errorstrings(AMQP_STATUS_OK, _AMQP_STATUS_NEXT_VALUE);
  check_errorstrings(AMQP_STATUS_TCP_ERROR, _AMQP_STATUS_TCP_NEXT_VALUE);
  check_errorstrings(AMQP_STATUS_SSL_ERROR, _AMQP_STATUS_SSL_NEXT_VALUE);

  return 0;
}
