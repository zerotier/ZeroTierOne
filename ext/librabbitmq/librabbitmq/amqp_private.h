#ifndef librabbitmq_amqp_private_h
#define librabbitmq_amqp_private_h

/*
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MIT
 *
 * Portions created by Alan Antonuk are Copyright (c) 2012-2014
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

#define AMQ_COPYRIGHT                                       \
  "Copyright (c) 2007-2014 VMWare Inc, Tony Garnock-Jones," \
  " and Alan Antonuk."

#include "amqp.h"
#include "amqp_framing.h"
#include <string.h>

#if ((defined(_WIN32)) || (defined(__MINGW32__)) || (defined(__MINGW64__)))
#ifndef WINVER
/* WINVER 0x0502 is WinXP SP2+, Windows Server 2003 SP1+
 * See:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa383745(v=vs.85).aspx#macros_for_conditional_declarations
 */
#define WINVER 0x0502
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <sys/uio.h>
#endif

/* GCC attributes */
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
#define AMQP_NORETURN __attribute__((__noreturn__))
#define AMQP_UNUSED __attribute__((__unused__))
#elif defined(_MSC_VER)
#define AMQP_NORETURN __declspec(noreturn)
#define AMQP_UNUSED
#else
#define AMQP_NORETURN
#define AMQP_UNUSED
#endif

#if __GNUC__ >= 4
#define AMQP_PRIVATE __attribute__((visibility("hidden")))
#else
#define AMQP_PRIVATE
#endif

char *amqp_os_error_string(int err);

#ifdef WITH_SSL
char *amqp_ssl_error_string(int err);
#endif

#include "amqp_socket.h"
#include "amqp_time.h"

/*
 * Connection states: XXX FIX THIS
 *
 * - CONNECTION_STATE_INITIAL: The initial state, when we cannot be
 *   sure if the next thing we will get is the first AMQP frame, or a
 *   protocol header from the server.
 *
 * - CONNECTION_STATE_IDLE: The normal state between
 *   frames. Connections may only be reconfigured, and the
 *   connection's pools recycled, when in this state. Whenever we're
 *   in this state, the inbound_buffer's bytes pointer must be NULL;
 *   any other state, and it must point to a block of memory allocated
 *   from the frame_pool.
 *
 * - CONNECTION_STATE_HEADER: Some bytes of an incoming frame have
 *   been seen, but not a complete frame header's worth.
 *
 * - CONNECTION_STATE_BODY: A complete frame header has been seen, but
 *   the frame is not yet complete. When it is completed, it will be
 *   returned, and the connection will return to IDLE state.
 *
 */
typedef enum amqp_connection_state_enum_ {
  CONNECTION_STATE_IDLE = 0,
  CONNECTION_STATE_INITIAL,
  CONNECTION_STATE_HEADER,
  CONNECTION_STATE_BODY
} amqp_connection_state_enum;

typedef enum amqp_status_private_enum_ {
  /* 0x00xx -> AMQP_STATUS_*/
  /* 0x01xx -> AMQP_STATUS_TCP_* */
  /* 0x02xx -> AMQP_STATUS_SSL_* */
  AMQP_PRIVATE_STATUS_SOCKET_NEEDREAD = -0x1301,
  AMQP_PRIVATE_STATUS_SOCKET_NEEDWRITE = -0x1302
} amqp_status_private_enum;

/* 7 bytes up front, then payload, then 1 byte footer */
#define HEADER_SIZE 7
#define FOOTER_SIZE 1

#define AMQP_PSEUDOFRAME_PROTOCOL_HEADER 'A'

typedef struct amqp_link_t_ {
  struct amqp_link_t_ *next;
  void *data;
} amqp_link_t;

#define POOL_TABLE_SIZE 16

typedef struct amqp_pool_table_entry_t_ {
  struct amqp_pool_table_entry_t_ *next;
  amqp_pool_t pool;
  amqp_channel_t channel;
} amqp_pool_table_entry_t;

struct amqp_connection_state_t_ {
  amqp_pool_table_entry_t *pool_table[POOL_TABLE_SIZE];

  amqp_connection_state_enum state;

  int channel_max;
  int frame_max;

  /* Heartbeat interval in seconds. If this is <= 0, then heartbeats are not
   * enabled, and next_recv_heartbeat and next_send_heartbeat are set to
   * infinite */
  int heartbeat;
  amqp_time_t next_recv_heartbeat;
  amqp_time_t next_send_heartbeat;

  /* buffer for holding frame headers.  Allows us to delay allocating
   * the raw frame buffer until the type, channel, and size are all known
   */
  char header_buffer[HEADER_SIZE + 1];
  amqp_bytes_t inbound_buffer;

  size_t inbound_offset;
  size_t target_size;

  amqp_bytes_t outbound_buffer;

  amqp_socket_t *socket;

  amqp_bytes_t sock_inbound_buffer;
  size_t sock_inbound_offset;
  size_t sock_inbound_limit;

  amqp_link_t *first_queued_frame;
  amqp_link_t *last_queued_frame;

  amqp_rpc_reply_t most_recent_api_result;

  amqp_table_t server_properties;
  amqp_table_t client_properties;
  amqp_pool_t properties_pool;

  struct timeval *handshake_timeout;
  struct timeval internal_handshake_timeout;
  struct timeval *rpc_timeout;
  struct timeval internal_rpc_timeout;
};

amqp_pool_t *amqp_get_or_create_channel_pool(amqp_connection_state_t connection,
                                             amqp_channel_t channel);
amqp_pool_t *amqp_get_channel_pool(amqp_connection_state_t state,
                                   amqp_channel_t channel);

static inline int amqp_heartbeat_send(amqp_connection_state_t state) {
  return state->heartbeat;
}

static inline int amqp_heartbeat_recv(amqp_connection_state_t state) {
  return 2 * state->heartbeat;
}

int amqp_try_recv(amqp_connection_state_t state);

static inline void *amqp_offset(void *data, size_t offset) {
  return (char *)data + offset;
}

/* This macro defines the encoding and decoding functions associated with a
   simple type. */

#define DECLARE_CODEC_BASE_TYPE(bits)                                        \
                                                                             \
  static inline int amqp_encode_##bits(amqp_bytes_t encoded, size_t *offset, \
                                       uint##bits##_t input) {               \
    size_t o = *offset;                                                      \
    if ((*offset = o + bits / 8) <= encoded.len) {                           \
      amqp_e##bits(input, amqp_offset(encoded.bytes, o));                    \
      return 1;                                                              \
    }                                                                        \
    return 0;                                                                \
  }                                                                          \
                                                                             \
  static inline int amqp_decode_##bits(amqp_bytes_t encoded, size_t *offset, \
                                       uint##bits##_t *output) {             \
    size_t o = *offset;                                                      \
    if ((*offset = o + bits / 8) <= encoded.len) {                           \
      *output = amqp_d##bits(amqp_offset(encoded.bytes, o));                 \
      return 1;                                                              \
    }                                                                        \
    return 0;                                                                \
  }

static inline int is_bigendian(void) {
  union {
    uint32_t i;
    char c[4];
  } bint = {0x01020304};
  return bint.c[0] == 1;
}

static inline void amqp_e8(uint8_t val, void *data) {
  memcpy(data, &val, sizeof(val));
}

static inline uint8_t amqp_d8(void *data) {
  uint8_t val;
  memcpy(&val, data, sizeof(val));
  return val;
}

static inline void amqp_e16(uint16_t val, void *data) {
  if (!is_bigendian()) {
    val = ((val & 0xFF00u) >> 8u) | ((val & 0x00FFu) << 8u);
  }
  memcpy(data, &val, sizeof(val));
}

static inline uint16_t amqp_d16(void *data) {
  uint16_t val;
  memcpy(&val, data, sizeof(val));
  if (!is_bigendian()) {
    val = ((val & 0xFF00u) >> 8u) | ((val & 0x00FFu) << 8u);
  }
  return val;
}

static inline void amqp_e32(uint32_t val, void *data) {
  if (!is_bigendian()) {
    val = ((val & 0xFF000000u) >> 24u) | ((val & 0x00FF0000u) >> 8u) |
          ((val & 0x0000FF00u) << 8u) | ((val & 0x000000FFu) << 24u);
  }
  memcpy(data, &val, sizeof(val));
}

static inline uint32_t amqp_d32(void *data) {
  uint32_t val;
  memcpy(&val, data, sizeof(val));
  if (!is_bigendian()) {
    val = ((val & 0xFF000000u) >> 24u) | ((val & 0x00FF0000u) >> 8u) |
          ((val & 0x0000FF00u) << 8u) | ((val & 0x000000FFu) << 24u);
  }
  return val;
}

static inline void amqp_e64(uint64_t val, void *data) {
  if (!is_bigendian()) {
    val = ((val & 0xFF00000000000000u) >> 56u) |
          ((val & 0x00FF000000000000u) >> 40u) |
          ((val & 0x0000FF0000000000u) >> 24u) |
          ((val & 0x000000FF00000000u) >> 8u) |
          ((val & 0x00000000FF000000u) << 8u) |
          ((val & 0x0000000000FF0000u) << 24u) |
          ((val & 0x000000000000FF00u) << 40u) |
          ((val & 0x00000000000000FFu) << 56u);
  }
  memcpy(data, &val, sizeof(val));
}

static inline uint64_t amqp_d64(void *data) {
  uint64_t val;
  memcpy(&val, data, sizeof(val));
  if (!is_bigendian()) {
    val = ((val & 0xFF00000000000000u) >> 56u) |
          ((val & 0x00FF000000000000u) >> 40u) |
          ((val & 0x0000FF0000000000u) >> 24u) |
          ((val & 0x000000FF00000000u) >> 8u) |
          ((val & 0x00000000FF000000u) << 8u) |
          ((val & 0x0000000000FF0000u) << 24u) |
          ((val & 0x000000000000FF00u) << 40u) |
          ((val & 0x00000000000000FFu) << 56u);
  }
  return val;
}

DECLARE_CODEC_BASE_TYPE(8)
DECLARE_CODEC_BASE_TYPE(16)
DECLARE_CODEC_BASE_TYPE(32)
DECLARE_CODEC_BASE_TYPE(64)

static inline int amqp_encode_bytes(amqp_bytes_t encoded, size_t *offset,
                                    amqp_bytes_t input) {
  size_t o = *offset;
  /* The memcpy below has undefined behavior if the input is NULL. It is valid
   * for a 0-length amqp_bytes_t to have .bytes == NULL. Thus we should check
   * before encoding.
   */
  if (input.len == 0) {
    return 1;
  }
  if ((*offset = o + input.len) <= encoded.len) {
    memcpy(amqp_offset(encoded.bytes, o), input.bytes, input.len);
    return 1;
  } else {
    return 0;
  }
}

static inline int amqp_decode_bytes(amqp_bytes_t encoded, size_t *offset,
                                    amqp_bytes_t *output, size_t len) {
  size_t o = *offset;
  if ((*offset = o + len) <= encoded.len) {
    output->bytes = amqp_offset(encoded.bytes, o);
    output->len = len;
    return 1;
  } else {
    return 0;
  }
}

AMQP_NORETURN
void amqp_abort(const char *fmt, ...);

int amqp_bytes_equal(amqp_bytes_t r, amqp_bytes_t l);

static inline amqp_rpc_reply_t amqp_rpc_reply_error(amqp_status_enum status) {
  amqp_rpc_reply_t reply;
  reply.reply_type = AMQP_RESPONSE_LIBRARY_EXCEPTION;
  reply.library_error = status;
  return reply;
}

int amqp_send_frame_inner(amqp_connection_state_t state,
                          const amqp_frame_t *frame, int flags,
                          amqp_time_t deadline);
#endif
