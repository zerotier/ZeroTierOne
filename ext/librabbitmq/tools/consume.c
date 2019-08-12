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

#include "common.h"
#include "process.h"

#define MAX_LISTEN_KEYS 1024
#define LISTEN_KEYS_DELIMITER ","

/* Convert a amqp_bytes_t to an escaped string form for printing.  We
   use the same escaping conventions as rabbitmqctl. */
static char *stringify_bytes(amqp_bytes_t bytes) {
  /* We will need up to 4 chars per byte, plus the terminating 0 */
  char *res = malloc(bytes.len * 4 + 1);
  uint8_t *data = bytes.bytes;
  char *p = res;
  size_t i;

  for (i = 0; i < bytes.len; i++) {
    if (data[i] >= 32 && data[i] != 127) {
      *p++ = data[i];
    } else {
      *p++ = '\\';
      *p++ = '0' + (data[i] >> 6);
      *p++ = '0' + (data[i] >> 3 & 0x7);
      *p++ = '0' + (data[i] & 0x7);
    }
  }

  *p = 0;
  return res;
}

static amqp_bytes_t setup_queue(amqp_connection_state_t conn, char *queue,
                                char *exchange, char *routing_key, int declare,
                                int exclusive) {
  amqp_bytes_t queue_bytes = cstring_bytes(queue);

  char *routing_key_rest;
  char *routing_key_token;
  char *routing_tmp;
  int routing_key_count = 0;

  /* if an exchange name wasn't provided, check that we don't have options that
   * require it. */
  if (!exchange && routing_key) {
    fprintf(stderr,
            "--routing-key option requires an exchange name to be provided "
            "with --exchange\n");
    exit(1);
  }

  if (!queue || exchange || declare || exclusive) {
    /* Declare the queue as auto-delete.  */
    amqp_queue_declare_ok_t *res = amqp_queue_declare(
        conn, 1, queue_bytes, 0, 0, exclusive, 1, amqp_empty_table);
    if (!res) {
      die_rpc(amqp_get_rpc_reply(conn), "queue.declare");
    }

    if (!queue) {
      /* the server should have provided a queue name */
      char *sq;
      queue_bytes = amqp_bytes_malloc_dup(res->queue);
      sq = stringify_bytes(queue_bytes);
      fprintf(stderr, "Server provided queue name: %s\n", sq);
      free(sq);
    }

    /* Bind to an exchange if requested */
    if (exchange) {
      amqp_bytes_t eb = amqp_cstring_bytes(exchange);

      routing_tmp = strdup(routing_key);
      if (NULL == routing_tmp) {
        fprintf(stderr, "could not allocate memory to parse routing key\n");
        exit(1);
      }

      for (routing_key_token =
               strtok_r(routing_tmp, LISTEN_KEYS_DELIMITER, &routing_key_rest);
           NULL != routing_key_token && routing_key_count < MAX_LISTEN_KEYS - 1;
           routing_key_token =
               strtok_r(NULL, LISTEN_KEYS_DELIMITER, &routing_key_rest)) {

        if (!amqp_queue_bind(conn, 1, queue_bytes, eb,
                             cstring_bytes(routing_key_token),
                             amqp_empty_table)) {
          die_rpc(amqp_get_rpc_reply(conn), "queue.bind");
        }
      }
      free(routing_tmp);
    }
  }

  return queue_bytes;
}

#define AMQP_CONSUME_MAX_PREFETCH_COUNT 65535

static void do_consume(amqp_connection_state_t conn, amqp_bytes_t queue,
                       int no_ack, int count, int prefetch_count,
                       const char *const *argv) {
  int i;

  /* If there is a limit, set the qos to match */
  if (count > 0 && count <= AMQP_CONSUME_MAX_PREFETCH_COUNT &&
      !amqp_basic_qos(conn, 1, 0, count, 0)) {
    die_rpc(amqp_get_rpc_reply(conn), "basic.qos");
  }

  /* if there is a maximum number of messages to be received at a time, set the
   * qos to match */
  if (prefetch_count > 0 && prefetch_count <= AMQP_CONSUME_MAX_PREFETCH_COUNT) {
    /* the maximum number of messages to be received at a time must be less
     * than the global maximum number of messages. */
    if (!(count > 0 && count <= AMQP_CONSUME_MAX_PREFETCH_COUNT &&
          prefetch_count >= count)) {
      if (!amqp_basic_qos(conn, 1, 0, prefetch_count, 0)) {
        die_rpc(amqp_get_rpc_reply(conn), "basic.qos");
      }
    }
  }

  if (!amqp_basic_consume(conn, 1, queue, amqp_empty_bytes, 0, no_ack, 0,
                          amqp_empty_table)) {
    die_rpc(amqp_get_rpc_reply(conn), "basic.consume");
  }

  for (i = 0; count < 0 || i < count; i++) {
    amqp_frame_t frame;
    struct pipeline pl;
    uint64_t delivery_tag;
    amqp_basic_deliver_t *deliver;
    int res = amqp_simple_wait_frame(conn, &frame);
    die_amqp_error(res, "waiting for header frame");

    if (frame.frame_type != AMQP_FRAME_METHOD ||
        frame.payload.method.id != AMQP_BASIC_DELIVER_METHOD) {
      continue;
    }

    deliver = (amqp_basic_deliver_t *)frame.payload.method.decoded;
    delivery_tag = deliver->delivery_tag;

    pipeline(argv, &pl);
    copy_body(conn, pl.infd);

    if (finish_pipeline(&pl) && !no_ack)
      die_amqp_error(amqp_basic_ack(conn, 1, delivery_tag, 0), "basic.ack");

    amqp_maybe_release_buffers(conn);
  }
}

int main(int argc, const char **argv) {
  poptContext opts;
  amqp_connection_state_t conn;
  const char *const *cmd_argv;
  static char *queue = NULL;
  static char *exchange = NULL;
  static char *routing_key = NULL;
  static int declare = 0;
  static int exclusive = 0;
  static int no_ack = 0;
  static int count = -1;
  static int prefetch_count = -1;
  amqp_bytes_t queue_bytes;

  struct poptOption options[] = {
      INCLUDE_OPTIONS(connect_options),
      {"queue", 'q', POPT_ARG_STRING, &queue, 0, "the queue to consume from",
       "queue"},
      {"exchange", 'e', POPT_ARG_STRING, &exchange, 0,
       "bind the queue to this exchange", "exchange"},
      {"routing-key", 'r', POPT_ARG_STRING, &routing_key, 0,
       "the routing key to bind with", "routing key"},
      {"declare", 'd', POPT_ARG_NONE, &declare, 0,
       "declare an exclusive queue (deprecated, use --exclusive instead)",
       NULL},
      {"exclusive", 'x', POPT_ARG_NONE, &exclusive, 0,
       "declare the queue as exclusive", NULL},
      {"no-ack", 'A', POPT_ARG_NONE, &no_ack, 0, "consume in no-ack mode",
       NULL},
      {"count", 'c', POPT_ARG_INT, &count, 0,
       "stop consuming after this many messages are consumed", "limit"},
      {"prefetch-count", 'p', POPT_ARG_INT, &prefetch_count, 0,
       "receive only this many message at a time from the server", "limit"},
      POPT_AUTOHELP{NULL, '\0', 0, NULL, 0, NULL, NULL}};

  opts = process_options(argc, argv, options, "[OPTIONS]... <command> <args>");

  cmd_argv = poptGetArgs(opts);
  if (!cmd_argv || !cmd_argv[0]) {
    fprintf(stderr, "consuming command not specified\n");
    poptPrintUsage(opts, stderr, 0);
    goto error;
  }

  conn = make_connection();
  queue_bytes =
      setup_queue(conn, queue, exchange, routing_key, declare, exclusive);
  do_consume(conn, queue_bytes, no_ack, count, prefetch_count, cmd_argv);
  close_connection(conn);
  return 0;

error:
  poptFreeContext(opts);
  return 1;
}
