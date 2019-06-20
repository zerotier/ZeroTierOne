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
/* MSVC complains about sprintf being deprecated in favor of sprintf_s */
#define _CRT_SECURE_NO_WARNINGS
/* MSVC complains about strdup being deprecated in favor of _strdup */
#define _CRT_NONSTDC_NO_DEPRECATE
#endif

#include "amqp_private.h"
#include "amqp_time.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ERROR_MASK (0x00FF)
#define ERROR_CATEGORY_MASK (0xFF00)

enum error_category_enum_ { EC_base = 0, EC_tcp = 1, EC_ssl = 2 };

static const char *base_error_strings[] = {
    /* AMQP_STATUS_OK 0x0 */
    "operation completed successfully",
    /* AMQP_STATUS_NO_MEMORY                  -0x0001 */
    "could not allocate memory",
    /* AMQP_STATUS_BAD_AQMP_DATA              -0x0002 */
    "invalid AMQP data",
    /* AMQP_STATUS_UNKNOWN_CLASS              -0x0003 */
    "unknown AMQP class id",
    /* AMQP_STATUS_UNKNOWN_METHOD             -0x0004 */
    "unknown AMQP method id",
    /* AMQP_STATUS_HOSTNAME_RESOLUTION_FAILED -0x0005 */
    "hostname lookup failed",
    /* AMQP_STATUS_INCOMPATIBLE_AMQP_VERSION  -0x0006 */
    "incompatible AMQP version",
    /* AMQP_STATUS_CONNECTION_CLOSED          -0x0007 */
    "connection closed unexpectedly",
    /* AMQP_STATUS_BAD_AMQP_URL               -0x0008 */
    "could not parse AMQP URL",
    /* AMQP_STATUS_SOCKET_ERROR               -0x0009 */
    "a socket error occurred",
    /* AMQP_STATUS_INVALID_PARAMETER          -0x000A */
    "invalid parameter",
    /* AMQP_STATUS_TABLE_TOO_BIG              -0x000B */
    "table too large for buffer",
    /* AMQP_STATUS_WRONG_METHOD               -0x000C */
    "unexpected method received",
    /* AMQP_STATUS_TIMEOUT                    -0x000D */
    "request timed out",
    /* AMQP_STATUS_TIMER_FAILED               -0x000E */
    "system timer has failed",
    /* AMQP_STATUS_HEARTBEAT_TIMEOUT          -0x000F */
    "heartbeat timeout, connection closed",
    /* AMQP_STATUS_UNEXPECTED STATE           -0x0010 */
    "unexpected protocol state",
    /* AMQP_STATUS_SOCKET_CLOSED              -0x0011 */
    "socket is closed",
    /* AMQP_STATUS_SOCKET_INUSE               -0x0012 */
    "socket already open",
    /* AMQP_STATUS_BROKER_UNSUPPORTED_SASL_METHOD -0x00013 */
    "unsupported sasl method requested",
    /* AMQP_STATUS_UNSUPPORTED                -0x0014 */
    "parameter value is unsupported"};

static const char *tcp_error_strings[] = {
    /* AMQP_STATUS_TCP_ERROR                  -0x0100 */
    "a socket error occurred",
    /* AMQP_STATUS_TCP_SOCKETLIB_INIT_ERROR   -0x0101 */
    "socket library initialization failed"};

static const char *ssl_error_strings[] = {
    /* AMQP_STATUS_SSL_ERRO  R                -0x0200 */
    "a SSL error occurred",
    /* AMQP_STATUS_SSL_HOSTNAME_VERIFY_FAILED -0x0201 */
    "SSL hostname verification failed",
    /* AMQP_STATUS_SSL_PEER_VERIFY_FAILED     -0x0202 */
    "SSL peer cert verification failed",
    /* AMQP_STATUS_SSL_CONNECTION_FAILED      -0x0203 */
    "SSL handshake failed"};

static const char *unknown_error_string = "(unknown error)";

const char *amqp_error_string2(int code) {
  const char *error_string;
  size_t category = (((-code) & ERROR_CATEGORY_MASK) >> 8);
  size_t error = (-code) & ERROR_MASK;

  switch (category) {
    case EC_base:
      if (error < (sizeof(base_error_strings) / sizeof(char *))) {
        error_string = base_error_strings[error];
      } else {
        error_string = unknown_error_string;
      }
      break;

    case EC_tcp:
      if (error < (sizeof(tcp_error_strings) / sizeof(char *))) {
        error_string = tcp_error_strings[error];
      } else {
        error_string = unknown_error_string;
      }
      break;

    case EC_ssl:
      if (error < (sizeof(ssl_error_strings) / sizeof(char *))) {
        error_string = ssl_error_strings[error];
      } else {
        error_string = unknown_error_string;
      }

      break;

    default:
      error_string = unknown_error_string;
      break;
  }

  return error_string;
}

char *amqp_error_string(int code) {
  /* Previously sometimes clients had to flip the sign on a return value from a
   * function to get the correct error code. Now, all error codes are negative.
   * To keep people's legacy code running correctly, we map all error codes to
   * negative values.
   *
   * This is only done with this deprecated function.
   */
  if (code > 0) {
    code = -code;
  }
  return strdup(amqp_error_string2(code));
}

void amqp_abort(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  abort();
}

const amqp_bytes_t amqp_empty_bytes = {0, NULL};
const amqp_table_t amqp_empty_table = {0, NULL};
const amqp_array_t amqp_empty_array = {0, NULL};

int amqp_basic_publish(amqp_connection_state_t state, amqp_channel_t channel,
                       amqp_bytes_t exchange, amqp_bytes_t routing_key,
                       amqp_boolean_t mandatory, amqp_boolean_t immediate,
                       amqp_basic_properties_t const *properties,
                       amqp_bytes_t body) {
  amqp_frame_t f;
  size_t body_offset;
  size_t usable_body_payload_size =
      state->frame_max - (HEADER_SIZE + FOOTER_SIZE);
  int res;
  int flagz;

  amqp_basic_publish_t m;
  amqp_basic_properties_t default_properties;

  m.exchange = exchange;
  m.routing_key = routing_key;
  m.mandatory = mandatory;
  m.immediate = immediate;
  m.ticket = 0;

  /* TODO(alanxz): this heartbeat check is happening in the wrong place, it
   * should really be done in amqp_try_send/writev */
  res = amqp_time_has_past(state->next_recv_heartbeat);
  if (AMQP_STATUS_TIMER_FAILURE == res) {
    return res;
  } else if (AMQP_STATUS_TIMEOUT == res) {
    res = amqp_try_recv(state);
    if (AMQP_STATUS_TIMEOUT == res) {
      return AMQP_STATUS_HEARTBEAT_TIMEOUT;
    } else if (AMQP_STATUS_OK != res) {
      return res;
    }
  }

  res = amqp_send_method_inner(state, channel, AMQP_BASIC_PUBLISH_METHOD, &m,
                               AMQP_SF_MORE, amqp_time_infinite());
  if (res < 0) {
    return res;
  }

  if (properties == NULL) {
    memset(&default_properties, 0, sizeof(default_properties));
    properties = &default_properties;
  }

  f.frame_type = AMQP_FRAME_HEADER;
  f.channel = channel;
  f.payload.properties.class_id = AMQP_BASIC_CLASS;
  f.payload.properties.body_size = body.len;
  f.payload.properties.decoded = (void *)properties;

  if (body.len > 0) {
    flagz = AMQP_SF_MORE;
  } else {
    flagz = AMQP_SF_NONE;
  }
  res = amqp_send_frame_inner(state, &f, flagz, amqp_time_infinite());
  if (res < 0) {
    return res;
  }

  body_offset = 0;
  while (body_offset < body.len) {
    size_t remaining = body.len - body_offset;

    if (remaining == 0) {
      break;
    }

    f.frame_type = AMQP_FRAME_BODY;
    f.channel = channel;
    f.payload.body_fragment.bytes = amqp_offset(body.bytes, body_offset);
    if (remaining >= usable_body_payload_size) {
      f.payload.body_fragment.len = usable_body_payload_size;
      flagz = AMQP_SF_MORE;
    } else {
      f.payload.body_fragment.len = remaining;
      flagz = AMQP_SF_NONE;
    }

    body_offset += f.payload.body_fragment.len;
    res = amqp_send_frame_inner(state, &f, flagz, amqp_time_infinite());
    if (res < 0) {
      return res;
    }
  }

  return AMQP_STATUS_OK;
}

amqp_rpc_reply_t amqp_channel_close(amqp_connection_state_t state,
                                    amqp_channel_t channel, int code) {
  char codestr[13];
  amqp_method_number_t replies[2] = {AMQP_CHANNEL_CLOSE_OK_METHOD, 0};
  amqp_channel_close_t req;

  if (code < 0 || code > UINT16_MAX) {
    return amqp_rpc_reply_error(AMQP_STATUS_INVALID_PARAMETER);
  }

  req.reply_code = (uint16_t)code;
  req.reply_text.bytes = codestr;
  req.reply_text.len = sprintf(codestr, "%d", code);
  req.class_id = 0;
  req.method_id = 0;

  return amqp_simple_rpc(state, channel, AMQP_CHANNEL_CLOSE_METHOD, replies,
                         &req);
}

amqp_rpc_reply_t amqp_connection_close(amqp_connection_state_t state,
                                       int code) {
  char codestr[13];
  amqp_method_number_t replies[2] = {AMQP_CONNECTION_CLOSE_OK_METHOD, 0};
  amqp_channel_close_t req;

  if (code < 0 || code > UINT16_MAX) {
    return amqp_rpc_reply_error(AMQP_STATUS_INVALID_PARAMETER);
  }

  req.reply_code = (uint16_t)code;
  req.reply_text.bytes = codestr;
  req.reply_text.len = sprintf(codestr, "%d", code);
  req.class_id = 0;
  req.method_id = 0;

  return amqp_simple_rpc(state, 0, AMQP_CONNECTION_CLOSE_METHOD, replies, &req);
}

int amqp_basic_ack(amqp_connection_state_t state, amqp_channel_t channel,
                   uint64_t delivery_tag, amqp_boolean_t multiple) {
  amqp_basic_ack_t m;
  m.delivery_tag = delivery_tag;
  m.multiple = multiple;
  return amqp_send_method(state, channel, AMQP_BASIC_ACK_METHOD, &m);
}

amqp_rpc_reply_t amqp_basic_get(amqp_connection_state_t state,
                                amqp_channel_t channel, amqp_bytes_t queue,
                                amqp_boolean_t no_ack) {
  amqp_method_number_t replies[] = {AMQP_BASIC_GET_OK_METHOD,
                                    AMQP_BASIC_GET_EMPTY_METHOD, 0};
  amqp_basic_get_t req;
  req.ticket = 0;
  req.queue = queue;
  req.no_ack = no_ack;

  state->most_recent_api_result =
      amqp_simple_rpc(state, channel, AMQP_BASIC_GET_METHOD, replies, &req);
  return state->most_recent_api_result;
}

int amqp_basic_reject(amqp_connection_state_t state, amqp_channel_t channel,
                      uint64_t delivery_tag, amqp_boolean_t requeue) {
  amqp_basic_reject_t req;
  req.delivery_tag = delivery_tag;
  req.requeue = requeue;
  return amqp_send_method(state, channel, AMQP_BASIC_REJECT_METHOD, &req);
}

int amqp_basic_nack(amqp_connection_state_t state, amqp_channel_t channel,
                    uint64_t delivery_tag, amqp_boolean_t multiple,
                    amqp_boolean_t requeue) {
  amqp_basic_nack_t req;
  req.delivery_tag = delivery_tag;
  req.multiple = multiple;
  req.requeue = requeue;
  return amqp_send_method(state, channel, AMQP_BASIC_NACK_METHOD, &req);
}

struct timeval *amqp_get_handshake_timeout(amqp_connection_state_t state) {
  return state->handshake_timeout;
}

int amqp_set_handshake_timeout(amqp_connection_state_t state,
                               struct timeval *timeout) {
  if (timeout) {
    if (timeout->tv_sec < 0 || timeout->tv_usec < 0) {
      return AMQP_STATUS_INVALID_PARAMETER;
    }
    state->internal_handshake_timeout = *timeout;
    state->handshake_timeout = &state->internal_handshake_timeout;
  } else {
    state->handshake_timeout = NULL;
  }

  return AMQP_STATUS_OK;
}

struct timeval *amqp_get_rpc_timeout(amqp_connection_state_t state) {
  return state->rpc_timeout;
}

int amqp_set_rpc_timeout(amqp_connection_state_t state,
                         struct timeval *timeout) {
  if (timeout) {
    if (timeout->tv_sec < 0 || timeout->tv_usec < 0) {
      return AMQP_STATUS_INVALID_PARAMETER;
    }
    state->rpc_timeout = &state->internal_rpc_timeout;
    *state->rpc_timeout = *timeout;
  } else {
    state->rpc_timeout = NULL;
  }
  return AMQP_STATUS_OK;
}
