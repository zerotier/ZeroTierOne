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

/** @file amqp_framing.h */
#ifndef AMQP_FRAMING_H
#define AMQP_FRAMING_H

#include <amqp.h>

AMQP_BEGIN_DECLS

#define AMQP_PROTOCOL_VERSION_MAJOR 0 /**< AMQP protocol version major */
#define AMQP_PROTOCOL_VERSION_MINOR 9 /**< AMQP protocol version minor */
#define AMQP_PROTOCOL_VERSION_REVISION                                   \
  1                                  /**< AMQP protocol version revision \
                                        */
#define AMQP_PROTOCOL_PORT 5672      /**< Default AMQP Port */
#define AMQP_FRAME_METHOD 1          /**< Constant: FRAME-METHOD */
#define AMQP_FRAME_HEADER 2          /**< Constant: FRAME-HEADER */
#define AMQP_FRAME_BODY 3            /**< Constant: FRAME-BODY */
#define AMQP_FRAME_HEARTBEAT 8       /**< Constant: FRAME-HEARTBEAT */
#define AMQP_FRAME_MIN_SIZE 4096     /**< Constant: FRAME-MIN-SIZE */
#define AMQP_FRAME_END 206           /**< Constant: FRAME-END */
#define AMQP_REPLY_SUCCESS 200       /**< Constant: REPLY-SUCCESS */
#define AMQP_CONTENT_TOO_LARGE 311   /**< Constant: CONTENT-TOO-LARGE */
#define AMQP_NO_ROUTE 312            /**< Constant: NO-ROUTE */
#define AMQP_NO_CONSUMERS 313        /**< Constant: NO-CONSUMERS */
#define AMQP_ACCESS_REFUSED 403      /**< Constant: ACCESS-REFUSED */
#define AMQP_NOT_FOUND 404           /**< Constant: NOT-FOUND */
#define AMQP_RESOURCE_LOCKED 405     /**< Constant: RESOURCE-LOCKED */
#define AMQP_PRECONDITION_FAILED 406 /**< Constant: PRECONDITION-FAILED */
#define AMQP_CONNECTION_FORCED 320   /**< Constant: CONNECTION-FORCED */
#define AMQP_INVALID_PATH 402        /**< Constant: INVALID-PATH */
#define AMQP_FRAME_ERROR 501         /**< Constant: FRAME-ERROR */
#define AMQP_SYNTAX_ERROR 502        /**< Constant: SYNTAX-ERROR */
#define AMQP_COMMAND_INVALID 503     /**< Constant: COMMAND-INVALID */
#define AMQP_CHANNEL_ERROR 504       /**< Constant: CHANNEL-ERROR */
#define AMQP_UNEXPECTED_FRAME 505    /**< Constant: UNEXPECTED-FRAME */
#define AMQP_RESOURCE_ERROR 506      /**< Constant: RESOURCE-ERROR */
#define AMQP_NOT_ALLOWED 530         /**< Constant: NOT-ALLOWED */
#define AMQP_NOT_IMPLEMENTED 540     /**< Constant: NOT-IMPLEMENTED */
#define AMQP_INTERNAL_ERROR 541      /**< Constant: INTERNAL-ERROR */

/* Function prototypes. */

/**
 * Get constant name string from constant
 *
 * @param [in] constantNumber constant to get the name of
 * @returns string describing the constant. String is managed by
 *           the library and should not be free()'d by the program
 */
AMQP_PUBLIC_FUNCTION
char const *AMQP_CALL amqp_constant_name(int constantNumber);

/**
 * Checks to see if a constant is a hard error
 *
 * A hard error occurs when something severe enough
 * happens that the connection must be closed.
 *
 * @param [in] constantNumber the error constant
 * @returns true if its a hard error, false otherwise
 */
AMQP_PUBLIC_FUNCTION
amqp_boolean_t AMQP_CALL amqp_constant_is_hard_error(int constantNumber);

/**
 * Get method name string from method number
 *
 * @param [in] methodNumber the method number
 * @returns method name string. String is managed by the library
 *           and should not be freed()'d by the program
 */
AMQP_PUBLIC_FUNCTION
char const *AMQP_CALL amqp_method_name(amqp_method_number_t methodNumber);

/**
 * Check whether a method has content
 *
 * A method that has content will receive the method frame
 * a properties frame, then 1 to N body frames
 *
 * @param [in] methodNumber the method number
 * @returns true if method has content, false otherwise
 */
AMQP_PUBLIC_FUNCTION
amqp_boolean_t AMQP_CALL
    amqp_method_has_content(amqp_method_number_t methodNumber);

/**
 * Decodes a method from AMQP wireformat
 *
 * @param [in] methodNumber the method number for the decoded parameter
 * @param [in] pool the memory pool to allocate the decoded method from
 * @param [in] encoded the encoded byte string buffer
 * @param [out] decoded pointer to the decoded method struct
 * @returns 0 on success, an error code otherwise
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_decode_method(amqp_method_number_t methodNumber,
                                 amqp_pool_t *pool, amqp_bytes_t encoded,
                                 void **decoded);

/**
 * Decodes a header frame properties structure from AMQP wireformat
 *
 * @param [in] class_id the class id for the decoded parameter
 * @param [in] pool the memory pool to allocate the decoded properties from
 * @param [in] encoded the encoded byte string buffer
 * @param [out] decoded pointer to the decoded properties struct
 * @returns 0 on success, an error code otherwise
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_decode_properties(uint16_t class_id, amqp_pool_t *pool,
                                     amqp_bytes_t encoded, void **decoded);

/**
 * Encodes a method structure in AMQP wireformat
 *
 * @param [in] methodNumber the method number for the decoded parameter
 * @param [in] decoded the method structure (e.g., amqp_connection_start_t)
 * @param [in] encoded an allocated byte buffer for the encoded method
 *              structure to be written to. If the buffer isn't large enough
 *              to hold the encoded method, an error code will be returned.
 * @returns 0 on success, an error code otherwise.
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_encode_method(amqp_method_number_t methodNumber,
                                 void *decoded, amqp_bytes_t encoded);

/**
 * Encodes a properties structure in AMQP wireformat
 *
 * @param [in] class_id the class id for the decoded parameter
 * @param [in] decoded the properties structure (e.g., amqp_basic_properties_t)
 * @param [in] encoded an allocated byte buffer for the encoded properties to
 * written to.
 *              If the buffer isn't large enough to hold the encoded method, an
 *              an error code will be returned
 * @returns 0 on success, an error code otherwise.
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_encode_properties(uint16_t class_id, void *decoded,
                                     amqp_bytes_t encoded);

/* Method field records. */

#define AMQP_CONNECTION_START_METHOD                                 \
  ((amqp_method_number_t)0x000A000A) /**< connection.start method id \
                                        @internal 10, 10; 655370 */
/** connection.start method fields */
typedef struct amqp_connection_start_t_ {
  uint8_t version_major;          /**< version-major */
  uint8_t version_minor;          /**< version-minor */
  amqp_table_t server_properties; /**< server-properties */
  amqp_bytes_t mechanisms;        /**< mechanisms */
  amqp_bytes_t locales;           /**< locales */
} amqp_connection_start_t;

#define AMQP_CONNECTION_START_OK_METHOD                                 \
  ((amqp_method_number_t)0x000A000B) /**< connection.start-ok method id \
                                        @internal 10, 11; 655371 */
/** connection.start-ok method fields */
typedef struct amqp_connection_start_ok_t_ {
  amqp_table_t client_properties; /**< client-properties */
  amqp_bytes_t mechanism;         /**< mechanism */
  amqp_bytes_t response;          /**< response */
  amqp_bytes_t locale;            /**< locale */
} amqp_connection_start_ok_t;

#define AMQP_CONNECTION_SECURE_METHOD                                 \
  ((amqp_method_number_t)0x000A0014) /**< connection.secure method id \
                                        @internal 10, 20; 655380 */
/** connection.secure method fields */
typedef struct amqp_connection_secure_t_ {
  amqp_bytes_t challenge; /**< challenge */
} amqp_connection_secure_t;

#define AMQP_CONNECTION_SECURE_OK_METHOD                                 \
  ((amqp_method_number_t)0x000A0015) /**< connection.secure-ok method id \
                                        @internal 10, 21; 655381 */
/** connection.secure-ok method fields */
typedef struct amqp_connection_secure_ok_t_ {
  amqp_bytes_t response; /**< response */
} amqp_connection_secure_ok_t;

#define AMQP_CONNECTION_TUNE_METHOD                                 \
  ((amqp_method_number_t)0x000A001E) /**< connection.tune method id \
                                        @internal 10, 30; 655390 */
/** connection.tune method fields */
typedef struct amqp_connection_tune_t_ {
  uint16_t channel_max; /**< channel-max */
  uint32_t frame_max;   /**< frame-max */
  uint16_t heartbeat;   /**< heartbeat */
} amqp_connection_tune_t;

#define AMQP_CONNECTION_TUNE_OK_METHOD                                 \
  ((amqp_method_number_t)0x000A001F) /**< connection.tune-ok method id \
                                        @internal 10, 31; 655391 */
/** connection.tune-ok method fields */
typedef struct amqp_connection_tune_ok_t_ {
  uint16_t channel_max; /**< channel-max */
  uint32_t frame_max;   /**< frame-max */
  uint16_t heartbeat;   /**< heartbeat */
} amqp_connection_tune_ok_t;

#define AMQP_CONNECTION_OPEN_METHOD                                 \
  ((amqp_method_number_t)0x000A0028) /**< connection.open method id \
                                        @internal 10, 40; 655400 */
/** connection.open method fields */
typedef struct amqp_connection_open_t_ {
  amqp_bytes_t virtual_host; /**< virtual-host */
  amqp_bytes_t capabilities; /**< capabilities */
  amqp_boolean_t insist;     /**< insist */
} amqp_connection_open_t;

#define AMQP_CONNECTION_OPEN_OK_METHOD                                 \
  ((amqp_method_number_t)0x000A0029) /**< connection.open-ok method id \
                                        @internal 10, 41; 655401 */
/** connection.open-ok method fields */
typedef struct amqp_connection_open_ok_t_ {
  amqp_bytes_t known_hosts; /**< known-hosts */
} amqp_connection_open_ok_t;

#define AMQP_CONNECTION_CLOSE_METHOD                                 \
  ((amqp_method_number_t)0x000A0032) /**< connection.close method id \
                                        @internal 10, 50; 655410 */
/** connection.close method fields */
typedef struct amqp_connection_close_t_ {
  uint16_t reply_code;     /**< reply-code */
  amqp_bytes_t reply_text; /**< reply-text */
  uint16_t class_id;       /**< class-id */
  uint16_t method_id;      /**< method-id */
} amqp_connection_close_t;

#define AMQP_CONNECTION_CLOSE_OK_METHOD                                 \
  ((amqp_method_number_t)0x000A0033) /**< connection.close-ok method id \
                                        @internal 10, 51; 655411 */
/** connection.close-ok method fields */
typedef struct amqp_connection_close_ok_t_ {
  char dummy; /**< Dummy field to avoid empty struct */
} amqp_connection_close_ok_t;

#define AMQP_CONNECTION_BLOCKED_METHOD                                 \
  ((amqp_method_number_t)0x000A003C) /**< connection.blocked method id \
                                        @internal 10, 60; 655420 */
/** connection.blocked method fields */
typedef struct amqp_connection_blocked_t_ {
  amqp_bytes_t reason; /**< reason */
} amqp_connection_blocked_t;

#define AMQP_CONNECTION_UNBLOCKED_METHOD                                 \
  ((amqp_method_number_t)0x000A003D) /**< connection.unblocked method id \
                                        @internal 10, 61; 655421 */
/** connection.unblocked method fields */
typedef struct amqp_connection_unblocked_t_ {
  char dummy; /**< Dummy field to avoid empty struct */
} amqp_connection_unblocked_t;

#define AMQP_CHANNEL_OPEN_METHOD                                           \
  ((amqp_method_number_t)0x0014000A) /**< channel.open method id @internal \
                                        20, 10; 1310730 */
/** channel.open method fields */
typedef struct amqp_channel_open_t_ {
  amqp_bytes_t out_of_band; /**< out-of-band */
} amqp_channel_open_t;

#define AMQP_CHANNEL_OPEN_OK_METHOD                                 \
  ((amqp_method_number_t)0x0014000B) /**< channel.open-ok method id \
                                        @internal 20, 11; 1310731 */
/** channel.open-ok method fields */
typedef struct amqp_channel_open_ok_t_ {
  amqp_bytes_t channel_id; /**< channel-id */
} amqp_channel_open_ok_t;

#define AMQP_CHANNEL_FLOW_METHOD                                           \
  ((amqp_method_number_t)0x00140014) /**< channel.flow method id @internal \
                                        20, 20; 1310740 */
/** channel.flow method fields */
typedef struct amqp_channel_flow_t_ {
  amqp_boolean_t active; /**< active */
} amqp_channel_flow_t;

#define AMQP_CHANNEL_FLOW_OK_METHOD                                 \
  ((amqp_method_number_t)0x00140015) /**< channel.flow-ok method id \
                                        @internal 20, 21; 1310741 */
/** channel.flow-ok method fields */
typedef struct amqp_channel_flow_ok_t_ {
  amqp_boolean_t active; /**< active */
} amqp_channel_flow_ok_t;

#define AMQP_CHANNEL_CLOSE_METHOD                                           \
  ((amqp_method_number_t)0x00140028) /**< channel.close method id @internal \
                                        20, 40; 1310760 */
/** channel.close method fields */
typedef struct amqp_channel_close_t_ {
  uint16_t reply_code;     /**< reply-code */
  amqp_bytes_t reply_text; /**< reply-text */
  uint16_t class_id;       /**< class-id */
  uint16_t method_id;      /**< method-id */
} amqp_channel_close_t;

#define AMQP_CHANNEL_CLOSE_OK_METHOD                                 \
  ((amqp_method_number_t)0x00140029) /**< channel.close-ok method id \
                                        @internal 20, 41; 1310761 */
/** channel.close-ok method fields */
typedef struct amqp_channel_close_ok_t_ {
  char dummy; /**< Dummy field to avoid empty struct */
} amqp_channel_close_ok_t;

#define AMQP_ACCESS_REQUEST_METHOD                                           \
  ((amqp_method_number_t)0x001E000A) /**< access.request method id @internal \
                                        30, 10; 1966090 */
/** access.request method fields */
typedef struct amqp_access_request_t_ {
  amqp_bytes_t realm;       /**< realm */
  amqp_boolean_t exclusive; /**< exclusive */
  amqp_boolean_t passive;   /**< passive */
  amqp_boolean_t active;    /**< active */
  amqp_boolean_t write;     /**< write */
  amqp_boolean_t read;      /**< read */
} amqp_access_request_t;

#define AMQP_ACCESS_REQUEST_OK_METHOD                                 \
  ((amqp_method_number_t)0x001E000B) /**< access.request-ok method id \
                                        @internal 30, 11; 1966091 */
/** access.request-ok method fields */
typedef struct amqp_access_request_ok_t_ {
  uint16_t ticket; /**< ticket */
} amqp_access_request_ok_t;

#define AMQP_EXCHANGE_DECLARE_METHOD                                 \
  ((amqp_method_number_t)0x0028000A) /**< exchange.declare method id \
                                        @internal 40, 10; 2621450 */
/** exchange.declare method fields */
typedef struct amqp_exchange_declare_t_ {
  uint16_t ticket;            /**< ticket */
  amqp_bytes_t exchange;      /**< exchange */
  amqp_bytes_t type;          /**< type */
  amqp_boolean_t passive;     /**< passive */
  amqp_boolean_t durable;     /**< durable */
  amqp_boolean_t auto_delete; /**< auto-delete */
  amqp_boolean_t internal;    /**< internal */
  amqp_boolean_t nowait;      /**< nowait */
  amqp_table_t arguments;     /**< arguments */
} amqp_exchange_declare_t;

#define AMQP_EXCHANGE_DECLARE_OK_METHOD                                 \
  ((amqp_method_number_t)0x0028000B) /**< exchange.declare-ok method id \
                                        @internal 40, 11; 2621451 */
/** exchange.declare-ok method fields */
typedef struct amqp_exchange_declare_ok_t_ {
  char dummy; /**< Dummy field to avoid empty struct */
} amqp_exchange_declare_ok_t;

#define AMQP_EXCHANGE_DELETE_METHOD                                 \
  ((amqp_method_number_t)0x00280014) /**< exchange.delete method id \
                                        @internal 40, 20; 2621460 */
/** exchange.delete method fields */
typedef struct amqp_exchange_delete_t_ {
  uint16_t ticket;          /**< ticket */
  amqp_bytes_t exchange;    /**< exchange */
  amqp_boolean_t if_unused; /**< if-unused */
  amqp_boolean_t nowait;    /**< nowait */
} amqp_exchange_delete_t;

#define AMQP_EXCHANGE_DELETE_OK_METHOD                                 \
  ((amqp_method_number_t)0x00280015) /**< exchange.delete-ok method id \
                                        @internal 40, 21; 2621461 */
/** exchange.delete-ok method fields */
typedef struct amqp_exchange_delete_ok_t_ {
  char dummy; /**< Dummy field to avoid empty struct */
} amqp_exchange_delete_ok_t;

#define AMQP_EXCHANGE_BIND_METHOD                                           \
  ((amqp_method_number_t)0x0028001E) /**< exchange.bind method id @internal \
                                        40, 30; 2621470 */
/** exchange.bind method fields */
typedef struct amqp_exchange_bind_t_ {
  uint16_t ticket;          /**< ticket */
  amqp_bytes_t destination; /**< destination */
  amqp_bytes_t source;      /**< source */
  amqp_bytes_t routing_key; /**< routing-key */
  amqp_boolean_t nowait;    /**< nowait */
  amqp_table_t arguments;   /**< arguments */
} amqp_exchange_bind_t;

#define AMQP_EXCHANGE_BIND_OK_METHOD                                 \
  ((amqp_method_number_t)0x0028001F) /**< exchange.bind-ok method id \
                                        @internal 40, 31; 2621471 */
/** exchange.bind-ok method fields */
typedef struct amqp_exchange_bind_ok_t_ {
  char dummy; /**< Dummy field to avoid empty struct */
} amqp_exchange_bind_ok_t;

#define AMQP_EXCHANGE_UNBIND_METHOD                                 \
  ((amqp_method_number_t)0x00280028) /**< exchange.unbind method id \
                                        @internal 40, 40; 2621480 */
/** exchange.unbind method fields */
typedef struct amqp_exchange_unbind_t_ {
  uint16_t ticket;          /**< ticket */
  amqp_bytes_t destination; /**< destination */
  amqp_bytes_t source;      /**< source */
  amqp_bytes_t routing_key; /**< routing-key */
  amqp_boolean_t nowait;    /**< nowait */
  amqp_table_t arguments;   /**< arguments */
} amqp_exchange_unbind_t;

#define AMQP_EXCHANGE_UNBIND_OK_METHOD                                 \
  ((amqp_method_number_t)0x00280033) /**< exchange.unbind-ok method id \
                                        @internal 40, 51; 2621491 */
/** exchange.unbind-ok method fields */
typedef struct amqp_exchange_unbind_ok_t_ {
  char dummy; /**< Dummy field to avoid empty struct */
} amqp_exchange_unbind_ok_t;

#define AMQP_QUEUE_DECLARE_METHOD                                           \
  ((amqp_method_number_t)0x0032000A) /**< queue.declare method id @internal \
                                        50, 10; 3276810 */
/** queue.declare method fields */
typedef struct amqp_queue_declare_t_ {
  uint16_t ticket;            /**< ticket */
  amqp_bytes_t queue;         /**< queue */
  amqp_boolean_t passive;     /**< passive */
  amqp_boolean_t durable;     /**< durable */
  amqp_boolean_t exclusive;   /**< exclusive */
  amqp_boolean_t auto_delete; /**< auto-delete */
  amqp_boolean_t nowait;      /**< nowait */
  amqp_table_t arguments;     /**< arguments */
} amqp_queue_declare_t;

#define AMQP_QUEUE_DECLARE_OK_METHOD                                 \
  ((amqp_method_number_t)0x0032000B) /**< queue.declare-ok method id \
                                        @internal 50, 11; 3276811 */
/** queue.declare-ok method fields */
typedef struct amqp_queue_declare_ok_t_ {
  amqp_bytes_t queue;      /**< queue */
  uint32_t message_count;  /**< message-count */
  uint32_t consumer_count; /**< consumer-count */
} amqp_queue_declare_ok_t;

#define AMQP_QUEUE_BIND_METHOD                                               \
  ((amqp_method_number_t)0x00320014) /**< queue.bind method id @internal 50, \
                                        20; 3276820 */
/** queue.bind method fields */
typedef struct amqp_queue_bind_t_ {
  uint16_t ticket;          /**< ticket */
  amqp_bytes_t queue;       /**< queue */
  amqp_bytes_t exchange;    /**< exchange */
  amqp_bytes_t routing_key; /**< routing-key */
  amqp_boolean_t nowait;    /**< nowait */
  amqp_table_t arguments;   /**< arguments */
} amqp_queue_bind_t;

#define AMQP_QUEUE_BIND_OK_METHOD                                           \
  ((amqp_method_number_t)0x00320015) /**< queue.bind-ok method id @internal \
                                        50, 21; 3276821 */
/** queue.bind-ok method fields */
typedef struct amqp_queue_bind_ok_t_ {
  char dummy; /**< Dummy field to avoid empty struct */
} amqp_queue_bind_ok_t;

#define AMQP_QUEUE_PURGE_METHOD                                           \
  ((amqp_method_number_t)0x0032001E) /**< queue.purge method id @internal \
                                        50, 30; 3276830 */
/** queue.purge method fields */
typedef struct amqp_queue_purge_t_ {
  uint16_t ticket;       /**< ticket */
  amqp_bytes_t queue;    /**< queue */
  amqp_boolean_t nowait; /**< nowait */
} amqp_queue_purge_t;

#define AMQP_QUEUE_PURGE_OK_METHOD                                           \
  ((amqp_method_number_t)0x0032001F) /**< queue.purge-ok method id @internal \
                                        50, 31; 3276831 */
/** queue.purge-ok method fields */
typedef struct amqp_queue_purge_ok_t_ {
  uint32_t message_count; /**< message-count */
} amqp_queue_purge_ok_t;

#define AMQP_QUEUE_DELETE_METHOD                                           \
  ((amqp_method_number_t)0x00320028) /**< queue.delete method id @internal \
                                        50, 40; 3276840 */
/** queue.delete method fields */
typedef struct amqp_queue_delete_t_ {
  uint16_t ticket;          /**< ticket */
  amqp_bytes_t queue;       /**< queue */
  amqp_boolean_t if_unused; /**< if-unused */
  amqp_boolean_t if_empty;  /**< if-empty */
  amqp_boolean_t nowait;    /**< nowait */
} amqp_queue_delete_t;

#define AMQP_QUEUE_DELETE_OK_METHOD                                 \
  ((amqp_method_number_t)0x00320029) /**< queue.delete-ok method id \
                                        @internal 50, 41; 3276841 */
/** queue.delete-ok method fields */
typedef struct amqp_queue_delete_ok_t_ {
  uint32_t message_count; /**< message-count */
} amqp_queue_delete_ok_t;

#define AMQP_QUEUE_UNBIND_METHOD                                           \
  ((amqp_method_number_t)0x00320032) /**< queue.unbind method id @internal \
                                        50, 50; 3276850 */
/** queue.unbind method fields */
typedef struct amqp_queue_unbind_t_ {
  uint16_t ticket;          /**< ticket */
  amqp_bytes_t queue;       /**< queue */
  amqp_bytes_t exchange;    /**< exchange */
  amqp_bytes_t routing_key; /**< routing-key */
  amqp_table_t arguments;   /**< arguments */
} amqp_queue_unbind_t;

#define AMQP_QUEUE_UNBIND_OK_METHOD                                 \
  ((amqp_method_number_t)0x00320033) /**< queue.unbind-ok method id \
                                        @internal 50, 51; 3276851 */
/** queue.unbind-ok method fields */
typedef struct amqp_queue_unbind_ok_t_ {
  char dummy; /**< Dummy field to avoid empty struct */
} amqp_queue_unbind_ok_t;

#define AMQP_BASIC_QOS_METHOD                                               \
  ((amqp_method_number_t)0x003C000A) /**< basic.qos method id @internal 60, \
                                        10; 3932170 */
/** basic.qos method fields */
typedef struct amqp_basic_qos_t_ {
  uint32_t prefetch_size;  /**< prefetch-size */
  uint16_t prefetch_count; /**< prefetch-count */
  amqp_boolean_t global;   /**< global */
} amqp_basic_qos_t;

#define AMQP_BASIC_QOS_OK_METHOD                                           \
  ((amqp_method_number_t)0x003C000B) /**< basic.qos-ok method id @internal \
                                        60, 11; 3932171 */
/** basic.qos-ok method fields */
typedef struct amqp_basic_qos_ok_t_ {
  char dummy; /**< Dummy field to avoid empty struct */
} amqp_basic_qos_ok_t;

#define AMQP_BASIC_CONSUME_METHOD                                           \
  ((amqp_method_number_t)0x003C0014) /**< basic.consume method id @internal \
                                        60, 20; 3932180 */
/** basic.consume method fields */
typedef struct amqp_basic_consume_t_ {
  uint16_t ticket;           /**< ticket */
  amqp_bytes_t queue;        /**< queue */
  amqp_bytes_t consumer_tag; /**< consumer-tag */
  amqp_boolean_t no_local;   /**< no-local */
  amqp_boolean_t no_ack;     /**< no-ack */
  amqp_boolean_t exclusive;  /**< exclusive */
  amqp_boolean_t nowait;     /**< nowait */
  amqp_table_t arguments;    /**< arguments */
} amqp_basic_consume_t;

#define AMQP_BASIC_CONSUME_OK_METHOD                                 \
  ((amqp_method_number_t)0x003C0015) /**< basic.consume-ok method id \
                                        @internal 60, 21; 3932181 */
/** basic.consume-ok method fields */
typedef struct amqp_basic_consume_ok_t_ {
  amqp_bytes_t consumer_tag; /**< consumer-tag */
} amqp_basic_consume_ok_t;

#define AMQP_BASIC_CANCEL_METHOD                                           \
  ((amqp_method_number_t)0x003C001E) /**< basic.cancel method id @internal \
                                        60, 30; 3932190 */
/** basic.cancel method fields */
typedef struct amqp_basic_cancel_t_ {
  amqp_bytes_t consumer_tag; /**< consumer-tag */
  amqp_boolean_t nowait;     /**< nowait */
} amqp_basic_cancel_t;

#define AMQP_BASIC_CANCEL_OK_METHOD                                 \
  ((amqp_method_number_t)0x003C001F) /**< basic.cancel-ok method id \
                                        @internal 60, 31; 3932191 */
/** basic.cancel-ok method fields */
typedef struct amqp_basic_cancel_ok_t_ {
  amqp_bytes_t consumer_tag; /**< consumer-tag */
} amqp_basic_cancel_ok_t;

#define AMQP_BASIC_PUBLISH_METHOD                                           \
  ((amqp_method_number_t)0x003C0028) /**< basic.publish method id @internal \
                                        60, 40; 3932200 */
/** basic.publish method fields */
typedef struct amqp_basic_publish_t_ {
  uint16_t ticket;          /**< ticket */
  amqp_bytes_t exchange;    /**< exchange */
  amqp_bytes_t routing_key; /**< routing-key */
  amqp_boolean_t mandatory; /**< mandatory */
  amqp_boolean_t immediate; /**< immediate */
} amqp_basic_publish_t;

#define AMQP_BASIC_RETURN_METHOD                                           \
  ((amqp_method_number_t)0x003C0032) /**< basic.return method id @internal \
                                        60, 50; 3932210 */
/** basic.return method fields */
typedef struct amqp_basic_return_t_ {
  uint16_t reply_code;      /**< reply-code */
  amqp_bytes_t reply_text;  /**< reply-text */
  amqp_bytes_t exchange;    /**< exchange */
  amqp_bytes_t routing_key; /**< routing-key */
} amqp_basic_return_t;

#define AMQP_BASIC_DELIVER_METHOD                                           \
  ((amqp_method_number_t)0x003C003C) /**< basic.deliver method id @internal \
                                        60, 60; 3932220 */
/** basic.deliver method fields */
typedef struct amqp_basic_deliver_t_ {
  amqp_bytes_t consumer_tag;  /**< consumer-tag */
  uint64_t delivery_tag;      /**< delivery-tag */
  amqp_boolean_t redelivered; /**< redelivered */
  amqp_bytes_t exchange;      /**< exchange */
  amqp_bytes_t routing_key;   /**< routing-key */
} amqp_basic_deliver_t;

#define AMQP_BASIC_GET_METHOD                                               \
  ((amqp_method_number_t)0x003C0046) /**< basic.get method id @internal 60, \
                                        70; 3932230 */
/** basic.get method fields */
typedef struct amqp_basic_get_t_ {
  uint16_t ticket;       /**< ticket */
  amqp_bytes_t queue;    /**< queue */
  amqp_boolean_t no_ack; /**< no-ack */
} amqp_basic_get_t;

#define AMQP_BASIC_GET_OK_METHOD                                           \
  ((amqp_method_number_t)0x003C0047) /**< basic.get-ok method id @internal \
                                        60, 71; 3932231 */
/** basic.get-ok method fields */
typedef struct amqp_basic_get_ok_t_ {
  uint64_t delivery_tag;      /**< delivery-tag */
  amqp_boolean_t redelivered; /**< redelivered */
  amqp_bytes_t exchange;      /**< exchange */
  amqp_bytes_t routing_key;   /**< routing-key */
  uint32_t message_count;     /**< message-count */
} amqp_basic_get_ok_t;

#define AMQP_BASIC_GET_EMPTY_METHOD                                 \
  ((amqp_method_number_t)0x003C0048) /**< basic.get-empty method id \
                                        @internal 60, 72; 3932232 */
/** basic.get-empty method fields */
typedef struct amqp_basic_get_empty_t_ {
  amqp_bytes_t cluster_id; /**< cluster-id */
} amqp_basic_get_empty_t;

#define AMQP_BASIC_ACK_METHOD                                               \
  ((amqp_method_number_t)0x003C0050) /**< basic.ack method id @internal 60, \
                                        80; 3932240 */
/** basic.ack method fields */
typedef struct amqp_basic_ack_t_ {
  uint64_t delivery_tag;   /**< delivery-tag */
  amqp_boolean_t multiple; /**< multiple */
} amqp_basic_ack_t;

#define AMQP_BASIC_REJECT_METHOD                                           \
  ((amqp_method_number_t)0x003C005A) /**< basic.reject method id @internal \
                                        60, 90; 3932250 */
/** basic.reject method fields */
typedef struct amqp_basic_reject_t_ {
  uint64_t delivery_tag;  /**< delivery-tag */
  amqp_boolean_t requeue; /**< requeue */
} amqp_basic_reject_t;

#define AMQP_BASIC_RECOVER_ASYNC_METHOD                                 \
  ((amqp_method_number_t)0x003C0064) /**< basic.recover-async method id \
                                        @internal 60, 100; 3932260 */
/** basic.recover-async method fields */
typedef struct amqp_basic_recover_async_t_ {
  amqp_boolean_t requeue; /**< requeue */
} amqp_basic_recover_async_t;

#define AMQP_BASIC_RECOVER_METHOD                                           \
  ((amqp_method_number_t)0x003C006E) /**< basic.recover method id @internal \
                                        60, 110; 3932270 */
/** basic.recover method fields */
typedef struct amqp_basic_recover_t_ {
  amqp_boolean_t requeue; /**< requeue */
} amqp_basic_recover_t;

#define AMQP_BASIC_RECOVER_OK_METHOD                                 \
  ((amqp_method_number_t)0x003C006F) /**< basic.recover-ok method id \
                                        @internal 60, 111; 3932271 */
/** basic.recover-ok method fields */
typedef struct amqp_basic_recover_ok_t_ {
  char dummy; /**< Dummy field to avoid empty struct */
} amqp_basic_recover_ok_t;

#define AMQP_BASIC_NACK_METHOD                                               \
  ((amqp_method_number_t)0x003C0078) /**< basic.nack method id @internal 60, \
                                        120; 3932280 */
/** basic.nack method fields */
typedef struct amqp_basic_nack_t_ {
  uint64_t delivery_tag;   /**< delivery-tag */
  amqp_boolean_t multiple; /**< multiple */
  amqp_boolean_t requeue;  /**< requeue */
} amqp_basic_nack_t;

#define AMQP_TX_SELECT_METHOD                                               \
  ((amqp_method_number_t)0x005A000A) /**< tx.select method id @internal 90, \
                                        10; 5898250 */
/** tx.select method fields */
typedef struct amqp_tx_select_t_ {
  char dummy; /**< Dummy field to avoid empty struct */
} amqp_tx_select_t;

#define AMQP_TX_SELECT_OK_METHOD                                           \
  ((amqp_method_number_t)0x005A000B) /**< tx.select-ok method id @internal \
                                        90, 11; 5898251 */
/** tx.select-ok method fields */
typedef struct amqp_tx_select_ok_t_ {
  char dummy; /**< Dummy field to avoid empty struct */
} amqp_tx_select_ok_t;

#define AMQP_TX_COMMIT_METHOD                                               \
  ((amqp_method_number_t)0x005A0014) /**< tx.commit method id @internal 90, \
                                        20; 5898260 */
/** tx.commit method fields */
typedef struct amqp_tx_commit_t_ {
  char dummy; /**< Dummy field to avoid empty struct */
} amqp_tx_commit_t;

#define AMQP_TX_COMMIT_OK_METHOD                                           \
  ((amqp_method_number_t)0x005A0015) /**< tx.commit-ok method id @internal \
                                        90, 21; 5898261 */
/** tx.commit-ok method fields */
typedef struct amqp_tx_commit_ok_t_ {
  char dummy; /**< Dummy field to avoid empty struct */
} amqp_tx_commit_ok_t;

#define AMQP_TX_ROLLBACK_METHOD                                           \
  ((amqp_method_number_t)0x005A001E) /**< tx.rollback method id @internal \
                                        90, 30; 5898270 */
/** tx.rollback method fields */
typedef struct amqp_tx_rollback_t_ {
  char dummy; /**< Dummy field to avoid empty struct */
} amqp_tx_rollback_t;

#define AMQP_TX_ROLLBACK_OK_METHOD                                           \
  ((amqp_method_number_t)0x005A001F) /**< tx.rollback-ok method id @internal \
                                        90, 31; 5898271 */
/** tx.rollback-ok method fields */
typedef struct amqp_tx_rollback_ok_t_ {
  char dummy; /**< Dummy field to avoid empty struct */
} amqp_tx_rollback_ok_t;

#define AMQP_CONFIRM_SELECT_METHOD                                           \
  ((amqp_method_number_t)0x0055000A) /**< confirm.select method id @internal \
                                        85, 10; 5570570 */
/** confirm.select method fields */
typedef struct amqp_confirm_select_t_ {
  amqp_boolean_t nowait; /**< nowait */
} amqp_confirm_select_t;

#define AMQP_CONFIRM_SELECT_OK_METHOD                                 \
  ((amqp_method_number_t)0x0055000B) /**< confirm.select-ok method id \
                                        @internal 85, 11; 5570571 */
/** confirm.select-ok method fields */
typedef struct amqp_confirm_select_ok_t_ {
  char dummy; /**< Dummy field to avoid empty struct */
} amqp_confirm_select_ok_t;

/* Class property records. */
#define AMQP_CONNECTION_CLASS                    \
  (0x000A) /**< connection class id @internal 10 \
              */
/** connection class properties */
typedef struct amqp_connection_properties_t_ {
  amqp_flags_t _flags; /**< bit-mask of set fields */
  char dummy;          /**< Dummy field to avoid empty struct */
} amqp_connection_properties_t;

#define AMQP_CHANNEL_CLASS (0x0014) /**< channel class id @internal 20 */
/** channel class properties */
typedef struct amqp_channel_properties_t_ {
  amqp_flags_t _flags; /**< bit-mask of set fields */
  char dummy;          /**< Dummy field to avoid empty struct */
} amqp_channel_properties_t;

#define AMQP_ACCESS_CLASS (0x001E) /**< access class id @internal 30 */
/** access class properties */
typedef struct amqp_access_properties_t_ {
  amqp_flags_t _flags; /**< bit-mask of set fields */
  char dummy;          /**< Dummy field to avoid empty struct */
} amqp_access_properties_t;

#define AMQP_EXCHANGE_CLASS (0x0028) /**< exchange class id @internal 40 */
/** exchange class properties */
typedef struct amqp_exchange_properties_t_ {
  amqp_flags_t _flags; /**< bit-mask of set fields */
  char dummy;          /**< Dummy field to avoid empty struct */
} amqp_exchange_properties_t;

#define AMQP_QUEUE_CLASS (0x0032) /**< queue class id @internal 50 */
/** queue class properties */
typedef struct amqp_queue_properties_t_ {
  amqp_flags_t _flags; /**< bit-mask of set fields */
  char dummy;          /**< Dummy field to avoid empty struct */
} amqp_queue_properties_t;

#define AMQP_BASIC_CLASS (0x003C) /**< basic class id @internal 60 */
#define AMQP_BASIC_CONTENT_TYPE_FLAG (1 << 15)
#define AMQP_BASIC_CONTENT_ENCODING_FLAG (1 << 14)
#define AMQP_BASIC_HEADERS_FLAG (1 << 13)
#define AMQP_BASIC_DELIVERY_MODE_FLAG (1 << 12)
#define AMQP_BASIC_PRIORITY_FLAG (1 << 11)
#define AMQP_BASIC_CORRELATION_ID_FLAG (1 << 10)
#define AMQP_BASIC_REPLY_TO_FLAG (1 << 9)
#define AMQP_BASIC_EXPIRATION_FLAG (1 << 8)
#define AMQP_BASIC_MESSAGE_ID_FLAG (1 << 7)
#define AMQP_BASIC_TIMESTAMP_FLAG (1 << 6)
#define AMQP_BASIC_TYPE_FLAG (1 << 5)
#define AMQP_BASIC_USER_ID_FLAG (1 << 4)
#define AMQP_BASIC_APP_ID_FLAG (1 << 3)
#define AMQP_BASIC_CLUSTER_ID_FLAG (1 << 2)
/** basic class properties */
typedef struct amqp_basic_properties_t_ {
  amqp_flags_t _flags;           /**< bit-mask of set fields */
  amqp_bytes_t content_type;     /**< content-type */
  amqp_bytes_t content_encoding; /**< content-encoding */
  amqp_table_t headers;          /**< headers */
  uint8_t delivery_mode;         /**< delivery-mode */
  uint8_t priority;              /**< priority */
  amqp_bytes_t correlation_id;   /**< correlation-id */
  amqp_bytes_t reply_to;         /**< reply-to */
  amqp_bytes_t expiration;       /**< expiration */
  amqp_bytes_t message_id;       /**< message-id */
  uint64_t timestamp;            /**< timestamp */
  amqp_bytes_t type;             /**< type */
  amqp_bytes_t user_id;          /**< user-id */
  amqp_bytes_t app_id;           /**< app-id */
  amqp_bytes_t cluster_id;       /**< cluster-id */
} amqp_basic_properties_t;

#define AMQP_TX_CLASS (0x005A) /**< tx class id @internal 90 */
/** tx class properties */
typedef struct amqp_tx_properties_t_ {
  amqp_flags_t _flags; /**< bit-mask of set fields */
  char dummy;          /**< Dummy field to avoid empty struct */
} amqp_tx_properties_t;

#define AMQP_CONFIRM_CLASS (0x0055) /**< confirm class id @internal 85 */
/** confirm class properties */
typedef struct amqp_confirm_properties_t_ {
  amqp_flags_t _flags; /**< bit-mask of set fields */
  char dummy;          /**< Dummy field to avoid empty struct */
} amqp_confirm_properties_t;

/* API functions for methods */

/**
 * amqp_channel_open
 *
 * @param [in] state connection state
 * @param [in] channel the channel to do the RPC on
 * @returns amqp_channel_open_ok_t
 */
AMQP_PUBLIC_FUNCTION
amqp_channel_open_ok_t *AMQP_CALL
    amqp_channel_open(amqp_connection_state_t state, amqp_channel_t channel);
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
                      amqp_boolean_t active);
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
    amqp_table_t arguments);
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
                         amqp_bytes_t exchange, amqp_boolean_t if_unused);
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
                       amqp_bytes_t routing_key, amqp_table_t arguments);
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
                         amqp_bytes_t routing_key, amqp_table_t arguments);
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
    amqp_boolean_t auto_delete, amqp_table_t arguments);
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
    amqp_bytes_t exchange, amqp_bytes_t routing_key, amqp_table_t arguments);
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
                                                  amqp_bytes_t queue);
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
    amqp_boolean_t if_unused, amqp_boolean_t if_empty);
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
    amqp_bytes_t exchange, amqp_bytes_t routing_key, amqp_table_t arguments);
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
                                              amqp_boolean_t global);
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
    amqp_boolean_t exclusive, amqp_table_t arguments);
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
                      amqp_bytes_t consumer_tag);
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
                       amqp_boolean_t requeue);
/**
 * amqp_tx_select
 *
 * @param [in] state connection state
 * @param [in] channel the channel to do the RPC on
 * @returns amqp_tx_select_ok_t
 */
AMQP_PUBLIC_FUNCTION
amqp_tx_select_ok_t *AMQP_CALL amqp_tx_select(amqp_connection_state_t state,
                                              amqp_channel_t channel);
/**
 * amqp_tx_commit
 *
 * @param [in] state connection state
 * @param [in] channel the channel to do the RPC on
 * @returns amqp_tx_commit_ok_t
 */
AMQP_PUBLIC_FUNCTION
amqp_tx_commit_ok_t *AMQP_CALL amqp_tx_commit(amqp_connection_state_t state,
                                              amqp_channel_t channel);
/**
 * amqp_tx_rollback
 *
 * @param [in] state connection state
 * @param [in] channel the channel to do the RPC on
 * @returns amqp_tx_rollback_ok_t
 */
AMQP_PUBLIC_FUNCTION
amqp_tx_rollback_ok_t *AMQP_CALL amqp_tx_rollback(amqp_connection_state_t state,
                                                  amqp_channel_t channel);
/**
 * amqp_confirm_select
 *
 * @param [in] state connection state
 * @param [in] channel the channel to do the RPC on
 * @returns amqp_confirm_select_ok_t
 */
AMQP_PUBLIC_FUNCTION
amqp_confirm_select_ok_t *AMQP_CALL
    amqp_confirm_select(amqp_connection_state_t state, amqp_channel_t channel);

AMQP_END_DECLS

#endif /* AMQP_FRAMING_H */
