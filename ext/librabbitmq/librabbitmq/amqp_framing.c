/* Generated code. Do not edit. Edit and re-run codegen.py instead.
 *
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

#include "amqp_private.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char const *amqp_constant_name(int constantNumber) {
  switch (constantNumber) {
    case AMQP_FRAME_METHOD:
      return "AMQP_FRAME_METHOD";
    case AMQP_FRAME_HEADER:
      return "AMQP_FRAME_HEADER";
    case AMQP_FRAME_BODY:
      return "AMQP_FRAME_BODY";
    case AMQP_FRAME_HEARTBEAT:
      return "AMQP_FRAME_HEARTBEAT";
    case AMQP_FRAME_MIN_SIZE:
      return "AMQP_FRAME_MIN_SIZE";
    case AMQP_FRAME_END:
      return "AMQP_FRAME_END";
    case AMQP_REPLY_SUCCESS:
      return "AMQP_REPLY_SUCCESS";
    case AMQP_CONTENT_TOO_LARGE:
      return "AMQP_CONTENT_TOO_LARGE";
    case AMQP_NO_ROUTE:
      return "AMQP_NO_ROUTE";
    case AMQP_NO_CONSUMERS:
      return "AMQP_NO_CONSUMERS";
    case AMQP_ACCESS_REFUSED:
      return "AMQP_ACCESS_REFUSED";
    case AMQP_NOT_FOUND:
      return "AMQP_NOT_FOUND";
    case AMQP_RESOURCE_LOCKED:
      return "AMQP_RESOURCE_LOCKED";
    case AMQP_PRECONDITION_FAILED:
      return "AMQP_PRECONDITION_FAILED";
    case AMQP_CONNECTION_FORCED:
      return "AMQP_CONNECTION_FORCED";
    case AMQP_INVALID_PATH:
      return "AMQP_INVALID_PATH";
    case AMQP_FRAME_ERROR:
      return "AMQP_FRAME_ERROR";
    case AMQP_SYNTAX_ERROR:
      return "AMQP_SYNTAX_ERROR";
    case AMQP_COMMAND_INVALID:
      return "AMQP_COMMAND_INVALID";
    case AMQP_CHANNEL_ERROR:
      return "AMQP_CHANNEL_ERROR";
    case AMQP_UNEXPECTED_FRAME:
      return "AMQP_UNEXPECTED_FRAME";
    case AMQP_RESOURCE_ERROR:
      return "AMQP_RESOURCE_ERROR";
    case AMQP_NOT_ALLOWED:
      return "AMQP_NOT_ALLOWED";
    case AMQP_NOT_IMPLEMENTED:
      return "AMQP_NOT_IMPLEMENTED";
    case AMQP_INTERNAL_ERROR:
      return "AMQP_INTERNAL_ERROR";
    default:
      return "(unknown)";
  }
}

amqp_boolean_t amqp_constant_is_hard_error(int constantNumber) {
  switch (constantNumber) {
    case AMQP_CONNECTION_FORCED:
      return 1;
    case AMQP_INVALID_PATH:
      return 1;
    case AMQP_FRAME_ERROR:
      return 1;
    case AMQP_SYNTAX_ERROR:
      return 1;
    case AMQP_COMMAND_INVALID:
      return 1;
    case AMQP_CHANNEL_ERROR:
      return 1;
    case AMQP_UNEXPECTED_FRAME:
      return 1;
    case AMQP_RESOURCE_ERROR:
      return 1;
    case AMQP_NOT_ALLOWED:
      return 1;
    case AMQP_NOT_IMPLEMENTED:
      return 1;
    case AMQP_INTERNAL_ERROR:
      return 1;
    default:
      return 0;
  }
}

char const *amqp_method_name(amqp_method_number_t methodNumber) {
  switch (methodNumber) {
    case AMQP_CONNECTION_START_METHOD:
      return "AMQP_CONNECTION_START_METHOD";
    case AMQP_CONNECTION_START_OK_METHOD:
      return "AMQP_CONNECTION_START_OK_METHOD";
    case AMQP_CONNECTION_SECURE_METHOD:
      return "AMQP_CONNECTION_SECURE_METHOD";
    case AMQP_CONNECTION_SECURE_OK_METHOD:
      return "AMQP_CONNECTION_SECURE_OK_METHOD";
    case AMQP_CONNECTION_TUNE_METHOD:
      return "AMQP_CONNECTION_TUNE_METHOD";
    case AMQP_CONNECTION_TUNE_OK_METHOD:
      return "AMQP_CONNECTION_TUNE_OK_METHOD";
    case AMQP_CONNECTION_OPEN_METHOD:
      return "AMQP_CONNECTION_OPEN_METHOD";
    case AMQP_CONNECTION_OPEN_OK_METHOD:
      return "AMQP_CONNECTION_OPEN_OK_METHOD";
    case AMQP_CONNECTION_CLOSE_METHOD:
      return "AMQP_CONNECTION_CLOSE_METHOD";
    case AMQP_CONNECTION_CLOSE_OK_METHOD:
      return "AMQP_CONNECTION_CLOSE_OK_METHOD";
    case AMQP_CONNECTION_BLOCKED_METHOD:
      return "AMQP_CONNECTION_BLOCKED_METHOD";
    case AMQP_CONNECTION_UNBLOCKED_METHOD:
      return "AMQP_CONNECTION_UNBLOCKED_METHOD";
    case AMQP_CHANNEL_OPEN_METHOD:
      return "AMQP_CHANNEL_OPEN_METHOD";
    case AMQP_CHANNEL_OPEN_OK_METHOD:
      return "AMQP_CHANNEL_OPEN_OK_METHOD";
    case AMQP_CHANNEL_FLOW_METHOD:
      return "AMQP_CHANNEL_FLOW_METHOD";
    case AMQP_CHANNEL_FLOW_OK_METHOD:
      return "AMQP_CHANNEL_FLOW_OK_METHOD";
    case AMQP_CHANNEL_CLOSE_METHOD:
      return "AMQP_CHANNEL_CLOSE_METHOD";
    case AMQP_CHANNEL_CLOSE_OK_METHOD:
      return "AMQP_CHANNEL_CLOSE_OK_METHOD";
    case AMQP_ACCESS_REQUEST_METHOD:
      return "AMQP_ACCESS_REQUEST_METHOD";
    case AMQP_ACCESS_REQUEST_OK_METHOD:
      return "AMQP_ACCESS_REQUEST_OK_METHOD";
    case AMQP_EXCHANGE_DECLARE_METHOD:
      return "AMQP_EXCHANGE_DECLARE_METHOD";
    case AMQP_EXCHANGE_DECLARE_OK_METHOD:
      return "AMQP_EXCHANGE_DECLARE_OK_METHOD";
    case AMQP_EXCHANGE_DELETE_METHOD:
      return "AMQP_EXCHANGE_DELETE_METHOD";
    case AMQP_EXCHANGE_DELETE_OK_METHOD:
      return "AMQP_EXCHANGE_DELETE_OK_METHOD";
    case AMQP_EXCHANGE_BIND_METHOD:
      return "AMQP_EXCHANGE_BIND_METHOD";
    case AMQP_EXCHANGE_BIND_OK_METHOD:
      return "AMQP_EXCHANGE_BIND_OK_METHOD";
    case AMQP_EXCHANGE_UNBIND_METHOD:
      return "AMQP_EXCHANGE_UNBIND_METHOD";
    case AMQP_EXCHANGE_UNBIND_OK_METHOD:
      return "AMQP_EXCHANGE_UNBIND_OK_METHOD";
    case AMQP_QUEUE_DECLARE_METHOD:
      return "AMQP_QUEUE_DECLARE_METHOD";
    case AMQP_QUEUE_DECLARE_OK_METHOD:
      return "AMQP_QUEUE_DECLARE_OK_METHOD";
    case AMQP_QUEUE_BIND_METHOD:
      return "AMQP_QUEUE_BIND_METHOD";
    case AMQP_QUEUE_BIND_OK_METHOD:
      return "AMQP_QUEUE_BIND_OK_METHOD";
    case AMQP_QUEUE_PURGE_METHOD:
      return "AMQP_QUEUE_PURGE_METHOD";
    case AMQP_QUEUE_PURGE_OK_METHOD:
      return "AMQP_QUEUE_PURGE_OK_METHOD";
    case AMQP_QUEUE_DELETE_METHOD:
      return "AMQP_QUEUE_DELETE_METHOD";
    case AMQP_QUEUE_DELETE_OK_METHOD:
      return "AMQP_QUEUE_DELETE_OK_METHOD";
    case AMQP_QUEUE_UNBIND_METHOD:
      return "AMQP_QUEUE_UNBIND_METHOD";
    case AMQP_QUEUE_UNBIND_OK_METHOD:
      return "AMQP_QUEUE_UNBIND_OK_METHOD";
    case AMQP_BASIC_QOS_METHOD:
      return "AMQP_BASIC_QOS_METHOD";
    case AMQP_BASIC_QOS_OK_METHOD:
      return "AMQP_BASIC_QOS_OK_METHOD";
    case AMQP_BASIC_CONSUME_METHOD:
      return "AMQP_BASIC_CONSUME_METHOD";
    case AMQP_BASIC_CONSUME_OK_METHOD:
      return "AMQP_BASIC_CONSUME_OK_METHOD";
    case AMQP_BASIC_CANCEL_METHOD:
      return "AMQP_BASIC_CANCEL_METHOD";
    case AMQP_BASIC_CANCEL_OK_METHOD:
      return "AMQP_BASIC_CANCEL_OK_METHOD";
    case AMQP_BASIC_PUBLISH_METHOD:
      return "AMQP_BASIC_PUBLISH_METHOD";
    case AMQP_BASIC_RETURN_METHOD:
      return "AMQP_BASIC_RETURN_METHOD";
    case AMQP_BASIC_DELIVER_METHOD:
      return "AMQP_BASIC_DELIVER_METHOD";
    case AMQP_BASIC_GET_METHOD:
      return "AMQP_BASIC_GET_METHOD";
    case AMQP_BASIC_GET_OK_METHOD:
      return "AMQP_BASIC_GET_OK_METHOD";
    case AMQP_BASIC_GET_EMPTY_METHOD:
      return "AMQP_BASIC_GET_EMPTY_METHOD";
    case AMQP_BASIC_ACK_METHOD:
      return "AMQP_BASIC_ACK_METHOD";
    case AMQP_BASIC_REJECT_METHOD:
      return "AMQP_BASIC_REJECT_METHOD";
    case AMQP_BASIC_RECOVER_ASYNC_METHOD:
      return "AMQP_BASIC_RECOVER_ASYNC_METHOD";
    case AMQP_BASIC_RECOVER_METHOD:
      return "AMQP_BASIC_RECOVER_METHOD";
    case AMQP_BASIC_RECOVER_OK_METHOD:
      return "AMQP_BASIC_RECOVER_OK_METHOD";
    case AMQP_BASIC_NACK_METHOD:
      return "AMQP_BASIC_NACK_METHOD";
    case AMQP_TX_SELECT_METHOD:
      return "AMQP_TX_SELECT_METHOD";
    case AMQP_TX_SELECT_OK_METHOD:
      return "AMQP_TX_SELECT_OK_METHOD";
    case AMQP_TX_COMMIT_METHOD:
      return "AMQP_TX_COMMIT_METHOD";
    case AMQP_TX_COMMIT_OK_METHOD:
      return "AMQP_TX_COMMIT_OK_METHOD";
    case AMQP_TX_ROLLBACK_METHOD:
      return "AMQP_TX_ROLLBACK_METHOD";
    case AMQP_TX_ROLLBACK_OK_METHOD:
      return "AMQP_TX_ROLLBACK_OK_METHOD";
    case AMQP_CONFIRM_SELECT_METHOD:
      return "AMQP_CONFIRM_SELECT_METHOD";
    case AMQP_CONFIRM_SELECT_OK_METHOD:
      return "AMQP_CONFIRM_SELECT_OK_METHOD";
    default:
      return NULL;
  }
}

amqp_boolean_t amqp_method_has_content(amqp_method_number_t methodNumber) {
  switch (methodNumber) {
    case AMQP_BASIC_PUBLISH_METHOD:
      return 1;
    case AMQP_BASIC_RETURN_METHOD:
      return 1;
    case AMQP_BASIC_DELIVER_METHOD:
      return 1;
    case AMQP_BASIC_GET_OK_METHOD:
      return 1;
    default:
      return 0;
  }
}

int amqp_decode_method(amqp_method_number_t methodNumber, amqp_pool_t *pool,
                       amqp_bytes_t encoded, void **decoded) {
  size_t offset = 0;
  uint8_t bit_buffer;

  switch (methodNumber) {
    case AMQP_CONNECTION_START_METHOD: {
      amqp_connection_start_t *m = (amqp_connection_start_t *)amqp_pool_alloc(
          pool, sizeof(amqp_connection_start_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_8(encoded, &offset, &m->version_major))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (!amqp_decode_8(encoded, &offset, &m->version_minor))
        return AMQP_STATUS_BAD_AMQP_DATA;
      {
        int res =
            amqp_decode_table(encoded, pool, &(m->server_properties), &offset);
        if (res < 0) return res;
      }
      {
        uint32_t len;
        if (!amqp_decode_32(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->mechanisms, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      {
        uint32_t len;
        if (!amqp_decode_32(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->locales, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_CONNECTION_START_OK_METHOD: {
      amqp_connection_start_ok_t *m =
          (amqp_connection_start_ok_t *)amqp_pool_alloc(
              pool, sizeof(amqp_connection_start_ok_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      {
        int res =
            amqp_decode_table(encoded, pool, &(m->client_properties), &offset);
        if (res < 0) return res;
      }
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->mechanism, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      {
        uint32_t len;
        if (!amqp_decode_32(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->response, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->locale, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_CONNECTION_SECURE_METHOD: {
      amqp_connection_secure_t *m = (amqp_connection_secure_t *)amqp_pool_alloc(
          pool, sizeof(amqp_connection_secure_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      {
        uint32_t len;
        if (!amqp_decode_32(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->challenge, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_CONNECTION_SECURE_OK_METHOD: {
      amqp_connection_secure_ok_t *m =
          (amqp_connection_secure_ok_t *)amqp_pool_alloc(
              pool, sizeof(amqp_connection_secure_ok_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      {
        uint32_t len;
        if (!amqp_decode_32(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->response, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_CONNECTION_TUNE_METHOD: {
      amqp_connection_tune_t *m = (amqp_connection_tune_t *)amqp_pool_alloc(
          pool, sizeof(amqp_connection_tune_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_16(encoded, &offset, &m->channel_max))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (!amqp_decode_32(encoded, &offset, &m->frame_max))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (!amqp_decode_16(encoded, &offset, &m->heartbeat))
        return AMQP_STATUS_BAD_AMQP_DATA;
      *decoded = m;
      return 0;
    }
    case AMQP_CONNECTION_TUNE_OK_METHOD: {
      amqp_connection_tune_ok_t *m =
          (amqp_connection_tune_ok_t *)amqp_pool_alloc(
              pool, sizeof(amqp_connection_tune_ok_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_16(encoded, &offset, &m->channel_max))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (!amqp_decode_32(encoded, &offset, &m->frame_max))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (!amqp_decode_16(encoded, &offset, &m->heartbeat))
        return AMQP_STATUS_BAD_AMQP_DATA;
      *decoded = m;
      return 0;
    }
    case AMQP_CONNECTION_OPEN_METHOD: {
      amqp_connection_open_t *m = (amqp_connection_open_t *)amqp_pool_alloc(
          pool, sizeof(amqp_connection_open_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->virtual_host, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->capabilities, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (!amqp_decode_8(encoded, &offset, &bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      m->insist = (bit_buffer & (1 << 0)) ? 1 : 0;
      *decoded = m;
      return 0;
    }
    case AMQP_CONNECTION_OPEN_OK_METHOD: {
      amqp_connection_open_ok_t *m =
          (amqp_connection_open_ok_t *)amqp_pool_alloc(
              pool, sizeof(amqp_connection_open_ok_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->known_hosts, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_CONNECTION_CLOSE_METHOD: {
      amqp_connection_close_t *m = (amqp_connection_close_t *)amqp_pool_alloc(
          pool, sizeof(amqp_connection_close_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_16(encoded, &offset, &m->reply_code))
        return AMQP_STATUS_BAD_AMQP_DATA;
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->reply_text, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (!amqp_decode_16(encoded, &offset, &m->class_id))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (!amqp_decode_16(encoded, &offset, &m->method_id))
        return AMQP_STATUS_BAD_AMQP_DATA;
      *decoded = m;
      return 0;
    }
    case AMQP_CONNECTION_CLOSE_OK_METHOD: {
      amqp_connection_close_ok_t *m =
          (amqp_connection_close_ok_t *)amqp_pool_alloc(
              pool, sizeof(amqp_connection_close_ok_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_CONNECTION_BLOCKED_METHOD: {
      amqp_connection_blocked_t *m =
          (amqp_connection_blocked_t *)amqp_pool_alloc(
              pool, sizeof(amqp_connection_blocked_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->reason, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_CONNECTION_UNBLOCKED_METHOD: {
      amqp_connection_unblocked_t *m =
          (amqp_connection_unblocked_t *)amqp_pool_alloc(
              pool, sizeof(amqp_connection_unblocked_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_CHANNEL_OPEN_METHOD: {
      amqp_channel_open_t *m = (amqp_channel_open_t *)amqp_pool_alloc(
          pool, sizeof(amqp_channel_open_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->out_of_band, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_CHANNEL_OPEN_OK_METHOD: {
      amqp_channel_open_ok_t *m = (amqp_channel_open_ok_t *)amqp_pool_alloc(
          pool, sizeof(amqp_channel_open_ok_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      {
        uint32_t len;
        if (!amqp_decode_32(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->channel_id, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_CHANNEL_FLOW_METHOD: {
      amqp_channel_flow_t *m = (amqp_channel_flow_t *)amqp_pool_alloc(
          pool, sizeof(amqp_channel_flow_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_8(encoded, &offset, &bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      m->active = (bit_buffer & (1 << 0)) ? 1 : 0;
      *decoded = m;
      return 0;
    }
    case AMQP_CHANNEL_FLOW_OK_METHOD: {
      amqp_channel_flow_ok_t *m = (amqp_channel_flow_ok_t *)amqp_pool_alloc(
          pool, sizeof(amqp_channel_flow_ok_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_8(encoded, &offset, &bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      m->active = (bit_buffer & (1 << 0)) ? 1 : 0;
      *decoded = m;
      return 0;
    }
    case AMQP_CHANNEL_CLOSE_METHOD: {
      amqp_channel_close_t *m = (amqp_channel_close_t *)amqp_pool_alloc(
          pool, sizeof(amqp_channel_close_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_16(encoded, &offset, &m->reply_code))
        return AMQP_STATUS_BAD_AMQP_DATA;
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->reply_text, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (!amqp_decode_16(encoded, &offset, &m->class_id))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (!amqp_decode_16(encoded, &offset, &m->method_id))
        return AMQP_STATUS_BAD_AMQP_DATA;
      *decoded = m;
      return 0;
    }
    case AMQP_CHANNEL_CLOSE_OK_METHOD: {
      amqp_channel_close_ok_t *m = (amqp_channel_close_ok_t *)amqp_pool_alloc(
          pool, sizeof(amqp_channel_close_ok_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_ACCESS_REQUEST_METHOD: {
      amqp_access_request_t *m = (amqp_access_request_t *)amqp_pool_alloc(
          pool, sizeof(amqp_access_request_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->realm, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (!amqp_decode_8(encoded, &offset, &bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      m->exclusive = (bit_buffer & (1 << 0)) ? 1 : 0;
      m->passive = (bit_buffer & (1 << 1)) ? 1 : 0;
      m->active = (bit_buffer & (1 << 2)) ? 1 : 0;
      m->write = (bit_buffer & (1 << 3)) ? 1 : 0;
      m->read = (bit_buffer & (1 << 4)) ? 1 : 0;
      *decoded = m;
      return 0;
    }
    case AMQP_ACCESS_REQUEST_OK_METHOD: {
      amqp_access_request_ok_t *m = (amqp_access_request_ok_t *)amqp_pool_alloc(
          pool, sizeof(amqp_access_request_ok_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_16(encoded, &offset, &m->ticket))
        return AMQP_STATUS_BAD_AMQP_DATA;
      *decoded = m;
      return 0;
    }
    case AMQP_EXCHANGE_DECLARE_METHOD: {
      amqp_exchange_declare_t *m = (amqp_exchange_declare_t *)amqp_pool_alloc(
          pool, sizeof(amqp_exchange_declare_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_16(encoded, &offset, &m->ticket))
        return AMQP_STATUS_BAD_AMQP_DATA;
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->exchange, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->type, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (!amqp_decode_8(encoded, &offset, &bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      m->passive = (bit_buffer & (1 << 0)) ? 1 : 0;
      m->durable = (bit_buffer & (1 << 1)) ? 1 : 0;
      m->auto_delete = (bit_buffer & (1 << 2)) ? 1 : 0;
      m->internal = (bit_buffer & (1 << 3)) ? 1 : 0;
      m->nowait = (bit_buffer & (1 << 4)) ? 1 : 0;
      {
        int res = amqp_decode_table(encoded, pool, &(m->arguments), &offset);
        if (res < 0) return res;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_EXCHANGE_DECLARE_OK_METHOD: {
      amqp_exchange_declare_ok_t *m =
          (amqp_exchange_declare_ok_t *)amqp_pool_alloc(
              pool, sizeof(amqp_exchange_declare_ok_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_EXCHANGE_DELETE_METHOD: {
      amqp_exchange_delete_t *m = (amqp_exchange_delete_t *)amqp_pool_alloc(
          pool, sizeof(amqp_exchange_delete_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_16(encoded, &offset, &m->ticket))
        return AMQP_STATUS_BAD_AMQP_DATA;
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->exchange, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (!amqp_decode_8(encoded, &offset, &bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      m->if_unused = (bit_buffer & (1 << 0)) ? 1 : 0;
      m->nowait = (bit_buffer & (1 << 1)) ? 1 : 0;
      *decoded = m;
      return 0;
    }
    case AMQP_EXCHANGE_DELETE_OK_METHOD: {
      amqp_exchange_delete_ok_t *m =
          (amqp_exchange_delete_ok_t *)amqp_pool_alloc(
              pool, sizeof(amqp_exchange_delete_ok_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_EXCHANGE_BIND_METHOD: {
      amqp_exchange_bind_t *m = (amqp_exchange_bind_t *)amqp_pool_alloc(
          pool, sizeof(amqp_exchange_bind_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_16(encoded, &offset, &m->ticket))
        return AMQP_STATUS_BAD_AMQP_DATA;
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->destination, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->source, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->routing_key, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (!amqp_decode_8(encoded, &offset, &bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      m->nowait = (bit_buffer & (1 << 0)) ? 1 : 0;
      {
        int res = amqp_decode_table(encoded, pool, &(m->arguments), &offset);
        if (res < 0) return res;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_EXCHANGE_BIND_OK_METHOD: {
      amqp_exchange_bind_ok_t *m = (amqp_exchange_bind_ok_t *)amqp_pool_alloc(
          pool, sizeof(amqp_exchange_bind_ok_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_EXCHANGE_UNBIND_METHOD: {
      amqp_exchange_unbind_t *m = (amqp_exchange_unbind_t *)amqp_pool_alloc(
          pool, sizeof(amqp_exchange_unbind_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_16(encoded, &offset, &m->ticket))
        return AMQP_STATUS_BAD_AMQP_DATA;
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->destination, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->source, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->routing_key, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (!amqp_decode_8(encoded, &offset, &bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      m->nowait = (bit_buffer & (1 << 0)) ? 1 : 0;
      {
        int res = amqp_decode_table(encoded, pool, &(m->arguments), &offset);
        if (res < 0) return res;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_EXCHANGE_UNBIND_OK_METHOD: {
      amqp_exchange_unbind_ok_t *m =
          (amqp_exchange_unbind_ok_t *)amqp_pool_alloc(
              pool, sizeof(amqp_exchange_unbind_ok_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_QUEUE_DECLARE_METHOD: {
      amqp_queue_declare_t *m = (amqp_queue_declare_t *)amqp_pool_alloc(
          pool, sizeof(amqp_queue_declare_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_16(encoded, &offset, &m->ticket))
        return AMQP_STATUS_BAD_AMQP_DATA;
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->queue, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (!amqp_decode_8(encoded, &offset, &bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      m->passive = (bit_buffer & (1 << 0)) ? 1 : 0;
      m->durable = (bit_buffer & (1 << 1)) ? 1 : 0;
      m->exclusive = (bit_buffer & (1 << 2)) ? 1 : 0;
      m->auto_delete = (bit_buffer & (1 << 3)) ? 1 : 0;
      m->nowait = (bit_buffer & (1 << 4)) ? 1 : 0;
      {
        int res = amqp_decode_table(encoded, pool, &(m->arguments), &offset);
        if (res < 0) return res;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_QUEUE_DECLARE_OK_METHOD: {
      amqp_queue_declare_ok_t *m = (amqp_queue_declare_ok_t *)amqp_pool_alloc(
          pool, sizeof(amqp_queue_declare_ok_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->queue, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (!amqp_decode_32(encoded, &offset, &m->message_count))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (!amqp_decode_32(encoded, &offset, &m->consumer_count))
        return AMQP_STATUS_BAD_AMQP_DATA;
      *decoded = m;
      return 0;
    }
    case AMQP_QUEUE_BIND_METHOD: {
      amqp_queue_bind_t *m =
          (amqp_queue_bind_t *)amqp_pool_alloc(pool, sizeof(amqp_queue_bind_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_16(encoded, &offset, &m->ticket))
        return AMQP_STATUS_BAD_AMQP_DATA;
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->queue, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->exchange, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->routing_key, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (!amqp_decode_8(encoded, &offset, &bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      m->nowait = (bit_buffer & (1 << 0)) ? 1 : 0;
      {
        int res = amqp_decode_table(encoded, pool, &(m->arguments), &offset);
        if (res < 0) return res;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_QUEUE_BIND_OK_METHOD: {
      amqp_queue_bind_ok_t *m = (amqp_queue_bind_ok_t *)amqp_pool_alloc(
          pool, sizeof(amqp_queue_bind_ok_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_QUEUE_PURGE_METHOD: {
      amqp_queue_purge_t *m = (amqp_queue_purge_t *)amqp_pool_alloc(
          pool, sizeof(amqp_queue_purge_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_16(encoded, &offset, &m->ticket))
        return AMQP_STATUS_BAD_AMQP_DATA;
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->queue, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (!amqp_decode_8(encoded, &offset, &bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      m->nowait = (bit_buffer & (1 << 0)) ? 1 : 0;
      *decoded = m;
      return 0;
    }
    case AMQP_QUEUE_PURGE_OK_METHOD: {
      amqp_queue_purge_ok_t *m = (amqp_queue_purge_ok_t *)amqp_pool_alloc(
          pool, sizeof(amqp_queue_purge_ok_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_32(encoded, &offset, &m->message_count))
        return AMQP_STATUS_BAD_AMQP_DATA;
      *decoded = m;
      return 0;
    }
    case AMQP_QUEUE_DELETE_METHOD: {
      amqp_queue_delete_t *m = (amqp_queue_delete_t *)amqp_pool_alloc(
          pool, sizeof(amqp_queue_delete_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_16(encoded, &offset, &m->ticket))
        return AMQP_STATUS_BAD_AMQP_DATA;
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->queue, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (!amqp_decode_8(encoded, &offset, &bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      m->if_unused = (bit_buffer & (1 << 0)) ? 1 : 0;
      m->if_empty = (bit_buffer & (1 << 1)) ? 1 : 0;
      m->nowait = (bit_buffer & (1 << 2)) ? 1 : 0;
      *decoded = m;
      return 0;
    }
    case AMQP_QUEUE_DELETE_OK_METHOD: {
      amqp_queue_delete_ok_t *m = (amqp_queue_delete_ok_t *)amqp_pool_alloc(
          pool, sizeof(amqp_queue_delete_ok_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_32(encoded, &offset, &m->message_count))
        return AMQP_STATUS_BAD_AMQP_DATA;
      *decoded = m;
      return 0;
    }
    case AMQP_QUEUE_UNBIND_METHOD: {
      amqp_queue_unbind_t *m = (amqp_queue_unbind_t *)amqp_pool_alloc(
          pool, sizeof(amqp_queue_unbind_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_16(encoded, &offset, &m->ticket))
        return AMQP_STATUS_BAD_AMQP_DATA;
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->queue, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->exchange, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->routing_key, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      {
        int res = amqp_decode_table(encoded, pool, &(m->arguments), &offset);
        if (res < 0) return res;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_QUEUE_UNBIND_OK_METHOD: {
      amqp_queue_unbind_ok_t *m = (amqp_queue_unbind_ok_t *)amqp_pool_alloc(
          pool, sizeof(amqp_queue_unbind_ok_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_BASIC_QOS_METHOD: {
      amqp_basic_qos_t *m =
          (amqp_basic_qos_t *)amqp_pool_alloc(pool, sizeof(amqp_basic_qos_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_32(encoded, &offset, &m->prefetch_size))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (!amqp_decode_16(encoded, &offset, &m->prefetch_count))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (!amqp_decode_8(encoded, &offset, &bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      m->global = (bit_buffer & (1 << 0)) ? 1 : 0;
      *decoded = m;
      return 0;
    }
    case AMQP_BASIC_QOS_OK_METHOD: {
      amqp_basic_qos_ok_t *m = (amqp_basic_qos_ok_t *)amqp_pool_alloc(
          pool, sizeof(amqp_basic_qos_ok_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_BASIC_CONSUME_METHOD: {
      amqp_basic_consume_t *m = (amqp_basic_consume_t *)amqp_pool_alloc(
          pool, sizeof(amqp_basic_consume_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_16(encoded, &offset, &m->ticket))
        return AMQP_STATUS_BAD_AMQP_DATA;
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->queue, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->consumer_tag, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (!amqp_decode_8(encoded, &offset, &bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      m->no_local = (bit_buffer & (1 << 0)) ? 1 : 0;
      m->no_ack = (bit_buffer & (1 << 1)) ? 1 : 0;
      m->exclusive = (bit_buffer & (1 << 2)) ? 1 : 0;
      m->nowait = (bit_buffer & (1 << 3)) ? 1 : 0;
      {
        int res = amqp_decode_table(encoded, pool, &(m->arguments), &offset);
        if (res < 0) return res;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_BASIC_CONSUME_OK_METHOD: {
      amqp_basic_consume_ok_t *m = (amqp_basic_consume_ok_t *)amqp_pool_alloc(
          pool, sizeof(amqp_basic_consume_ok_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->consumer_tag, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_BASIC_CANCEL_METHOD: {
      amqp_basic_cancel_t *m = (amqp_basic_cancel_t *)amqp_pool_alloc(
          pool, sizeof(amqp_basic_cancel_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->consumer_tag, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (!amqp_decode_8(encoded, &offset, &bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      m->nowait = (bit_buffer & (1 << 0)) ? 1 : 0;
      *decoded = m;
      return 0;
    }
    case AMQP_BASIC_CANCEL_OK_METHOD: {
      amqp_basic_cancel_ok_t *m = (amqp_basic_cancel_ok_t *)amqp_pool_alloc(
          pool, sizeof(amqp_basic_cancel_ok_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->consumer_tag, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_BASIC_PUBLISH_METHOD: {
      amqp_basic_publish_t *m = (amqp_basic_publish_t *)amqp_pool_alloc(
          pool, sizeof(amqp_basic_publish_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_16(encoded, &offset, &m->ticket))
        return AMQP_STATUS_BAD_AMQP_DATA;
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->exchange, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->routing_key, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (!amqp_decode_8(encoded, &offset, &bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      m->mandatory = (bit_buffer & (1 << 0)) ? 1 : 0;
      m->immediate = (bit_buffer & (1 << 1)) ? 1 : 0;
      *decoded = m;
      return 0;
    }
    case AMQP_BASIC_RETURN_METHOD: {
      amqp_basic_return_t *m = (amqp_basic_return_t *)amqp_pool_alloc(
          pool, sizeof(amqp_basic_return_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_16(encoded, &offset, &m->reply_code))
        return AMQP_STATUS_BAD_AMQP_DATA;
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->reply_text, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->exchange, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->routing_key, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_BASIC_DELIVER_METHOD: {
      amqp_basic_deliver_t *m = (amqp_basic_deliver_t *)amqp_pool_alloc(
          pool, sizeof(amqp_basic_deliver_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->consumer_tag, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (!amqp_decode_64(encoded, &offset, &m->delivery_tag))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (!amqp_decode_8(encoded, &offset, &bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      m->redelivered = (bit_buffer & (1 << 0)) ? 1 : 0;
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->exchange, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->routing_key, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_BASIC_GET_METHOD: {
      amqp_basic_get_t *m =
          (amqp_basic_get_t *)amqp_pool_alloc(pool, sizeof(amqp_basic_get_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_16(encoded, &offset, &m->ticket))
        return AMQP_STATUS_BAD_AMQP_DATA;
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->queue, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (!amqp_decode_8(encoded, &offset, &bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      m->no_ack = (bit_buffer & (1 << 0)) ? 1 : 0;
      *decoded = m;
      return 0;
    }
    case AMQP_BASIC_GET_OK_METHOD: {
      amqp_basic_get_ok_t *m = (amqp_basic_get_ok_t *)amqp_pool_alloc(
          pool, sizeof(amqp_basic_get_ok_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_64(encoded, &offset, &m->delivery_tag))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (!amqp_decode_8(encoded, &offset, &bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      m->redelivered = (bit_buffer & (1 << 0)) ? 1 : 0;
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->exchange, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->routing_key, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (!amqp_decode_32(encoded, &offset, &m->message_count))
        return AMQP_STATUS_BAD_AMQP_DATA;
      *decoded = m;
      return 0;
    }
    case AMQP_BASIC_GET_EMPTY_METHOD: {
      amqp_basic_get_empty_t *m = (amqp_basic_get_empty_t *)amqp_pool_alloc(
          pool, sizeof(amqp_basic_get_empty_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      {
        uint8_t len;
        if (!amqp_decode_8(encoded, &offset, &len) ||
            !amqp_decode_bytes(encoded, &offset, &m->cluster_id, len))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_BASIC_ACK_METHOD: {
      amqp_basic_ack_t *m =
          (amqp_basic_ack_t *)amqp_pool_alloc(pool, sizeof(amqp_basic_ack_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_64(encoded, &offset, &m->delivery_tag))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (!amqp_decode_8(encoded, &offset, &bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      m->multiple = (bit_buffer & (1 << 0)) ? 1 : 0;
      *decoded = m;
      return 0;
    }
    case AMQP_BASIC_REJECT_METHOD: {
      amqp_basic_reject_t *m = (amqp_basic_reject_t *)amqp_pool_alloc(
          pool, sizeof(amqp_basic_reject_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_64(encoded, &offset, &m->delivery_tag))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (!amqp_decode_8(encoded, &offset, &bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      m->requeue = (bit_buffer & (1 << 0)) ? 1 : 0;
      *decoded = m;
      return 0;
    }
    case AMQP_BASIC_RECOVER_ASYNC_METHOD: {
      amqp_basic_recover_async_t *m =
          (amqp_basic_recover_async_t *)amqp_pool_alloc(
              pool, sizeof(amqp_basic_recover_async_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_8(encoded, &offset, &bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      m->requeue = (bit_buffer & (1 << 0)) ? 1 : 0;
      *decoded = m;
      return 0;
    }
    case AMQP_BASIC_RECOVER_METHOD: {
      amqp_basic_recover_t *m = (amqp_basic_recover_t *)amqp_pool_alloc(
          pool, sizeof(amqp_basic_recover_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_8(encoded, &offset, &bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      m->requeue = (bit_buffer & (1 << 0)) ? 1 : 0;
      *decoded = m;
      return 0;
    }
    case AMQP_BASIC_RECOVER_OK_METHOD: {
      amqp_basic_recover_ok_t *m = (amqp_basic_recover_ok_t *)amqp_pool_alloc(
          pool, sizeof(amqp_basic_recover_ok_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_BASIC_NACK_METHOD: {
      amqp_basic_nack_t *m =
          (amqp_basic_nack_t *)amqp_pool_alloc(pool, sizeof(amqp_basic_nack_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_64(encoded, &offset, &m->delivery_tag))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (!amqp_decode_8(encoded, &offset, &bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      m->multiple = (bit_buffer & (1 << 0)) ? 1 : 0;
      m->requeue = (bit_buffer & (1 << 1)) ? 1 : 0;
      *decoded = m;
      return 0;
    }
    case AMQP_TX_SELECT_METHOD: {
      amqp_tx_select_t *m =
          (amqp_tx_select_t *)amqp_pool_alloc(pool, sizeof(amqp_tx_select_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_TX_SELECT_OK_METHOD: {
      amqp_tx_select_ok_t *m = (amqp_tx_select_ok_t *)amqp_pool_alloc(
          pool, sizeof(amqp_tx_select_ok_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_TX_COMMIT_METHOD: {
      amqp_tx_commit_t *m =
          (amqp_tx_commit_t *)amqp_pool_alloc(pool, sizeof(amqp_tx_commit_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_TX_COMMIT_OK_METHOD: {
      amqp_tx_commit_ok_t *m = (amqp_tx_commit_ok_t *)amqp_pool_alloc(
          pool, sizeof(amqp_tx_commit_ok_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_TX_ROLLBACK_METHOD: {
      amqp_tx_rollback_t *m = (amqp_tx_rollback_t *)amqp_pool_alloc(
          pool, sizeof(amqp_tx_rollback_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_TX_ROLLBACK_OK_METHOD: {
      amqp_tx_rollback_ok_t *m = (amqp_tx_rollback_ok_t *)amqp_pool_alloc(
          pool, sizeof(amqp_tx_rollback_ok_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      *decoded = m;
      return 0;
    }
    case AMQP_CONFIRM_SELECT_METHOD: {
      amqp_confirm_select_t *m = (amqp_confirm_select_t *)amqp_pool_alloc(
          pool, sizeof(amqp_confirm_select_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      if (!amqp_decode_8(encoded, &offset, &bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      m->nowait = (bit_buffer & (1 << 0)) ? 1 : 0;
      *decoded = m;
      return 0;
    }
    case AMQP_CONFIRM_SELECT_OK_METHOD: {
      amqp_confirm_select_ok_t *m = (amqp_confirm_select_ok_t *)amqp_pool_alloc(
          pool, sizeof(amqp_confirm_select_ok_t));
      if (m == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      *decoded = m;
      return 0;
    }
    default:
      return AMQP_STATUS_UNKNOWN_METHOD;
  }
}

int amqp_decode_properties(uint16_t class_id, amqp_pool_t *pool,
                           amqp_bytes_t encoded, void **decoded) {
  size_t offset = 0;

  amqp_flags_t flags = 0;
  int flagword_index = 0;
  uint16_t partial_flags;

  do {
    if (!amqp_decode_16(encoded, &offset, &partial_flags))
      return AMQP_STATUS_BAD_AMQP_DATA;
    flags |= (partial_flags << (flagword_index * 16));
    flagword_index++;
  } while (partial_flags & 1);

  switch (class_id) {
    case 10: {
      amqp_connection_properties_t *p =
          (amqp_connection_properties_t *)amqp_pool_alloc(
              pool, sizeof(amqp_connection_properties_t));
      if (p == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      p->_flags = flags;
      *decoded = p;
      return 0;
    }
    case 20: {
      amqp_channel_properties_t *p =
          (amqp_channel_properties_t *)amqp_pool_alloc(
              pool, sizeof(amqp_channel_properties_t));
      if (p == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      p->_flags = flags;
      *decoded = p;
      return 0;
    }
    case 30: {
      amqp_access_properties_t *p = (amqp_access_properties_t *)amqp_pool_alloc(
          pool, sizeof(amqp_access_properties_t));
      if (p == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      p->_flags = flags;
      *decoded = p;
      return 0;
    }
    case 40: {
      amqp_exchange_properties_t *p =
          (amqp_exchange_properties_t *)amqp_pool_alloc(
              pool, sizeof(amqp_exchange_properties_t));
      if (p == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      p->_flags = flags;
      *decoded = p;
      return 0;
    }
    case 50: {
      amqp_queue_properties_t *p = (amqp_queue_properties_t *)amqp_pool_alloc(
          pool, sizeof(amqp_queue_properties_t));
      if (p == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      p->_flags = flags;
      *decoded = p;
      return 0;
    }
    case 60: {
      amqp_basic_properties_t *p = (amqp_basic_properties_t *)amqp_pool_alloc(
          pool, sizeof(amqp_basic_properties_t));
      if (p == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      p->_flags = flags;
      if (flags & AMQP_BASIC_CONTENT_TYPE_FLAG) {
        {
          uint8_t len;
          if (!amqp_decode_8(encoded, &offset, &len) ||
              !amqp_decode_bytes(encoded, &offset, &p->content_type, len))
            return AMQP_STATUS_BAD_AMQP_DATA;
        }
      }
      if (flags & AMQP_BASIC_CONTENT_ENCODING_FLAG) {
        {
          uint8_t len;
          if (!amqp_decode_8(encoded, &offset, &len) ||
              !amqp_decode_bytes(encoded, &offset, &p->content_encoding, len))
            return AMQP_STATUS_BAD_AMQP_DATA;
        }
      }
      if (flags & AMQP_BASIC_HEADERS_FLAG) {
        {
          int res = amqp_decode_table(encoded, pool, &(p->headers), &offset);
          if (res < 0) return res;
        }
      }
      if (flags & AMQP_BASIC_DELIVERY_MODE_FLAG) {
        if (!amqp_decode_8(encoded, &offset, &p->delivery_mode))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (flags & AMQP_BASIC_PRIORITY_FLAG) {
        if (!amqp_decode_8(encoded, &offset, &p->priority))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (flags & AMQP_BASIC_CORRELATION_ID_FLAG) {
        {
          uint8_t len;
          if (!amqp_decode_8(encoded, &offset, &len) ||
              !amqp_decode_bytes(encoded, &offset, &p->correlation_id, len))
            return AMQP_STATUS_BAD_AMQP_DATA;
        }
      }
      if (flags & AMQP_BASIC_REPLY_TO_FLAG) {
        {
          uint8_t len;
          if (!amqp_decode_8(encoded, &offset, &len) ||
              !amqp_decode_bytes(encoded, &offset, &p->reply_to, len))
            return AMQP_STATUS_BAD_AMQP_DATA;
        }
      }
      if (flags & AMQP_BASIC_EXPIRATION_FLAG) {
        {
          uint8_t len;
          if (!amqp_decode_8(encoded, &offset, &len) ||
              !amqp_decode_bytes(encoded, &offset, &p->expiration, len))
            return AMQP_STATUS_BAD_AMQP_DATA;
        }
      }
      if (flags & AMQP_BASIC_MESSAGE_ID_FLAG) {
        {
          uint8_t len;
          if (!amqp_decode_8(encoded, &offset, &len) ||
              !amqp_decode_bytes(encoded, &offset, &p->message_id, len))
            return AMQP_STATUS_BAD_AMQP_DATA;
        }
      }
      if (flags & AMQP_BASIC_TIMESTAMP_FLAG) {
        if (!amqp_decode_64(encoded, &offset, &p->timestamp))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (flags & AMQP_BASIC_TYPE_FLAG) {
        {
          uint8_t len;
          if (!amqp_decode_8(encoded, &offset, &len) ||
              !amqp_decode_bytes(encoded, &offset, &p->type, len))
            return AMQP_STATUS_BAD_AMQP_DATA;
        }
      }
      if (flags & AMQP_BASIC_USER_ID_FLAG) {
        {
          uint8_t len;
          if (!amqp_decode_8(encoded, &offset, &len) ||
              !amqp_decode_bytes(encoded, &offset, &p->user_id, len))
            return AMQP_STATUS_BAD_AMQP_DATA;
        }
      }
      if (flags & AMQP_BASIC_APP_ID_FLAG) {
        {
          uint8_t len;
          if (!amqp_decode_8(encoded, &offset, &len) ||
              !amqp_decode_bytes(encoded, &offset, &p->app_id, len))
            return AMQP_STATUS_BAD_AMQP_DATA;
        }
      }
      if (flags & AMQP_BASIC_CLUSTER_ID_FLAG) {
        {
          uint8_t len;
          if (!amqp_decode_8(encoded, &offset, &len) ||
              !amqp_decode_bytes(encoded, &offset, &p->cluster_id, len))
            return AMQP_STATUS_BAD_AMQP_DATA;
        }
      }
      *decoded = p;
      return 0;
    }
    case 90: {
      amqp_tx_properties_t *p = (amqp_tx_properties_t *)amqp_pool_alloc(
          pool, sizeof(amqp_tx_properties_t));
      if (p == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      p->_flags = flags;
      *decoded = p;
      return 0;
    }
    case 85: {
      amqp_confirm_properties_t *p =
          (amqp_confirm_properties_t *)amqp_pool_alloc(
              pool, sizeof(amqp_confirm_properties_t));
      if (p == NULL) {
        return AMQP_STATUS_NO_MEMORY;
      }
      p->_flags = flags;
      *decoded = p;
      return 0;
    }
    default:
      return AMQP_STATUS_UNKNOWN_CLASS;
  }
}

int amqp_encode_method(amqp_method_number_t methodNumber, void *decoded,
                       amqp_bytes_t encoded) {
  size_t offset = 0;
  uint8_t bit_buffer;

  switch (methodNumber) {
    case AMQP_CONNECTION_START_METHOD: {
      amqp_connection_start_t *m = (amqp_connection_start_t *)decoded;
      if (!amqp_encode_8(encoded, &offset, m->version_major))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (!amqp_encode_8(encoded, &offset, m->version_minor))
        return AMQP_STATUS_BAD_AMQP_DATA;
      {
        int res = amqp_encode_table(encoded, &(m->server_properties), &offset);
        if (res < 0) return res;
      }
      if (UINT32_MAX < m->mechanisms.len ||
          !amqp_encode_32(encoded, &offset, (uint32_t)m->mechanisms.len) ||
          !amqp_encode_bytes(encoded, &offset, m->mechanisms))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT32_MAX < m->locales.len ||
          !amqp_encode_32(encoded, &offset, (uint32_t)m->locales.len) ||
          !amqp_encode_bytes(encoded, &offset, m->locales))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_CONNECTION_START_OK_METHOD: {
      amqp_connection_start_ok_t *m = (amqp_connection_start_ok_t *)decoded;
      {
        int res = amqp_encode_table(encoded, &(m->client_properties), &offset);
        if (res < 0) return res;
      }
      if (UINT8_MAX < m->mechanism.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->mechanism.len) ||
          !amqp_encode_bytes(encoded, &offset, m->mechanism))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT32_MAX < m->response.len ||
          !amqp_encode_32(encoded, &offset, (uint32_t)m->response.len) ||
          !amqp_encode_bytes(encoded, &offset, m->response))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->locale.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->locale.len) ||
          !amqp_encode_bytes(encoded, &offset, m->locale))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_CONNECTION_SECURE_METHOD: {
      amqp_connection_secure_t *m = (amqp_connection_secure_t *)decoded;
      if (UINT32_MAX < m->challenge.len ||
          !amqp_encode_32(encoded, &offset, (uint32_t)m->challenge.len) ||
          !amqp_encode_bytes(encoded, &offset, m->challenge))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_CONNECTION_SECURE_OK_METHOD: {
      amqp_connection_secure_ok_t *m = (amqp_connection_secure_ok_t *)decoded;
      if (UINT32_MAX < m->response.len ||
          !amqp_encode_32(encoded, &offset, (uint32_t)m->response.len) ||
          !amqp_encode_bytes(encoded, &offset, m->response))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_CONNECTION_TUNE_METHOD: {
      amqp_connection_tune_t *m = (amqp_connection_tune_t *)decoded;
      if (!amqp_encode_16(encoded, &offset, m->channel_max))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (!amqp_encode_32(encoded, &offset, m->frame_max))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (!amqp_encode_16(encoded, &offset, m->heartbeat))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_CONNECTION_TUNE_OK_METHOD: {
      amqp_connection_tune_ok_t *m = (amqp_connection_tune_ok_t *)decoded;
      if (!amqp_encode_16(encoded, &offset, m->channel_max))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (!amqp_encode_32(encoded, &offset, m->frame_max))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (!amqp_encode_16(encoded, &offset, m->heartbeat))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_CONNECTION_OPEN_METHOD: {
      amqp_connection_open_t *m = (amqp_connection_open_t *)decoded;
      if (UINT8_MAX < m->virtual_host.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->virtual_host.len) ||
          !amqp_encode_bytes(encoded, &offset, m->virtual_host))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->capabilities.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->capabilities.len) ||
          !amqp_encode_bytes(encoded, &offset, m->capabilities))
        return AMQP_STATUS_BAD_AMQP_DATA;
      bit_buffer = 0;
      if (m->insist) bit_buffer |= (1 << 0);
      if (!amqp_encode_8(encoded, &offset, bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_CONNECTION_OPEN_OK_METHOD: {
      amqp_connection_open_ok_t *m = (amqp_connection_open_ok_t *)decoded;
      if (UINT8_MAX < m->known_hosts.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->known_hosts.len) ||
          !amqp_encode_bytes(encoded, &offset, m->known_hosts))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_CONNECTION_CLOSE_METHOD: {
      amqp_connection_close_t *m = (amqp_connection_close_t *)decoded;
      if (!amqp_encode_16(encoded, &offset, m->reply_code))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->reply_text.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->reply_text.len) ||
          !amqp_encode_bytes(encoded, &offset, m->reply_text))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (!amqp_encode_16(encoded, &offset, m->class_id))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (!amqp_encode_16(encoded, &offset, m->method_id))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_CONNECTION_CLOSE_OK_METHOD: {
      return (int)offset;
    }
    case AMQP_CONNECTION_BLOCKED_METHOD: {
      amqp_connection_blocked_t *m = (amqp_connection_blocked_t *)decoded;
      if (UINT8_MAX < m->reason.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->reason.len) ||
          !amqp_encode_bytes(encoded, &offset, m->reason))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_CONNECTION_UNBLOCKED_METHOD: {
      return (int)offset;
    }
    case AMQP_CHANNEL_OPEN_METHOD: {
      amqp_channel_open_t *m = (amqp_channel_open_t *)decoded;
      if (UINT8_MAX < m->out_of_band.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->out_of_band.len) ||
          !amqp_encode_bytes(encoded, &offset, m->out_of_band))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_CHANNEL_OPEN_OK_METHOD: {
      amqp_channel_open_ok_t *m = (amqp_channel_open_ok_t *)decoded;
      if (UINT32_MAX < m->channel_id.len ||
          !amqp_encode_32(encoded, &offset, (uint32_t)m->channel_id.len) ||
          !amqp_encode_bytes(encoded, &offset, m->channel_id))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_CHANNEL_FLOW_METHOD: {
      amqp_channel_flow_t *m = (amqp_channel_flow_t *)decoded;
      bit_buffer = 0;
      if (m->active) bit_buffer |= (1 << 0);
      if (!amqp_encode_8(encoded, &offset, bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_CHANNEL_FLOW_OK_METHOD: {
      amqp_channel_flow_ok_t *m = (amqp_channel_flow_ok_t *)decoded;
      bit_buffer = 0;
      if (m->active) bit_buffer |= (1 << 0);
      if (!amqp_encode_8(encoded, &offset, bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_CHANNEL_CLOSE_METHOD: {
      amqp_channel_close_t *m = (amqp_channel_close_t *)decoded;
      if (!amqp_encode_16(encoded, &offset, m->reply_code))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->reply_text.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->reply_text.len) ||
          !amqp_encode_bytes(encoded, &offset, m->reply_text))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (!amqp_encode_16(encoded, &offset, m->class_id))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (!amqp_encode_16(encoded, &offset, m->method_id))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_CHANNEL_CLOSE_OK_METHOD: {
      return (int)offset;
    }
    case AMQP_ACCESS_REQUEST_METHOD: {
      amqp_access_request_t *m = (amqp_access_request_t *)decoded;
      if (UINT8_MAX < m->realm.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->realm.len) ||
          !amqp_encode_bytes(encoded, &offset, m->realm))
        return AMQP_STATUS_BAD_AMQP_DATA;
      bit_buffer = 0;
      if (m->exclusive) bit_buffer |= (1 << 0);
      if (m->passive) bit_buffer |= (1 << 1);
      if (m->active) bit_buffer |= (1 << 2);
      if (m->write) bit_buffer |= (1 << 3);
      if (m->read) bit_buffer |= (1 << 4);
      if (!amqp_encode_8(encoded, &offset, bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_ACCESS_REQUEST_OK_METHOD: {
      amqp_access_request_ok_t *m = (amqp_access_request_ok_t *)decoded;
      if (!amqp_encode_16(encoded, &offset, m->ticket))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_EXCHANGE_DECLARE_METHOD: {
      amqp_exchange_declare_t *m = (amqp_exchange_declare_t *)decoded;
      if (!amqp_encode_16(encoded, &offset, m->ticket))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->exchange.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->exchange.len) ||
          !amqp_encode_bytes(encoded, &offset, m->exchange))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->type.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->type.len) ||
          !amqp_encode_bytes(encoded, &offset, m->type))
        return AMQP_STATUS_BAD_AMQP_DATA;
      bit_buffer = 0;
      if (m->passive) bit_buffer |= (1 << 0);
      if (m->durable) bit_buffer |= (1 << 1);
      if (m->auto_delete) bit_buffer |= (1 << 2);
      if (m->internal) bit_buffer |= (1 << 3);
      if (m->nowait) bit_buffer |= (1 << 4);
      if (!amqp_encode_8(encoded, &offset, bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      {
        int res = amqp_encode_table(encoded, &(m->arguments), &offset);
        if (res < 0) return res;
      }
      return (int)offset;
    }
    case AMQP_EXCHANGE_DECLARE_OK_METHOD: {
      return (int)offset;
    }
    case AMQP_EXCHANGE_DELETE_METHOD: {
      amqp_exchange_delete_t *m = (amqp_exchange_delete_t *)decoded;
      if (!amqp_encode_16(encoded, &offset, m->ticket))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->exchange.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->exchange.len) ||
          !amqp_encode_bytes(encoded, &offset, m->exchange))
        return AMQP_STATUS_BAD_AMQP_DATA;
      bit_buffer = 0;
      if (m->if_unused) bit_buffer |= (1 << 0);
      if (m->nowait) bit_buffer |= (1 << 1);
      if (!amqp_encode_8(encoded, &offset, bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_EXCHANGE_DELETE_OK_METHOD: {
      return (int)offset;
    }
    case AMQP_EXCHANGE_BIND_METHOD: {
      amqp_exchange_bind_t *m = (amqp_exchange_bind_t *)decoded;
      if (!amqp_encode_16(encoded, &offset, m->ticket))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->destination.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->destination.len) ||
          !amqp_encode_bytes(encoded, &offset, m->destination))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->source.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->source.len) ||
          !amqp_encode_bytes(encoded, &offset, m->source))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->routing_key.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->routing_key.len) ||
          !amqp_encode_bytes(encoded, &offset, m->routing_key))
        return AMQP_STATUS_BAD_AMQP_DATA;
      bit_buffer = 0;
      if (m->nowait) bit_buffer |= (1 << 0);
      if (!amqp_encode_8(encoded, &offset, bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      {
        int res = amqp_encode_table(encoded, &(m->arguments), &offset);
        if (res < 0) return res;
      }
      return (int)offset;
    }
    case AMQP_EXCHANGE_BIND_OK_METHOD: {
      return (int)offset;
    }
    case AMQP_EXCHANGE_UNBIND_METHOD: {
      amqp_exchange_unbind_t *m = (amqp_exchange_unbind_t *)decoded;
      if (!amqp_encode_16(encoded, &offset, m->ticket))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->destination.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->destination.len) ||
          !amqp_encode_bytes(encoded, &offset, m->destination))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->source.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->source.len) ||
          !amqp_encode_bytes(encoded, &offset, m->source))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->routing_key.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->routing_key.len) ||
          !amqp_encode_bytes(encoded, &offset, m->routing_key))
        return AMQP_STATUS_BAD_AMQP_DATA;
      bit_buffer = 0;
      if (m->nowait) bit_buffer |= (1 << 0);
      if (!amqp_encode_8(encoded, &offset, bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      {
        int res = amqp_encode_table(encoded, &(m->arguments), &offset);
        if (res < 0) return res;
      }
      return (int)offset;
    }
    case AMQP_EXCHANGE_UNBIND_OK_METHOD: {
      return (int)offset;
    }
    case AMQP_QUEUE_DECLARE_METHOD: {
      amqp_queue_declare_t *m = (amqp_queue_declare_t *)decoded;
      if (!amqp_encode_16(encoded, &offset, m->ticket))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->queue.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->queue.len) ||
          !amqp_encode_bytes(encoded, &offset, m->queue))
        return AMQP_STATUS_BAD_AMQP_DATA;
      bit_buffer = 0;
      if (m->passive) bit_buffer |= (1 << 0);
      if (m->durable) bit_buffer |= (1 << 1);
      if (m->exclusive) bit_buffer |= (1 << 2);
      if (m->auto_delete) bit_buffer |= (1 << 3);
      if (m->nowait) bit_buffer |= (1 << 4);
      if (!amqp_encode_8(encoded, &offset, bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      {
        int res = amqp_encode_table(encoded, &(m->arguments), &offset);
        if (res < 0) return res;
      }
      return (int)offset;
    }
    case AMQP_QUEUE_DECLARE_OK_METHOD: {
      amqp_queue_declare_ok_t *m = (amqp_queue_declare_ok_t *)decoded;
      if (UINT8_MAX < m->queue.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->queue.len) ||
          !amqp_encode_bytes(encoded, &offset, m->queue))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (!amqp_encode_32(encoded, &offset, m->message_count))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (!amqp_encode_32(encoded, &offset, m->consumer_count))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_QUEUE_BIND_METHOD: {
      amqp_queue_bind_t *m = (amqp_queue_bind_t *)decoded;
      if (!amqp_encode_16(encoded, &offset, m->ticket))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->queue.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->queue.len) ||
          !amqp_encode_bytes(encoded, &offset, m->queue))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->exchange.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->exchange.len) ||
          !amqp_encode_bytes(encoded, &offset, m->exchange))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->routing_key.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->routing_key.len) ||
          !amqp_encode_bytes(encoded, &offset, m->routing_key))
        return AMQP_STATUS_BAD_AMQP_DATA;
      bit_buffer = 0;
      if (m->nowait) bit_buffer |= (1 << 0);
      if (!amqp_encode_8(encoded, &offset, bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      {
        int res = amqp_encode_table(encoded, &(m->arguments), &offset);
        if (res < 0) return res;
      }
      return (int)offset;
    }
    case AMQP_QUEUE_BIND_OK_METHOD: {
      return (int)offset;
    }
    case AMQP_QUEUE_PURGE_METHOD: {
      amqp_queue_purge_t *m = (amqp_queue_purge_t *)decoded;
      if (!amqp_encode_16(encoded, &offset, m->ticket))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->queue.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->queue.len) ||
          !amqp_encode_bytes(encoded, &offset, m->queue))
        return AMQP_STATUS_BAD_AMQP_DATA;
      bit_buffer = 0;
      if (m->nowait) bit_buffer |= (1 << 0);
      if (!amqp_encode_8(encoded, &offset, bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_QUEUE_PURGE_OK_METHOD: {
      amqp_queue_purge_ok_t *m = (amqp_queue_purge_ok_t *)decoded;
      if (!amqp_encode_32(encoded, &offset, m->message_count))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_QUEUE_DELETE_METHOD: {
      amqp_queue_delete_t *m = (amqp_queue_delete_t *)decoded;
      if (!amqp_encode_16(encoded, &offset, m->ticket))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->queue.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->queue.len) ||
          !amqp_encode_bytes(encoded, &offset, m->queue))
        return AMQP_STATUS_BAD_AMQP_DATA;
      bit_buffer = 0;
      if (m->if_unused) bit_buffer |= (1 << 0);
      if (m->if_empty) bit_buffer |= (1 << 1);
      if (m->nowait) bit_buffer |= (1 << 2);
      if (!amqp_encode_8(encoded, &offset, bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_QUEUE_DELETE_OK_METHOD: {
      amqp_queue_delete_ok_t *m = (amqp_queue_delete_ok_t *)decoded;
      if (!amqp_encode_32(encoded, &offset, m->message_count))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_QUEUE_UNBIND_METHOD: {
      amqp_queue_unbind_t *m = (amqp_queue_unbind_t *)decoded;
      if (!amqp_encode_16(encoded, &offset, m->ticket))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->queue.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->queue.len) ||
          !amqp_encode_bytes(encoded, &offset, m->queue))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->exchange.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->exchange.len) ||
          !amqp_encode_bytes(encoded, &offset, m->exchange))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->routing_key.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->routing_key.len) ||
          !amqp_encode_bytes(encoded, &offset, m->routing_key))
        return AMQP_STATUS_BAD_AMQP_DATA;
      {
        int res = amqp_encode_table(encoded, &(m->arguments), &offset);
        if (res < 0) return res;
      }
      return (int)offset;
    }
    case AMQP_QUEUE_UNBIND_OK_METHOD: {
      return (int)offset;
    }
    case AMQP_BASIC_QOS_METHOD: {
      amqp_basic_qos_t *m = (amqp_basic_qos_t *)decoded;
      if (!amqp_encode_32(encoded, &offset, m->prefetch_size))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (!amqp_encode_16(encoded, &offset, m->prefetch_count))
        return AMQP_STATUS_BAD_AMQP_DATA;
      bit_buffer = 0;
      if (m->global) bit_buffer |= (1 << 0);
      if (!amqp_encode_8(encoded, &offset, bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_BASIC_QOS_OK_METHOD: {
      return (int)offset;
    }
    case AMQP_BASIC_CONSUME_METHOD: {
      amqp_basic_consume_t *m = (amqp_basic_consume_t *)decoded;
      if (!amqp_encode_16(encoded, &offset, m->ticket))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->queue.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->queue.len) ||
          !amqp_encode_bytes(encoded, &offset, m->queue))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->consumer_tag.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->consumer_tag.len) ||
          !amqp_encode_bytes(encoded, &offset, m->consumer_tag))
        return AMQP_STATUS_BAD_AMQP_DATA;
      bit_buffer = 0;
      if (m->no_local) bit_buffer |= (1 << 0);
      if (m->no_ack) bit_buffer |= (1 << 1);
      if (m->exclusive) bit_buffer |= (1 << 2);
      if (m->nowait) bit_buffer |= (1 << 3);
      if (!amqp_encode_8(encoded, &offset, bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      {
        int res = amqp_encode_table(encoded, &(m->arguments), &offset);
        if (res < 0) return res;
      }
      return (int)offset;
    }
    case AMQP_BASIC_CONSUME_OK_METHOD: {
      amqp_basic_consume_ok_t *m = (amqp_basic_consume_ok_t *)decoded;
      if (UINT8_MAX < m->consumer_tag.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->consumer_tag.len) ||
          !amqp_encode_bytes(encoded, &offset, m->consumer_tag))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_BASIC_CANCEL_METHOD: {
      amqp_basic_cancel_t *m = (amqp_basic_cancel_t *)decoded;
      if (UINT8_MAX < m->consumer_tag.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->consumer_tag.len) ||
          !amqp_encode_bytes(encoded, &offset, m->consumer_tag))
        return AMQP_STATUS_BAD_AMQP_DATA;
      bit_buffer = 0;
      if (m->nowait) bit_buffer |= (1 << 0);
      if (!amqp_encode_8(encoded, &offset, bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_BASIC_CANCEL_OK_METHOD: {
      amqp_basic_cancel_ok_t *m = (amqp_basic_cancel_ok_t *)decoded;
      if (UINT8_MAX < m->consumer_tag.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->consumer_tag.len) ||
          !amqp_encode_bytes(encoded, &offset, m->consumer_tag))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_BASIC_PUBLISH_METHOD: {
      amqp_basic_publish_t *m = (amqp_basic_publish_t *)decoded;
      if (!amqp_encode_16(encoded, &offset, m->ticket))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->exchange.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->exchange.len) ||
          !amqp_encode_bytes(encoded, &offset, m->exchange))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->routing_key.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->routing_key.len) ||
          !amqp_encode_bytes(encoded, &offset, m->routing_key))
        return AMQP_STATUS_BAD_AMQP_DATA;
      bit_buffer = 0;
      if (m->mandatory) bit_buffer |= (1 << 0);
      if (m->immediate) bit_buffer |= (1 << 1);
      if (!amqp_encode_8(encoded, &offset, bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_BASIC_RETURN_METHOD: {
      amqp_basic_return_t *m = (amqp_basic_return_t *)decoded;
      if (!amqp_encode_16(encoded, &offset, m->reply_code))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->reply_text.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->reply_text.len) ||
          !amqp_encode_bytes(encoded, &offset, m->reply_text))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->exchange.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->exchange.len) ||
          !amqp_encode_bytes(encoded, &offset, m->exchange))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->routing_key.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->routing_key.len) ||
          !amqp_encode_bytes(encoded, &offset, m->routing_key))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_BASIC_DELIVER_METHOD: {
      amqp_basic_deliver_t *m = (amqp_basic_deliver_t *)decoded;
      if (UINT8_MAX < m->consumer_tag.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->consumer_tag.len) ||
          !amqp_encode_bytes(encoded, &offset, m->consumer_tag))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (!amqp_encode_64(encoded, &offset, m->delivery_tag))
        return AMQP_STATUS_BAD_AMQP_DATA;
      bit_buffer = 0;
      if (m->redelivered) bit_buffer |= (1 << 0);
      if (!amqp_encode_8(encoded, &offset, bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->exchange.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->exchange.len) ||
          !amqp_encode_bytes(encoded, &offset, m->exchange))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->routing_key.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->routing_key.len) ||
          !amqp_encode_bytes(encoded, &offset, m->routing_key))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_BASIC_GET_METHOD: {
      amqp_basic_get_t *m = (amqp_basic_get_t *)decoded;
      if (!amqp_encode_16(encoded, &offset, m->ticket))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->queue.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->queue.len) ||
          !amqp_encode_bytes(encoded, &offset, m->queue))
        return AMQP_STATUS_BAD_AMQP_DATA;
      bit_buffer = 0;
      if (m->no_ack) bit_buffer |= (1 << 0);
      if (!amqp_encode_8(encoded, &offset, bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_BASIC_GET_OK_METHOD: {
      amqp_basic_get_ok_t *m = (amqp_basic_get_ok_t *)decoded;
      if (!amqp_encode_64(encoded, &offset, m->delivery_tag))
        return AMQP_STATUS_BAD_AMQP_DATA;
      bit_buffer = 0;
      if (m->redelivered) bit_buffer |= (1 << 0);
      if (!amqp_encode_8(encoded, &offset, bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->exchange.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->exchange.len) ||
          !amqp_encode_bytes(encoded, &offset, m->exchange))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (UINT8_MAX < m->routing_key.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->routing_key.len) ||
          !amqp_encode_bytes(encoded, &offset, m->routing_key))
        return AMQP_STATUS_BAD_AMQP_DATA;
      if (!amqp_encode_32(encoded, &offset, m->message_count))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_BASIC_GET_EMPTY_METHOD: {
      amqp_basic_get_empty_t *m = (amqp_basic_get_empty_t *)decoded;
      if (UINT8_MAX < m->cluster_id.len ||
          !amqp_encode_8(encoded, &offset, (uint8_t)m->cluster_id.len) ||
          !amqp_encode_bytes(encoded, &offset, m->cluster_id))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_BASIC_ACK_METHOD: {
      amqp_basic_ack_t *m = (amqp_basic_ack_t *)decoded;
      if (!amqp_encode_64(encoded, &offset, m->delivery_tag))
        return AMQP_STATUS_BAD_AMQP_DATA;
      bit_buffer = 0;
      if (m->multiple) bit_buffer |= (1 << 0);
      if (!amqp_encode_8(encoded, &offset, bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_BASIC_REJECT_METHOD: {
      amqp_basic_reject_t *m = (amqp_basic_reject_t *)decoded;
      if (!amqp_encode_64(encoded, &offset, m->delivery_tag))
        return AMQP_STATUS_BAD_AMQP_DATA;
      bit_buffer = 0;
      if (m->requeue) bit_buffer |= (1 << 0);
      if (!amqp_encode_8(encoded, &offset, bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_BASIC_RECOVER_ASYNC_METHOD: {
      amqp_basic_recover_async_t *m = (amqp_basic_recover_async_t *)decoded;
      bit_buffer = 0;
      if (m->requeue) bit_buffer |= (1 << 0);
      if (!amqp_encode_8(encoded, &offset, bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_BASIC_RECOVER_METHOD: {
      amqp_basic_recover_t *m = (amqp_basic_recover_t *)decoded;
      bit_buffer = 0;
      if (m->requeue) bit_buffer |= (1 << 0);
      if (!amqp_encode_8(encoded, &offset, bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_BASIC_RECOVER_OK_METHOD: {
      return (int)offset;
    }
    case AMQP_BASIC_NACK_METHOD: {
      amqp_basic_nack_t *m = (amqp_basic_nack_t *)decoded;
      if (!amqp_encode_64(encoded, &offset, m->delivery_tag))
        return AMQP_STATUS_BAD_AMQP_DATA;
      bit_buffer = 0;
      if (m->multiple) bit_buffer |= (1 << 0);
      if (m->requeue) bit_buffer |= (1 << 1);
      if (!amqp_encode_8(encoded, &offset, bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_TX_SELECT_METHOD: {
      return (int)offset;
    }
    case AMQP_TX_SELECT_OK_METHOD: {
      return (int)offset;
    }
    case AMQP_TX_COMMIT_METHOD: {
      return (int)offset;
    }
    case AMQP_TX_COMMIT_OK_METHOD: {
      return (int)offset;
    }
    case AMQP_TX_ROLLBACK_METHOD: {
      return (int)offset;
    }
    case AMQP_TX_ROLLBACK_OK_METHOD: {
      return (int)offset;
    }
    case AMQP_CONFIRM_SELECT_METHOD: {
      amqp_confirm_select_t *m = (amqp_confirm_select_t *)decoded;
      bit_buffer = 0;
      if (m->nowait) bit_buffer |= (1 << 0);
      if (!amqp_encode_8(encoded, &offset, bit_buffer))
        return AMQP_STATUS_BAD_AMQP_DATA;
      return (int)offset;
    }
    case AMQP_CONFIRM_SELECT_OK_METHOD: {
      return (int)offset;
    }
    default:
      return AMQP_STATUS_UNKNOWN_METHOD;
  }
}

int amqp_encode_properties(uint16_t class_id, void *decoded,
                           amqp_bytes_t encoded) {
  size_t offset = 0;

  /* Cheat, and get the flags out generically, relying on the
     similarity of structure between classes */
  amqp_flags_t flags = *(amqp_flags_t *)decoded; /* cheating! */

  {
    /* We take a copy of flags to avoid destroying it, as it is used
       in the autogenerated code below. */
    amqp_flags_t remaining_flags = flags;
    do {
      amqp_flags_t remainder = remaining_flags >> 16;
      uint16_t partial_flags = remaining_flags & 0xFFFE;
      if (remainder != 0) {
        partial_flags |= 1;
      }
      if (!amqp_encode_16(encoded, &offset, partial_flags))
        return AMQP_STATUS_BAD_AMQP_DATA;
      remaining_flags = remainder;
    } while (remaining_flags != 0);
  }

  switch (class_id) {
    case 10: {
      return (int)offset;
    }
    case 20: {
      return (int)offset;
    }
    case 30: {
      return (int)offset;
    }
    case 40: {
      return (int)offset;
    }
    case 50: {
      return (int)offset;
    }
    case 60: {
      amqp_basic_properties_t *p = (amqp_basic_properties_t *)decoded;
      if (flags & AMQP_BASIC_CONTENT_TYPE_FLAG) {
        if (UINT8_MAX < p->content_type.len ||
            !amqp_encode_8(encoded, &offset, (uint8_t)p->content_type.len) ||
            !amqp_encode_bytes(encoded, &offset, p->content_type))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (flags & AMQP_BASIC_CONTENT_ENCODING_FLAG) {
        if (UINT8_MAX < p->content_encoding.len ||
            !amqp_encode_8(encoded, &offset,
                           (uint8_t)p->content_encoding.len) ||
            !amqp_encode_bytes(encoded, &offset, p->content_encoding))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (flags & AMQP_BASIC_HEADERS_FLAG) {
        {
          int res = amqp_encode_table(encoded, &(p->headers), &offset);
          if (res < 0) return res;
        }
      }
      if (flags & AMQP_BASIC_DELIVERY_MODE_FLAG) {
        if (!amqp_encode_8(encoded, &offset, p->delivery_mode))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (flags & AMQP_BASIC_PRIORITY_FLAG) {
        if (!amqp_encode_8(encoded, &offset, p->priority))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (flags & AMQP_BASIC_CORRELATION_ID_FLAG) {
        if (UINT8_MAX < p->correlation_id.len ||
            !amqp_encode_8(encoded, &offset, (uint8_t)p->correlation_id.len) ||
            !amqp_encode_bytes(encoded, &offset, p->correlation_id))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (flags & AMQP_BASIC_REPLY_TO_FLAG) {
        if (UINT8_MAX < p->reply_to.len ||
            !amqp_encode_8(encoded, &offset, (uint8_t)p->reply_to.len) ||
            !amqp_encode_bytes(encoded, &offset, p->reply_to))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (flags & AMQP_BASIC_EXPIRATION_FLAG) {
        if (UINT8_MAX < p->expiration.len ||
            !amqp_encode_8(encoded, &offset, (uint8_t)p->expiration.len) ||
            !amqp_encode_bytes(encoded, &offset, p->expiration))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (flags & AMQP_BASIC_MESSAGE_ID_FLAG) {
        if (UINT8_MAX < p->message_id.len ||
            !amqp_encode_8(encoded, &offset, (uint8_t)p->message_id.len) ||
            !amqp_encode_bytes(encoded, &offset, p->message_id))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (flags & AMQP_BASIC_TIMESTAMP_FLAG) {
        if (!amqp_encode_64(encoded, &offset, p->timestamp))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (flags & AMQP_BASIC_TYPE_FLAG) {
        if (UINT8_MAX < p->type.len ||
            !amqp_encode_8(encoded, &offset, (uint8_t)p->type.len) ||
            !amqp_encode_bytes(encoded, &offset, p->type))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (flags & AMQP_BASIC_USER_ID_FLAG) {
        if (UINT8_MAX < p->user_id.len ||
            !amqp_encode_8(encoded, &offset, (uint8_t)p->user_id.len) ||
            !amqp_encode_bytes(encoded, &offset, p->user_id))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (flags & AMQP_BASIC_APP_ID_FLAG) {
        if (UINT8_MAX < p->app_id.len ||
            !amqp_encode_8(encoded, &offset, (uint8_t)p->app_id.len) ||
            !amqp_encode_bytes(encoded, &offset, p->app_id))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      if (flags & AMQP_BASIC_CLUSTER_ID_FLAG) {
        if (UINT8_MAX < p->cluster_id.len ||
            !amqp_encode_8(encoded, &offset, (uint8_t)p->cluster_id.len) ||
            !amqp_encode_bytes(encoded, &offset, p->cluster_id))
          return AMQP_STATUS_BAD_AMQP_DATA;
      }
      return (int)offset;
    }
    case 90: {
      return (int)offset;
    }
    case 85: {
      return (int)offset;
    }
    default:
      return AMQP_STATUS_UNKNOWN_CLASS;
  }
}

/**
 * amqp_channel_open
 *
 * @param [in] state connection state
 * @param [in] channel the channel to do the RPC on
 * @returns amqp_channel_open_ok_t
 */
AMQP_PUBLIC_FUNCTION
amqp_channel_open_ok_t *AMQP_CALL
    amqp_channel_open(amqp_connection_state_t state, amqp_channel_t channel) {
  amqp_channel_open_t req;
  req.out_of_band = amqp_empty_bytes;

  return amqp_simple_rpc_decoded(state, channel, AMQP_CHANNEL_OPEN_METHOD,
                                 AMQP_CHANNEL_OPEN_OK_METHOD, &req);
}

/**
 * amqp_channel_flow
 *
 * @param [in] state connection state
 * @param [in] channel the channel to do the RPC on
 * @param [in] active active
 * @returns amqp_channel_flow_ok_t
 */
AMQP_PUBLIC_FUNCTION
amqp_channel_flow_ok_t *AMQP_CALL
    amqp_channel_flow(amqp_connection_state_t state, amqp_channel_t channel,
                      amqp_boolean_t active) {
  amqp_channel_flow_t req;
  req.active = active;

  return amqp_simple_rpc_decoded(state, channel, AMQP_CHANNEL_FLOW_METHOD,
                                 AMQP_CHANNEL_FLOW_OK_METHOD, &req);
}

/**
 * amqp_exchange_declare
 *
 * @param [in] state connection state
 * @param [in] channel the channel to do the RPC on
 * @param [in] exchange exchange
 * @param [in] type type
 * @param [in] passive passive
 * @param [in] durable durable
 * @param [in] auto_delete auto_delete
 * @param [in] internal internal
 * @param [in] arguments arguments
 * @returns amqp_exchange_declare_ok_t
 */
AMQP_PUBLIC_FUNCTION
amqp_exchange_declare_ok_t *AMQP_CALL amqp_exchange_declare(
    amqp_connection_state_t state, amqp_channel_t channel,
    amqp_bytes_t exchange, amqp_bytes_t type, amqp_boolean_t passive,
    amqp_boolean_t durable, amqp_boolean_t auto_delete, amqp_boolean_t internal,
    amqp_table_t arguments) {
  amqp_exchange_declare_t req;
  req.ticket = 0;
  req.exchange = exchange;
  req.type = type;
  req.passive = passive;
  req.durable = durable;
  req.auto_delete = auto_delete;
  req.internal = internal;
  req.nowait = 0;
  req.arguments = arguments;

  return amqp_simple_rpc_decoded(state, channel, AMQP_EXCHANGE_DECLARE_METHOD,
                                 AMQP_EXCHANGE_DECLARE_OK_METHOD, &req);
}

/**
 * amqp_exchange_delete
 *
 * @param [in] state connection state
 * @param [in] channel the channel to do the RPC on
 * @param [in] exchange exchange
 * @param [in] if_unused if_unused
 * @returns amqp_exchange_delete_ok_t
 */
AMQP_PUBLIC_FUNCTION
amqp_exchange_delete_ok_t *AMQP_CALL
    amqp_exchange_delete(amqp_connection_state_t state, amqp_channel_t channel,
                         amqp_bytes_t exchange, amqp_boolean_t if_unused) {
  amqp_exchange_delete_t req;
  req.ticket = 0;
  req.exchange = exchange;
  req.if_unused = if_unused;
  req.nowait = 0;

  return amqp_simple_rpc_decoded(state, channel, AMQP_EXCHANGE_DELETE_METHOD,
                                 AMQP_EXCHANGE_DELETE_OK_METHOD, &req);
}

/**
 * amqp_exchange_bind
 *
 * @param [in] state connection state
 * @param [in] channel the channel to do the RPC on
 * @param [in] destination destination
 * @param [in] source source
 * @param [in] routing_key routing_key
 * @param [in] arguments arguments
 * @returns amqp_exchange_bind_ok_t
 */
AMQP_PUBLIC_FUNCTION
amqp_exchange_bind_ok_t *AMQP_CALL
    amqp_exchange_bind(amqp_connection_state_t state, amqp_channel_t channel,
                       amqp_bytes_t destination, amqp_bytes_t source,
                       amqp_bytes_t routing_key, amqp_table_t arguments) {
  amqp_exchange_bind_t req;
  req.ticket = 0;
  req.destination = destination;
  req.source = source;
  req.routing_key = routing_key;
  req.nowait = 0;
  req.arguments = arguments;

  return amqp_simple_rpc_decoded(state, channel, AMQP_EXCHANGE_BIND_METHOD,
                                 AMQP_EXCHANGE_BIND_OK_METHOD, &req);
}

/**
 * amqp_exchange_unbind
 *
 * @param [in] state connection state
 * @param [in] channel the channel to do the RPC on
 * @param [in] destination destination
 * @param [in] source source
 * @param [in] routing_key routing_key
 * @param [in] arguments arguments
 * @returns amqp_exchange_unbind_ok_t
 */
AMQP_PUBLIC_FUNCTION
amqp_exchange_unbind_ok_t *AMQP_CALL
    amqp_exchange_unbind(amqp_connection_state_t state, amqp_channel_t channel,
                         amqp_bytes_t destination, amqp_bytes_t source,
                         amqp_bytes_t routing_key, amqp_table_t arguments) {
  amqp_exchange_unbind_t req;
  req.ticket = 0;
  req.destination = destination;
  req.source = source;
  req.routing_key = routing_key;
  req.nowait = 0;
  req.arguments = arguments;

  return amqp_simple_rpc_decoded(state, channel, AMQP_EXCHANGE_UNBIND_METHOD,
                                 AMQP_EXCHANGE_UNBIND_OK_METHOD, &req);
}

/**
 * amqp_queue_declare
 *
 * @param [in] state connection state
 * @param [in] channel the channel to do the RPC on
 * @param [in] queue queue
 * @param [in] passive passive
 * @param [in] durable durable
 * @param [in] exclusive exclusive
 * @param [in] auto_delete auto_delete
 * @param [in] arguments arguments
 * @returns amqp_queue_declare_ok_t
 */
AMQP_PUBLIC_FUNCTION
amqp_queue_declare_ok_t *AMQP_CALL amqp_queue_declare(
    amqp_connection_state_t state, amqp_channel_t channel, amqp_bytes_t queue,
    amqp_boolean_t passive, amqp_boolean_t durable, amqp_boolean_t exclusive,
    amqp_boolean_t auto_delete, amqp_table_t arguments) {
  amqp_queue_declare_t req;
  req.ticket = 0;
  req.queue = queue;
  req.passive = passive;
  req.durable = durable;
  req.exclusive = exclusive;
  req.auto_delete = auto_delete;
  req.nowait = 0;
  req.arguments = arguments;

  return amqp_simple_rpc_decoded(state, channel, AMQP_QUEUE_DECLARE_METHOD,
                                 AMQP_QUEUE_DECLARE_OK_METHOD, &req);
}

/**
 * amqp_queue_bind
 *
 * @param [in] state connection state
 * @param [in] channel the channel to do the RPC on
 * @param [in] queue queue
 * @param [in] exchange exchange
 * @param [in] routing_key routing_key
 * @param [in] arguments arguments
 * @returns amqp_queue_bind_ok_t
 */
AMQP_PUBLIC_FUNCTION
amqp_queue_bind_ok_t *AMQP_CALL amqp_queue_bind(
    amqp_connection_state_t state, amqp_channel_t channel, amqp_bytes_t queue,
    amqp_bytes_t exchange, amqp_bytes_t routing_key, amqp_table_t arguments) {
  amqp_queue_bind_t req;
  req.ticket = 0;
  req.queue = queue;
  req.exchange = exchange;
  req.routing_key = routing_key;
  req.nowait = 0;
  req.arguments = arguments;

  return amqp_simple_rpc_decoded(state, channel, AMQP_QUEUE_BIND_METHOD,
                                 AMQP_QUEUE_BIND_OK_METHOD, &req);
}

/**
 * amqp_queue_purge
 *
 * @param [in] state connection state
 * @param [in] channel the channel to do the RPC on
 * @param [in] queue queue
 * @returns amqp_queue_purge_ok_t
 */
AMQP_PUBLIC_FUNCTION
amqp_queue_purge_ok_t *AMQP_CALL amqp_queue_purge(amqp_connection_state_t state,
                                                  amqp_channel_t channel,
                                                  amqp_bytes_t queue) {
  amqp_queue_purge_t req;
  req.ticket = 0;
  req.queue = queue;
  req.nowait = 0;

  return amqp_simple_rpc_decoded(state, channel, AMQP_QUEUE_PURGE_METHOD,
                                 AMQP_QUEUE_PURGE_OK_METHOD, &req);
}

/**
 * amqp_queue_delete
 *
 * @param [in] state connection state
 * @param [in] channel the channel to do the RPC on
 * @param [in] queue queue
 * @param [in] if_unused if_unused
 * @param [in] if_empty if_empty
 * @returns amqp_queue_delete_ok_t
 */
AMQP_PUBLIC_FUNCTION
amqp_queue_delete_ok_t *AMQP_CALL amqp_queue_delete(
    amqp_connection_state_t state, amqp_channel_t channel, amqp_bytes_t queue,
    amqp_boolean_t if_unused, amqp_boolean_t if_empty) {
  amqp_queue_delete_t req;
  req.ticket = 0;
  req.queue = queue;
  req.if_unused = if_unused;
  req.if_empty = if_empty;
  req.nowait = 0;

  return amqp_simple_rpc_decoded(state, channel, AMQP_QUEUE_DELETE_METHOD,
                                 AMQP_QUEUE_DELETE_OK_METHOD, &req);
}

/**
 * amqp_queue_unbind
 *
 * @param [in] state connection state
 * @param [in] channel the channel to do the RPC on
 * @param [in] queue queue
 * @param [in] exchange exchange
 * @param [in] routing_key routing_key
 * @param [in] arguments arguments
 * @returns amqp_queue_unbind_ok_t
 */
AMQP_PUBLIC_FUNCTION
amqp_queue_unbind_ok_t *AMQP_CALL amqp_queue_unbind(
    amqp_connection_state_t state, amqp_channel_t channel, amqp_bytes_t queue,
    amqp_bytes_t exchange, amqp_bytes_t routing_key, amqp_table_t arguments) {
  amqp_queue_unbind_t req;
  req.ticket = 0;
  req.queue = queue;
  req.exchange = exchange;
  req.routing_key = routing_key;
  req.arguments = arguments;

  return amqp_simple_rpc_decoded(state, channel, AMQP_QUEUE_UNBIND_METHOD,
                                 AMQP_QUEUE_UNBIND_OK_METHOD, &req);
}

/**
 * amqp_basic_qos
 *
 * @param [in] state connection state
 * @param [in] channel the channel to do the RPC on
 * @param [in] prefetch_size prefetch_size
 * @param [in] prefetch_count prefetch_count
 * @param [in] global global
 * @returns amqp_basic_qos_ok_t
 */
AMQP_PUBLIC_FUNCTION
amqp_basic_qos_ok_t *AMQP_CALL amqp_basic_qos(amqp_connection_state_t state,
                                              amqp_channel_t channel,
                                              uint32_t prefetch_size,
                                              uint16_t prefetch_count,
                                              amqp_boolean_t global) {
  amqp_basic_qos_t req;
  req.prefetch_size = prefetch_size;
  req.prefetch_count = prefetch_count;
  req.global = global;

  return amqp_simple_rpc_decoded(state, channel, AMQP_BASIC_QOS_METHOD,
                                 AMQP_BASIC_QOS_OK_METHOD, &req);
}

/**
 * amqp_basic_consume
 *
 * @param [in] state connection state
 * @param [in] channel the channel to do the RPC on
 * @param [in] queue queue
 * @param [in] consumer_tag consumer_tag
 * @param [in] no_local no_local
 * @param [in] no_ack no_ack
 * @param [in] exclusive exclusive
 * @param [in] arguments arguments
 * @returns amqp_basic_consume_ok_t
 */
AMQP_PUBLIC_FUNCTION
amqp_basic_consume_ok_t *AMQP_CALL amqp_basic_consume(
    amqp_connection_state_t state, amqp_channel_t channel, amqp_bytes_t queue,
    amqp_bytes_t consumer_tag, amqp_boolean_t no_local, amqp_boolean_t no_ack,
    amqp_boolean_t exclusive, amqp_table_t arguments) {
  amqp_basic_consume_t req;
  req.ticket = 0;
  req.queue = queue;
  req.consumer_tag = consumer_tag;
  req.no_local = no_local;
  req.no_ack = no_ack;
  req.exclusive = exclusive;
  req.nowait = 0;
  req.arguments = arguments;

  return amqp_simple_rpc_decoded(state, channel, AMQP_BASIC_CONSUME_METHOD,
                                 AMQP_BASIC_CONSUME_OK_METHOD, &req);
}

/**
 * amqp_basic_cancel
 *
 * @param [in] state connection state
 * @param [in] channel the channel to do the RPC on
 * @param [in] consumer_tag consumer_tag
 * @returns amqp_basic_cancel_ok_t
 */
AMQP_PUBLIC_FUNCTION
amqp_basic_cancel_ok_t *AMQP_CALL
    amqp_basic_cancel(amqp_connection_state_t state, amqp_channel_t channel,
                      amqp_bytes_t consumer_tag) {
  amqp_basic_cancel_t req;
  req.consumer_tag = consumer_tag;
  req.nowait = 0;

  return amqp_simple_rpc_decoded(state, channel, AMQP_BASIC_CANCEL_METHOD,
                                 AMQP_BASIC_CANCEL_OK_METHOD, &req);
}

/**
 * amqp_basic_recover
 *
 * @param [in] state connection state
 * @param [in] channel the channel to do the RPC on
 * @param [in] requeue requeue
 * @returns amqp_basic_recover_ok_t
 */
AMQP_PUBLIC_FUNCTION
amqp_basic_recover_ok_t *AMQP_CALL
    amqp_basic_recover(amqp_connection_state_t state, amqp_channel_t channel,
                       amqp_boolean_t requeue) {
  amqp_basic_recover_t req;
  req.requeue = requeue;

  return amqp_simple_rpc_decoded(state, channel, AMQP_BASIC_RECOVER_METHOD,
                                 AMQP_BASIC_RECOVER_OK_METHOD, &req);
}

/**
 * amqp_tx_select
 *
 * @param [in] state connection state
 * @param [in] channel the channel to do the RPC on
 * @returns amqp_tx_select_ok_t
 */
AMQP_PUBLIC_FUNCTION
amqp_tx_select_ok_t *AMQP_CALL amqp_tx_select(amqp_connection_state_t state,
                                              amqp_channel_t channel) {
  amqp_tx_select_t req;

  return amqp_simple_rpc_decoded(state, channel, AMQP_TX_SELECT_METHOD,
                                 AMQP_TX_SELECT_OK_METHOD, &req);
}

/**
 * amqp_tx_commit
 *
 * @param [in] state connection state
 * @param [in] channel the channel to do the RPC on
 * @returns amqp_tx_commit_ok_t
 */
AMQP_PUBLIC_FUNCTION
amqp_tx_commit_ok_t *AMQP_CALL amqp_tx_commit(amqp_connection_state_t state,
                                              amqp_channel_t channel) {
  amqp_tx_commit_t req;

  return amqp_simple_rpc_decoded(state, channel, AMQP_TX_COMMIT_METHOD,
                                 AMQP_TX_COMMIT_OK_METHOD, &req);
}

/**
 * amqp_tx_rollback
 *
 * @param [in] state connection state
 * @param [in] channel the channel to do the RPC on
 * @returns amqp_tx_rollback_ok_t
 */
AMQP_PUBLIC_FUNCTION
amqp_tx_rollback_ok_t *AMQP_CALL amqp_tx_rollback(amqp_connection_state_t state,
                                                  amqp_channel_t channel) {
  amqp_tx_rollback_t req;

  return amqp_simple_rpc_decoded(state, channel, AMQP_TX_ROLLBACK_METHOD,
                                 AMQP_TX_ROLLBACK_OK_METHOD, &req);
}

/**
 * amqp_confirm_select
 *
 * @param [in] state connection state
 * @param [in] channel the channel to do the RPC on
 * @returns amqp_confirm_select_ok_t
 */
AMQP_PUBLIC_FUNCTION
amqp_confirm_select_ok_t *AMQP_CALL
    amqp_confirm_select(amqp_connection_state_t state, amqp_channel_t channel) {
  amqp_confirm_select_t req;
  req.nowait = 0;

  return amqp_simple_rpc_decoded(state, channel, AMQP_CONFIRM_SELECT_METHOD,
                                 AMQP_CONFIRM_SELECT_OK_METHOD, &req);
}
