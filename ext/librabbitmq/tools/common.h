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

#include <stdint.h>

#include <popt.h>

#include <amqp.h>
#include <amqp_framing.h>

extern const char *amqp_server_exception_string(amqp_rpc_reply_t r);
extern const char *amqp_rpc_reply_string(amqp_rpc_reply_t r);

extern void die(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
extern void die_errno(int err, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
extern void die_amqp_error(int err, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
extern void die_rpc(amqp_rpc_reply_t r, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

extern const char *connect_options_title;
extern struct poptOption connect_options[];
extern amqp_connection_state_t make_connection(void);
extern void close_connection(amqp_connection_state_t conn);

extern amqp_bytes_t read_all(int fd);
extern void write_all(int fd, amqp_bytes_t data);

extern void copy_body(amqp_connection_state_t conn, int fd);

#define INCLUDE_OPTIONS(options) \
  { NULL, 0, POPT_ARG_INCLUDE_TABLE, options, 0, options##_title, NULL }

extern poptContext process_options(int argc, const char **argv,
                                   struct poptOption *options,
                                   const char *help);
extern void process_all_options(int argc, const char **argv,
                                struct poptOption *options);

extern amqp_bytes_t cstring_bytes(const char *str);
