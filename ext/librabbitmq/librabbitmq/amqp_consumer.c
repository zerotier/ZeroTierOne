/*
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MIT
 *
 * Portions created by Alan Antonuk are Copyright (c) 2013-2014
 * Alan Antonuk. All Rights Reserved.
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
#include "amqp.h"
#include "amqp_private.h"
#include "amqp_socket.h"

#include <stdlib.h>
#include <string.h>

static int amqp_basic_properties_clone(amqp_basic_properties_t *original,
                                       amqp_basic_properties_t *clone,
                                       amqp_pool_t *pool) {
  memset(clone, 0, sizeof(*clone));
  clone->_flags = original->_flags;

#define CLONE_BYTES_POOL(original, clone, pool)        \
  if (0 == original.len) {                             \
    clone = amqp_empty_bytes;                          \
  } else {                                             \
    amqp_pool_alloc_bytes(pool, original.len, &clone); \
    if (NULL == clone.bytes) {                         \
      return AMQP_STATUS_NO_MEMORY;                    \
    }                                                  \
    memcpy(clone.bytes, original.bytes, clone.len);    \
  }

  if (clone->_flags & AMQP_BASIC_CONTENT_TYPE_FLAG) {
    CLONE_BYTES_POOL(original->content_type, clone->content_type, pool)
  }

  if (clone->_flags & AMQP_BASIC_CONTENT_ENCODING_FLAG) {
    CLONE_BYTES_POOL(original->content_encoding, clone->content_encoding, pool)
  }

  if (clone->_flags & AMQP_BASIC_HEADERS_FLAG) {
    int res = amqp_table_clone(&original->headers, &clone->headers, pool);
    if (AMQP_STATUS_OK != res) {
      return res;
    }
  }

  if (clone->_flags & AMQP_BASIC_DELIVERY_MODE_FLAG) {
    clone->delivery_mode = original->delivery_mode;
  }

  if (clone->_flags & AMQP_BASIC_PRIORITY_FLAG) {
    clone->priority = original->priority;
  }

  if (clone->_flags & AMQP_BASIC_CORRELATION_ID_FLAG) {
    CLONE_BYTES_POOL(original->correlation_id, clone->correlation_id, pool)
  }

  if (clone->_flags & AMQP_BASIC_REPLY_TO_FLAG) {
    CLONE_BYTES_POOL(original->reply_to, clone->reply_to, pool)
  }

  if (clone->_flags & AMQP_BASIC_EXPIRATION_FLAG) {
    CLONE_BYTES_POOL(original->expiration, clone->expiration, pool)
  }

  if (clone->_flags & AMQP_BASIC_MESSAGE_ID_FLAG) {
    CLONE_BYTES_POOL(original->message_id, clone->message_id, pool)
  }

  if (clone->_flags & AMQP_BASIC_TIMESTAMP_FLAG) {
    clone->timestamp = original->timestamp;
  }

  if (clone->_flags & AMQP_BASIC_TYPE_FLAG) {
    CLONE_BYTES_POOL(original->type, clone->type, pool)
  }

  if (clone->_flags & AMQP_BASIC_USER_ID_FLAG) {
    CLONE_BYTES_POOL(original->user_id, clone->user_id, pool)
  }

  if (clone->_flags & AMQP_BASIC_APP_ID_FLAG) {
    CLONE_BYTES_POOL(original->app_id, clone->app_id, pool)
  }

  if (clone->_flags & AMQP_BASIC_CLUSTER_ID_FLAG) {
    CLONE_BYTES_POOL(original->cluster_id, clone->cluster_id, pool)
  }

  return AMQP_STATUS_OK;
#undef CLONE_BYTES_POOL
}

void amqp_destroy_message(amqp_message_t *message) {
  empty_amqp_pool(&message->pool);
  amqp_bytes_free(message->body);
}

void amqp_destroy_envelope(amqp_envelope_t *envelope) {
  amqp_destroy_message(&envelope->message);
  amqp_bytes_free(envelope->routing_key);
  amqp_bytes_free(envelope->exchange);
  amqp_bytes_free(envelope->consumer_tag);
}

static int amqp_bytes_malloc_dup_failed(amqp_bytes_t bytes) {
  if (bytes.len != 0 && bytes.bytes == NULL) {
    return 1;
  }
  return 0;
}

amqp_rpc_reply_t amqp_consume_message(amqp_connection_state_t state,
                                      amqp_envelope_t *envelope,
                                      struct timeval *timeout,
                                      AMQP_UNUSED int flags) {
  int res;
  amqp_frame_t frame;
  amqp_basic_deliver_t *delivery_method;
  amqp_rpc_reply_t ret;

  memset(&ret, 0, sizeof(ret));
  memset(envelope, 0, sizeof(*envelope));

  res = amqp_simple_wait_frame_noblock(state, &frame, timeout);
  if (AMQP_STATUS_OK != res) {
    ret.reply_type = AMQP_RESPONSE_LIBRARY_EXCEPTION;
    ret.library_error = res;
    goto error_out1;
  }

  if (AMQP_FRAME_METHOD != frame.frame_type ||
      AMQP_BASIC_DELIVER_METHOD != frame.payload.method.id) {
    amqp_put_back_frame(state, &frame);
    ret.reply_type = AMQP_RESPONSE_LIBRARY_EXCEPTION;
    ret.library_error = AMQP_STATUS_UNEXPECTED_STATE;
    goto error_out1;
  }

  delivery_method = frame.payload.method.decoded;

  envelope->channel = frame.channel;
  envelope->consumer_tag = amqp_bytes_malloc_dup(delivery_method->consumer_tag);
  envelope->delivery_tag = delivery_method->delivery_tag;
  envelope->redelivered = delivery_method->redelivered;
  envelope->exchange = amqp_bytes_malloc_dup(delivery_method->exchange);
  envelope->routing_key = amqp_bytes_malloc_dup(delivery_method->routing_key);

  if (amqp_bytes_malloc_dup_failed(envelope->consumer_tag) ||
      amqp_bytes_malloc_dup_failed(envelope->exchange) ||
      amqp_bytes_malloc_dup_failed(envelope->routing_key)) {
    ret.reply_type = AMQP_RESPONSE_LIBRARY_EXCEPTION;
    ret.library_error = AMQP_STATUS_NO_MEMORY;
    goto error_out2;
  }

  ret = amqp_read_message(state, envelope->channel, &envelope->message, 0);
  if (AMQP_RESPONSE_NORMAL != ret.reply_type) {
    goto error_out2;
  }

  ret.reply_type = AMQP_RESPONSE_NORMAL;
  return ret;

error_out2:
  amqp_bytes_free(envelope->routing_key);
  amqp_bytes_free(envelope->exchange);
  amqp_bytes_free(envelope->consumer_tag);
error_out1:
  return ret;
}

amqp_rpc_reply_t amqp_read_message(amqp_connection_state_t state,
                                   amqp_channel_t channel,
                                   amqp_message_t *message,
                                   AMQP_UNUSED int flags) {
  amqp_frame_t frame;
  amqp_rpc_reply_t ret;

  size_t body_read;
  char *body_read_ptr;
  int res;

  memset(&ret, 0, sizeof(ret));
  memset(message, 0, sizeof(*message));

  res = amqp_simple_wait_frame_on_channel(state, channel, &frame);
  if (AMQP_STATUS_OK != res) {
    ret.reply_type = AMQP_RESPONSE_LIBRARY_EXCEPTION;
    ret.library_error = res;

    goto error_out1;
  }

  if (AMQP_FRAME_HEADER != frame.frame_type) {
    if (AMQP_FRAME_METHOD == frame.frame_type &&
        (AMQP_CHANNEL_CLOSE_METHOD == frame.payload.method.id ||
         AMQP_CONNECTION_CLOSE_METHOD == frame.payload.method.id)) {

      ret.reply_type = AMQP_RESPONSE_SERVER_EXCEPTION;
      ret.reply = frame.payload.method;

    } else {
      ret.reply_type = AMQP_RESPONSE_LIBRARY_EXCEPTION;
      ret.library_error = AMQP_STATUS_UNEXPECTED_STATE;

      amqp_put_back_frame(state, &frame);
    }
    goto error_out1;
  }

  init_amqp_pool(&message->pool, 4096);
  res = amqp_basic_properties_clone(frame.payload.properties.decoded,
                                    &message->properties, &message->pool);

  if (AMQP_STATUS_OK != res) {
    ret.reply_type = AMQP_RESPONSE_LIBRARY_EXCEPTION;
    ret.library_error = res;
    goto error_out3;
  }

  if (0 == frame.payload.properties.body_size) {
    message->body = amqp_empty_bytes;
  } else {
    if (SIZE_MAX < frame.payload.properties.body_size) {
      ret.reply_type = AMQP_RESPONSE_LIBRARY_EXCEPTION;
      ret.library_error = AMQP_STATUS_NO_MEMORY;
      goto error_out1;
    }
    message->body =
        amqp_bytes_malloc((size_t)frame.payload.properties.body_size);
    if (NULL == message->body.bytes) {
      ret.reply_type = AMQP_RESPONSE_LIBRARY_EXCEPTION;
      ret.library_error = AMQP_STATUS_NO_MEMORY;
      goto error_out1;
    }
  }

  body_read = 0;
  body_read_ptr = message->body.bytes;

  while (body_read < message->body.len) {
    res = amqp_simple_wait_frame_on_channel(state, channel, &frame);
    if (AMQP_STATUS_OK != res) {
      ret.reply_type = AMQP_RESPONSE_LIBRARY_EXCEPTION;
      ret.library_error = res;
      goto error_out2;
    }
    if (AMQP_FRAME_BODY != frame.frame_type) {
      if (AMQP_FRAME_METHOD == frame.frame_type &&
          (AMQP_CHANNEL_CLOSE_METHOD == frame.payload.method.id ||
           AMQP_CONNECTION_CLOSE_METHOD == frame.payload.method.id)) {

        ret.reply_type = AMQP_RESPONSE_SERVER_EXCEPTION;
        ret.reply = frame.payload.method;
      } else {
        ret.reply_type = AMQP_RESPONSE_LIBRARY_EXCEPTION;
        ret.library_error = AMQP_STATUS_BAD_AMQP_DATA;
      }
      goto error_out2;
    }

    if (body_read + frame.payload.body_fragment.len > message->body.len) {
      ret.reply_type = AMQP_RESPONSE_LIBRARY_EXCEPTION;
      ret.library_error = AMQP_STATUS_BAD_AMQP_DATA;
      goto error_out2;
    }

    memcpy(body_read_ptr, frame.payload.body_fragment.bytes,
           frame.payload.body_fragment.len);

    body_read += frame.payload.body_fragment.len;
    body_read_ptr += frame.payload.body_fragment.len;
  }

  ret.reply_type = AMQP_RESPONSE_NORMAL;
  return ret;

error_out2:
  amqp_bytes_free(message->body);
error_out3:
  empty_amqp_pool(&message->pool);
error_out1:
  return ret;
}
