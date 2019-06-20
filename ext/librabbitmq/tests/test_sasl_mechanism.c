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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <amqp_socket.h>

static void parse_success(amqp_bytes_t mechanisms,
                          amqp_sasl_method_enum method) {
  if (!sasl_mechanism_in_list(mechanisms, method)) {
    fprintf(stderr, "Expected to find mechanism in list, but didn't: %s\n",
            (char *)mechanisms.bytes);
    abort();
  }
}

static void parse_fail(amqp_bytes_t mechanisms, amqp_sasl_method_enum method) {
  if (sasl_mechanism_in_list(mechanisms, method)) {
    fprintf(stderr,
            "Expected the mechanism not on the list, but it was present: %s\n",
            (char *)mechanisms.bytes);
    abort();
  }
}

int main(void) {
  parse_success(amqp_cstring_bytes("DIGEST-MD5 CRAM-MD5 LOGIN PLAIN"),
                AMQP_SASL_METHOD_PLAIN);
  parse_fail(amqp_cstring_bytes("DIGEST-MD5 CRAM-MD5 LOGIN PLAIN"),
             AMQP_SASL_METHOD_EXTERNAL);
  parse_success(amqp_cstring_bytes("DIGEST-MD5 CRAM-MD5 EXTERNAL"),
                AMQP_SASL_METHOD_EXTERNAL);
  parse_fail(amqp_cstring_bytes("DIGEST-MD5 CRAM-MD5 EXTERNAL"),
             AMQP_SASL_METHOD_PLAIN);
  return 0;
}
