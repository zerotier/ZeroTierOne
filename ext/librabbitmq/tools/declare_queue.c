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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"

int main(int argc, const char **argv) {
  amqp_connection_state_t conn;
  static char *queue = NULL;
  static int durable = 0;

  struct poptOption options[] = {
      INCLUDE_OPTIONS(connect_options),
      {"queue", 'q', POPT_ARG_STRING, &queue, 0,
       "the queue name to declare, or the empty string", "queue"},
      {"durable", 'd', POPT_ARG_VAL, &durable, 1, "declare a durable queue",
       NULL},
      POPT_AUTOHELP{NULL, '\0', 0, NULL, 0, NULL, NULL}};

  process_all_options(argc, argv, options);

  if (queue == NULL) {
    fprintf(stderr, "queue name not specified\n");
    return 1;
  }

  conn = make_connection();
  {
    amqp_queue_declare_ok_t *reply = amqp_queue_declare(
        conn, 1, cstring_bytes(queue), 0, durable, 0, 0, amqp_empty_table);
    if (reply == NULL) {
      die_rpc(amqp_get_rpc_reply(conn), "queue.declare");
    }

    printf("%.*s\n", (int)reply->queue.len, (char *)reply->queue.bytes);
  }
  close_connection(conn);
  return 0;
}
