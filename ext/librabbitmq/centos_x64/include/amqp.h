/** \file */
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

#ifndef AMQP_H
#define AMQP_H

/** \cond HIDE_FROM_DOXYGEN */

#ifdef __cplusplus
#define AMQP_BEGIN_DECLS extern "C" {
#define AMQP_END_DECLS }
#else
#define AMQP_BEGIN_DECLS
#define AMQP_END_DECLS
#endif

/*
 * \internal
 * Important API decorators:
 *  AMQP_PUBLIC_FUNCTION - a public API function
 *  AMQP_PUBLIC_VARIABLE - a public API external variable
 *  AMQP_CALL - calling convension (used on Win32)
 */

#if defined(_WIN32) && defined(_MSC_VER)
#if defined(AMQP_BUILD) && !defined(AMQP_STATIC)
#define AMQP_PUBLIC_FUNCTION __declspec(dllexport)
#define AMQP_PUBLIC_VARIABLE __declspec(dllexport) extern
#else
#define AMQP_PUBLIC_FUNCTION
#if !defined(AMQP_STATIC)
#define AMQP_PUBLIC_VARIABLE __declspec(dllimport) extern
#else
#define AMQP_PUBLIC_VARIABLE extern
#endif
#endif
#define AMQP_CALL __cdecl

#elif defined(_WIN32) && defined(__BORLANDC__)
#if defined(AMQP_BUILD) && !defined(AMQP_STATIC)
#define AMQP_PUBLIC_FUNCTION __declspec(dllexport)
#define AMQP_PUBLIC_VARIABLE __declspec(dllexport) extern
#else
#define AMQP_PUBLIC_FUNCTION
#if !defined(AMQP_STATIC)
#define AMQP_PUBLIC_VARIABLE __declspec(dllimport) extern
#else
#define AMQP_PUBLIC_VARIABLE extern
#endif
#endif
#define AMQP_CALL __cdecl

#elif defined(_WIN32) && defined(__MINGW32__)
#if defined(AMQP_BUILD) && !defined(AMQP_STATIC)
#define AMQP_PUBLIC_FUNCTION __declspec(dllexport)
#define AMQP_PUBLIC_VARIABLE __declspec(dllexport) extern
#else
#define AMQP_PUBLIC_FUNCTION
#if !defined(AMQP_STATIC)
#define AMQP_PUBLIC_VARIABLE __declspec(dllimport) extern
#else
#define AMQP_PUBLIC_VARIABLE extern
#endif
#endif
#define AMQP_CALL __cdecl

#elif defined(_WIN32) && defined(__CYGWIN__)
#if defined(AMQP_BUILD) && !defined(AMQP_STATIC)
#define AMQP_PUBLIC_FUNCTION __declspec(dllexport)
#define AMQP_PUBLIC_VARIABLE __declspec(dllexport)
#else
#define AMQP_PUBLIC_FUNCTION
#if !defined(AMQP_STATIC)
#define AMQP_PUBLIC_VARIABLE __declspec(dllimport) extern
#else
#define AMQP_PUBLIC_VARIABLE extern
#endif
#endif
#define AMQP_CALL __cdecl

#elif defined(__GNUC__) && __GNUC__ >= 4
#define AMQP_PUBLIC_FUNCTION __attribute__((visibility("default")))
#define AMQP_PUBLIC_VARIABLE __attribute__((visibility("default"))) extern
#define AMQP_CALL
#else
#define AMQP_PUBLIC_FUNCTION
#define AMQP_PUBLIC_VARIABLE extern
#define AMQP_CALL
#endif

#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1)
#define AMQP_DEPRECATED(function) function __attribute__((__deprecated__))
#elif defined(_MSC_VER)
#define AMQP_DEPRECATED(function) __declspec(deprecated) function
#else
#define AMQP_DEPRECATED(function)
#endif

/* Define ssize_t on Win32/64 platforms
   See: http://lists.cs.uiuc.edu/pipermail/llvmdev/2010-April/030649.html for
   details
   */
#if !defined(_W64)
#if !defined(__midl) && (defined(_X86_) || defined(_M_IX86)) && _MSC_VER >= 1300
#define _W64 __w64
#else
#define _W64
#endif
#endif

#ifdef _MSC_VER
#ifdef _WIN64
typedef __int64 ssize_t;
#else
typedef _W64 int ssize_t;
#endif
#endif

#if defined(_WIN32) && defined(__MINGW32__)
#include <sys/types.h>
#endif

/** \endcond */

#include <stddef.h>
#include <stdint.h>

struct timeval;

AMQP_BEGIN_DECLS

/**
 * \def AMQP_VERSION_MAJOR
 *
 * Major library version number compile-time constant
 *
 * The major version is incremented when backwards incompatible API changes
 * are made.
 *
 * \sa AMQP_VERSION, AMQP_VERSION_STRING
 *
 * \since v0.4.0
 */

/**
 * \def AMQP_VERSION_MINOR
 *
 * Minor library version number compile-time constant
 *
 * The minor version is incremented when new APIs are added. Existing APIs
 * are left alone.
 *
 * \sa AMQP_VERSION, AMQP_VERSION_STRING
 *
 * \since v0.4.0
 */

/**
 * \def AMQP_VERSION_PATCH
 *
 * Patch library version number compile-time constant
 *
 * The patch version is incremented when library code changes, but the API
 * is not changed.
 *
 * \sa AMQP_VERSION, AMQP_VERSION_STRING
 *
 * \since v0.4.0
 */

/**
 * \def AMQP_VERSION_IS_RELEASE
 *
 * Version constant set to 1 for tagged release, 0 otherwise
 *
 * NOTE: versions that are not tagged releases are not guaranteed to be API/ABI
 * compatible with older releases, and may change commit-to-commit.
 *
 * \sa AMQP_VERSION, AMQP_VERSION_STRING
 *
 * \since v0.4.0
 */
/*
 * Developer note: when changing these, be sure to update SOVERSION constants
 *  in CMakeLists.txt and configure.ac
 */

#define AMQP_VERSION_MAJOR 0
#define AMQP_VERSION_MINOR 10
#define AMQP_VERSION_PATCH 0
#define AMQP_VERSION_IS_RELEASE 0

/**
 * \def AMQP_VERSION_CODE
 *
 * Helper macro to geneate a packed version code suitable for
 * comparison with AMQP_VERSION.
 *
 * \sa amqp_version_number() AMQP_VERSION_MAJOR, AMQP_VERSION_MINOR,
 *     AMQP_VERSION_PATCH, AMQP_VERSION_IS_RELEASE, AMQP_VERSION
 *
 * \since v0.6.1
 */
#define AMQP_VERSION_CODE(major, minor, patch, release) \
  ((major << 24) | (minor << 16) | (patch << 8) | (release))

/**
 * \def AMQP_VERSION
 *
 * Packed version number
 *
 * AMQP_VERSION is a 4-byte unsigned integer with the most significant byte
 * set to AMQP_VERSION_MAJOR, the second most significant byte set to
 * AMQP_VERSION_MINOR, third most significant byte set to AMQP_VERSION_PATCH,
 * and the lowest byte set to AMQP_VERSION_IS_RELEASE.
 *
 * For example version 2.3.4 which is released version would be encoded as
 * 0x02030401
 *
 * \sa amqp_version_number() AMQP_VERSION_MAJOR, AMQP_VERSION_MINOR,
 *     AMQP_VERSION_PATCH, AMQP_VERSION_IS_RELEASE, AMQP_VERSION_CODE
 *
 * \since v0.4.0
 */
#define AMQP_VERSION                                        \
  AMQP_VERSION_CODE(AMQP_VERSION_MAJOR, AMQP_VERSION_MINOR, \
                    AMQP_VERSION_PATCH, AMQP_VERSION_IS_RELEASE)

/** \cond HIDE_FROM_DOXYGEN */
#define AMQ_STRINGIFY(s) AMQ_STRINGIFY_HELPER(s)
#define AMQ_STRINGIFY_HELPER(s) #s

#define AMQ_VERSION_STRING          \
  AMQ_STRINGIFY(AMQP_VERSION_MAJOR) \
  "." AMQ_STRINGIFY(AMQP_VERSION_MINOR) "." AMQ_STRINGIFY(AMQP_VERSION_PATCH)
/** \endcond */

/**
 * \def AMQP_VERSION_STRING
 *
 * Version string compile-time constant
 *
 * Non-released versions of the library will have "-pre" appended to the
 * version string
 *
 * \sa amqp_version()
 *
 * \since v0.4.0
 */
#if AMQP_VERSION_IS_RELEASE
#define AMQP_VERSION_STRING AMQ_VERSION_STRING
#else
#define AMQP_VERSION_STRING AMQ_VERSION_STRING "-pre"
#endif

/**
 * Returns the rabbitmq-c version as a packed integer.
 *
 * See \ref AMQP_VERSION
 *
 * \return packed 32-bit integer representing version of library at runtime
 *
 * \sa AMQP_VERSION, amqp_version()
 *
 * \since v0.4.0
 */
AMQP_PUBLIC_FUNCTION
uint32_t AMQP_CALL amqp_version_number(void);

/**
 * Returns the rabbitmq-c version as a string.
 *
 * See \ref AMQP_VERSION_STRING
 *
 * \return a statically allocated string describing the version of rabbitmq-c.
 *
 * \sa amqp_version_number(), AMQP_VERSION_STRING, AMQP_VERSION
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
char const *AMQP_CALL amqp_version(void);

/**
 * \def AMQP_DEFAULT_FRAME_SIZE
 *
 * Default frame size (128Kb)
 *
 * \sa amqp_login(), amqp_login_with_properties()
 *
 * \since v0.4.0
 */
#define AMQP_DEFAULT_FRAME_SIZE 131072

/**
 * \def AMQP_DEFAULT_MAX_CHANNELS
 *
 * Default maximum number of channels (2047, RabbitMQ default limit of 2048,
 * minus 1 for channel 0). RabbitMQ set a default limit of 2048 channels per
 * connection in v3.7.5 to prevent broken clients from leaking too many
 * channels.
 *
 * \sa amqp_login(), amqp_login_with_properties()
 *
 * \since v0.4.0
 */
#define AMQP_DEFAULT_MAX_CHANNELS 2047

/**
 * \def AMQP_DEFAULT_HEARTBEAT
 *
 * Default heartbeat interval (0, heartbeat disabled)
 *
 * \sa amqp_login(), amqp_login_with_properties()
 *
 * \since v0.4.0
 */
#define AMQP_DEFAULT_HEARTBEAT 0

/**
 * \def AMQP_DEFAULT_VHOST
 *
 * Default RabbitMQ vhost: "/"
 *
 * \sa amqp_login(), amqp_login_with_properties()
 *
 * \since v0.9.0
 */
#define AMQP_DEFAULT_VHOST "/"

/**
 * boolean type 0 = false, true otherwise
 *
 * \since v0.1
 */
typedef int amqp_boolean_t;

/**
 * Method number
 *
 * \since v0.1
 */
typedef uint32_t amqp_method_number_t;

/**
 * Bitmask for flags
 *
 * \since v0.1
 */
typedef uint32_t amqp_flags_t;

/**
 * Channel type
 *
 * \since v0.1
 */
typedef uint16_t amqp_channel_t;

/**
 * Buffer descriptor
 *
 * \since v0.1
 */
typedef struct amqp_bytes_t_ {
  size_t len;  /**< length of the buffer in bytes */
  void *bytes; /**< pointer to the beginning of the buffer */
} amqp_bytes_t;

/**
 * Decimal data type
 *
 * \since v0.1
 */
typedef struct amqp_decimal_t_ {
  uint8_t decimals; /**< the location of the decimal point */
  uint32_t value;   /**< the value before the decimal point is applied */
} amqp_decimal_t;

/**
 * AMQP field table
 *
 * An AMQP field table is a set of key-value pairs.
 * A key is a UTF-8 encoded string up to 128 bytes long, and are not null
 * terminated.
 * A value can be one of several different datatypes. \sa
 * amqp_field_value_kind_t
 *
 * \sa amqp_table_entry_t
 *
 * \since v0.1
 */
typedef struct amqp_table_t_ {
  int num_entries;                     /**< length of entries array */
  struct amqp_table_entry_t_ *entries; /**< an array of table entries */
} amqp_table_t;

/**
 * An AMQP Field Array
 *
 * A repeated set of field values, all must be of the same type
 *
 * \since v0.1
 */
typedef struct amqp_array_t_ {
  int num_entries;                     /**< Number of entries in the table */
  struct amqp_field_value_t_ *entries; /**< linked list of field values */
} amqp_array_t;

/*
  0-9   0-9-1   Qpid/Rabbit  Type               Remarks
---------------------------------------------------------------------------
        t       t            Boolean
        b       b            Signed 8-bit
        B                    Unsigned 8-bit
        U       s            Signed 16-bit      (A1)
        u                    Unsigned 16-bit
  I     I       I            Signed 32-bit
        i                    Unsigned 32-bit
        L       l            Signed 64-bit      (B)
        l                    Unsigned 64-bit
        f       f            32-bit float
        d       d            64-bit float
  D     D       D            Decimal
        s                    Short string       (A2)
  S     S       S            Long string
        A                    Nested Array
  T     T       T            Timestamp (u64)
  F     F       F            Nested Table
  V     V       V            Void
                x            Byte array

Remarks:

 A1, A2: Notice how the types **CONFLICT** here. In Qpid and Rabbit,
         's' means a signed 16-bit integer; in 0-9-1, it means a
          short string.

 B: Notice how the signednesses **CONFLICT** here. In Qpid and Rabbit,
    'l' means a signed 64-bit integer; in 0-9-1, it means an unsigned
    64-bit integer.

I'm going with the Qpid/Rabbit types, where there's a conflict, and
the 0-9-1 types otherwise. 0-8 is a subset of 0-9, which is a subset
of the other two, so this will work for both 0-8 and 0-9-1 branches of
the code.
*/

/**
 * A field table value
 *
 * \since v0.1
 */
typedef struct amqp_field_value_t_ {
  uint8_t kind; /**< the type of the entry /sa amqp_field_value_kind_t */
  union {
    amqp_boolean_t boolean; /**< boolean type AMQP_FIELD_KIND_BOOLEAN */
    int8_t i8;              /**< int8_t type AMQP_FIELD_KIND_I8 */
    uint8_t u8;             /**< uint8_t type AMQP_FIELD_KIND_U8 */
    int16_t i16;            /**< int16_t type AMQP_FIELD_KIND_I16 */
    uint16_t u16;           /**< uint16_t type AMQP_FIELD_KIND_U16 */
    int32_t i32;            /**< int32_t type AMQP_FIELD_KIND_I32 */
    uint32_t u32;           /**< uint32_t type AMQP_FIELD_KIND_U32 */
    int64_t i64;            /**< int64_t type AMQP_FIELD_KIND_I64 */
    uint64_t u64;           /**< uint64_t type AMQP_FIELD_KIND_U64,
                               AMQP_FIELD_KIND_TIMESTAMP */
    float f32;              /**< float type AMQP_FIELD_KIND_F32 */
    double f64;             /**< double type AMQP_FIELD_KIND_F64 */
    amqp_decimal_t decimal; /**< amqp_decimal_t AMQP_FIELD_KIND_DECIMAL */
    amqp_bytes_t bytes;     /**< amqp_bytes_t type AMQP_FIELD_KIND_UTF8,
                               AMQP_FIELD_KIND_BYTES */
    amqp_table_t table;     /**< amqp_table_t type AMQP_FIELD_KIND_TABLE */
    amqp_array_t array;     /**< amqp_array_t type AMQP_FIELD_KIND_ARRAY */
  } value;                  /**< a union of the value */
} amqp_field_value_t;

/**
 * An entry in a field-table
 *
 * \sa amqp_table_encode(), amqp_table_decode(), amqp_table_clone()
 *
 * \since v0.1
 */
typedef struct amqp_table_entry_t_ {
  amqp_bytes_t key; /**< the table entry key. Its a null-terminated UTF-8
                     * string, with a maximum size of 128 bytes */
  amqp_field_value_t value; /**< the table entry values */
} amqp_table_entry_t;

/**
 * Field value types
 *
 * \since v0.1
 */
typedef enum {
  AMQP_FIELD_KIND_BOOLEAN =
      't', /**< boolean type. 0 = false, 1 = true @see amqp_boolean_t */
  AMQP_FIELD_KIND_I8 = 'b',  /**< 8-bit signed integer, datatype: int8_t */
  AMQP_FIELD_KIND_U8 = 'B',  /**< 8-bit unsigned integer, datatype: uint8_t */
  AMQP_FIELD_KIND_I16 = 's', /**< 16-bit signed integer, datatype: int16_t */
  AMQP_FIELD_KIND_U16 = 'u', /**< 16-bit unsigned integer, datatype: uint16_t */
  AMQP_FIELD_KIND_I32 = 'I', /**< 32-bit signed integer, datatype: int32_t */
  AMQP_FIELD_KIND_U32 = 'i', /**< 32-bit unsigned integer, datatype: uint32_t */
  AMQP_FIELD_KIND_I64 = 'l', /**< 64-bit signed integer, datatype: int64_t */
  AMQP_FIELD_KIND_U64 = 'L', /**< 64-bit unsigned integer, datatype: uint64_t */
  AMQP_FIELD_KIND_F32 =
      'f', /**< single-precision floating point value, datatype: float */
  AMQP_FIELD_KIND_F64 =
      'd', /**< double-precision floating point value, datatype: double */
  AMQP_FIELD_KIND_DECIMAL =
      'D', /**< amqp-decimal value, datatype: amqp_decimal_t */
  AMQP_FIELD_KIND_UTF8 = 'S',      /**< UTF-8 null-terminated character string,
                                      datatype: amqp_bytes_t */
  AMQP_FIELD_KIND_ARRAY = 'A',     /**< field array (repeated values of another
                                      datatype. datatype: amqp_array_t */
  AMQP_FIELD_KIND_TIMESTAMP = 'T', /**< 64-bit timestamp. datatype uint64_t */
  AMQP_FIELD_KIND_TABLE = 'F', /**< field table. encapsulates a table inside a
                                  table entry. datatype: amqp_table_t */
  AMQP_FIELD_KIND_VOID = 'V',  /**< empty entry */
  AMQP_FIELD_KIND_BYTES =
      'x' /**< unformatted byte string, datatype: amqp_bytes_t */
} amqp_field_value_kind_t;

/**
 * A list of allocation blocks
 *
 * \since v0.1
 */
typedef struct amqp_pool_blocklist_t_ {
  int num_blocks;   /**< Number of blocks in the block list */
  void **blocklist; /**< Array of memory blocks */
} amqp_pool_blocklist_t;

/**
 * A memory pool
 *
 * \since v0.1
 */
typedef struct amqp_pool_t_ {
  size_t pagesize; /**< the size of the page in bytes. Allocations less than or
                    * equal to this size are allocated in the pages block list.
                    * Allocations greater than this are allocated in their own
                    * own block in the large_blocks block list */

  amqp_pool_blocklist_t pages; /**< blocks that are the size of pagesize */
  amqp_pool_blocklist_t
      large_blocks; /**< allocations larger than the pagesize */

  int next_page;     /**< an index to the next unused page block */
  char *alloc_block; /**< pointer to the current allocation block */
  size_t alloc_used; /**< number of bytes in the current allocation block that
                        has been used */
} amqp_pool_t;

/**
 * An amqp method
 *
 * \since v0.1
 */
typedef struct amqp_method_t_ {
  amqp_method_number_t id; /**< the method id number */
  void *decoded;           /**< pointer to the decoded method,
                            *    cast to the appropriate type to use */
} amqp_method_t;

/**
 * An AMQP frame
 *
 * \since v0.1
 */
typedef struct amqp_frame_t_ {
  uint8_t frame_type;     /**< frame type. The types:
                           * - AMQP_FRAME_METHOD - use the method union member
                           * - AMQP_FRAME_HEADER - use the properties union member
                           * - AMQP_FRAME_BODY - use the body_fragment union member
                           */
  amqp_channel_t channel; /**< the channel the frame was received on */
  union {
    amqp_method_t
        method; /**< a method, use if frame_type == AMQP_FRAME_METHOD */
    struct {
      uint16_t class_id;        /**< the class for the properties */
      uint64_t body_size;       /**< size of the body in bytes */
      void *decoded;            /**< the decoded properties */
      amqp_bytes_t raw;         /**< amqp-encoded properties structure */
    } properties;               /**< message header, a.k.a., properties,
                                      use if frame_type == AMQP_FRAME_HEADER */
    amqp_bytes_t body_fragment; /**< a body fragment, use if frame_type ==
                                   AMQP_FRAME_BODY */
    struct {
      uint8_t transport_high;         /**< @internal first byte of handshake */
      uint8_t transport_low;          /**< @internal second byte of handshake */
      uint8_t protocol_version_major; /**< @internal third byte of handshake */
      uint8_t protocol_version_minor; /**< @internal fourth byte of handshake */
    } protocol_header; /**< Used only when doing the initial handshake with the
                          broker, don't use otherwise */
  } payload;           /**< the payload of the frame */
} amqp_frame_t;

/**
 * Response type
 *
 * \since v0.1
 */
typedef enum amqp_response_type_enum_ {
  AMQP_RESPONSE_NONE = 0, /**< the library got an EOF from the socket */
  AMQP_RESPONSE_NORMAL, /**< response normal, the RPC completed successfully */
  AMQP_RESPONSE_LIBRARY_EXCEPTION, /**< library error, an error occurred in the
                                      library, examine the library_error */
  AMQP_RESPONSE_SERVER_EXCEPTION   /**< server exception, the broker returned an
                                      error, check replay */
} amqp_response_type_enum;

/**
 * Reply from a RPC method on the broker
 *
 * \since v0.1
 */
typedef struct amqp_rpc_reply_t_ {
  amqp_response_type_enum reply_type; /**< the reply type:
                                       * - AMQP_RESPONSE_NORMAL - the RPC
                                       * completed successfully
                                       * - AMQP_RESPONSE_SERVER_EXCEPTION - the
                                       * broker returned
                                       *     an exception, check the reply field
                                       * - AMQP_RESPONSE_LIBRARY_EXCEPTION - the
                                       * library
                                       *    encountered an error, check the
                                       * library_error field
                                       */
  amqp_method_t reply; /**< in case of AMQP_RESPONSE_SERVER_EXCEPTION this
                        * field will be set to the method returned from the
                        * broker */
  int library_error;   /**< in case of AMQP_RESPONSE_LIBRARY_EXCEPTION this
                        *    field will be set to an error code. An error
                        *     string can be retrieved using amqp_error_string */
} amqp_rpc_reply_t;

/**
 * SASL method type
 *
 * \since v0.1
 */
typedef enum amqp_sasl_method_enum_ {
  AMQP_SASL_METHOD_UNDEFINED = -1, /**< Invalid SASL method */
  AMQP_SASL_METHOD_PLAIN =
      0, /**< the PLAIN SASL method for authentication to the broker */
  AMQP_SASL_METHOD_EXTERNAL =
      1 /**< the EXTERNAL SASL method for authentication to the broker */
} amqp_sasl_method_enum;

/**
 * connection state object
 *
 * \since v0.1
 */
typedef struct amqp_connection_state_t_ *amqp_connection_state_t;

/**
 * Socket object
 *
 * \since v0.4.0
 */
typedef struct amqp_socket_t_ amqp_socket_t;

/**
 * Status codes
 *
 * \since v0.4.0
 */
/* NOTE: When updating this enum, update the strings in librabbitmq/amqp_api.c
 */
typedef enum amqp_status_enum_ {
  AMQP_STATUS_OK = 0x0,                             /**< Operation successful */
  AMQP_STATUS_NO_MEMORY = -0x0001,                  /**< Memory allocation
                                                         failed */
  AMQP_STATUS_BAD_AMQP_DATA = -0x0002,              /**< Incorrect or corrupt
                                                         data was received from
                                                         the broker. This is a
                                                         protocol error. */
  AMQP_STATUS_UNKNOWN_CLASS = -0x0003,              /**< An unknown AMQP class
                                                         was received. This is
                                                         a protocol error. */
  AMQP_STATUS_UNKNOWN_METHOD = -0x0004,             /**< An unknown AMQP method
                                                         was received. This is
                                                         a protocol error. */
  AMQP_STATUS_HOSTNAME_RESOLUTION_FAILED = -0x0005, /**< Unable to resolve the
                                                     * hostname */
  AMQP_STATUS_INCOMPATIBLE_AMQP_VERSION = -0x0006,  /**< The broker advertised
                                                         an incompaible AMQP
                                                         version */
  AMQP_STATUS_CONNECTION_CLOSED = -0x0007,          /**< The connection to the
                                                         broker has been closed
                                                         */
  AMQP_STATUS_BAD_URL = -0x0008,                    /**< malformed AMQP URL */
  AMQP_STATUS_SOCKET_ERROR = -0x0009,               /**< A socket error
                                                         occurred */
  AMQP_STATUS_INVALID_PARAMETER = -0x000A,          /**< An invalid parameter
                                                         was passed into the
                                                         function */
  AMQP_STATUS_TABLE_TOO_BIG = -0x000B,              /**< The amqp_table_t object
                                                         cannot be serialized
                                                         because the output
                                                         buffer is too small */
  AMQP_STATUS_WRONG_METHOD = -0x000C,               /**< The wrong method was
                                                         received */
  AMQP_STATUS_TIMEOUT = -0x000D,                    /**< Operation timed out */
  AMQP_STATUS_TIMER_FAILURE = -0x000E,              /**< The underlying system
                                                         timer facility failed */
  AMQP_STATUS_HEARTBEAT_TIMEOUT = -0x000F,          /**< Timed out waiting for
                                                         heartbeat */
  AMQP_STATUS_UNEXPECTED_STATE = -0x0010,           /**< Unexpected protocol
                                                         state */
  AMQP_STATUS_SOCKET_CLOSED = -0x0011,              /**< Underlying socket is
                                                         closed */
  AMQP_STATUS_SOCKET_INUSE = -0x0012,               /**< Underlying socket is
                                                         already open */
  AMQP_STATUS_BROKER_UNSUPPORTED_SASL_METHOD = -0x0013, /**< Broker does not
                                                          support the requested
                                                          SASL mechanism */
  AMQP_STATUS_UNSUPPORTED = -0x0014, /**< Parameter is unsupported
                                       in this version */
  _AMQP_STATUS_NEXT_VALUE = -0x0015, /**< Internal value */

  AMQP_STATUS_TCP_ERROR = -0x0100,                /**< A generic TCP error
                                                       occurred */
  AMQP_STATUS_TCP_SOCKETLIB_INIT_ERROR = -0x0101, /**< An error occurred trying
                                                       to initialize the
                                                       socket library*/
  _AMQP_STATUS_TCP_NEXT_VALUE = -0x0102,          /**< Internal value */

  AMQP_STATUS_SSL_ERROR = -0x0200,                  /**< A generic SSL error
                                                         occurred. */
  AMQP_STATUS_SSL_HOSTNAME_VERIFY_FAILED = -0x0201, /**< SSL validation of
                                                         hostname against
                                                         peer certificate
                                                         failed */
  AMQP_STATUS_SSL_PEER_VERIFY_FAILED = -0x0202,     /**< SSL validation of peer
                                                         certificate failed. */
  AMQP_STATUS_SSL_CONNECTION_FAILED = -0x0203, /**< SSL handshake failed. */
  _AMQP_STATUS_SSL_NEXT_VALUE = -0x0204        /**< Internal value */
} amqp_status_enum;

/**
 * AMQP delivery modes.
 * Use these values for the #amqp_basic_properties_t::delivery_mode field.
 *
 * \since v0.5
 */
typedef enum {
  AMQP_DELIVERY_NONPERSISTENT = 1, /**< Non-persistent message */
  AMQP_DELIVERY_PERSISTENT = 2     /**< Persistent message */
} amqp_delivery_mode_enum;

AMQP_END_DECLS

#include <amqp_framing.h>

AMQP_BEGIN_DECLS

/**
 * Empty bytes structure
 *
 * \since v0.2
 */
AMQP_PUBLIC_VARIABLE const amqp_bytes_t amqp_empty_bytes;

/**
 * Empty table structure
 *
 * \since v0.2
 */
AMQP_PUBLIC_VARIABLE const amqp_table_t amqp_empty_table;

/**
 * Empty table array structure
 *
 * \since v0.2
 */
AMQP_PUBLIC_VARIABLE const amqp_array_t amqp_empty_array;

/* Compatibility macros for the above, to avoid the need to update
   code written against earlier versions of librabbitmq. */

/**
 * \def AMQP_EMPTY_BYTES
 *
 * Deprecated, use \ref amqp_empty_bytes instead
 *
 * \deprecated use \ref amqp_empty_bytes instead
 *
 * \since v0.1
 */
#define AMQP_EMPTY_BYTES amqp_empty_bytes

/**
 * \def AMQP_EMPTY_TABLE
 *
 * Deprecated, use \ref amqp_empty_table instead
 *
 * \deprecated use \ref amqp_empty_table instead
 *
 * \since v0.1
 */
#define AMQP_EMPTY_TABLE amqp_empty_table

/**
 * \def AMQP_EMPTY_ARRAY
 *
 * Deprecated, use \ref amqp_empty_array instead
 *
 * \deprecated use \ref amqp_empty_array instead
 *
 * \since v0.1
 */
#define AMQP_EMPTY_ARRAY amqp_empty_array

/**
 * Initializes an amqp_pool_t memory allocation pool for use
 *
 * Readies an allocation pool for use. An amqp_pool_t
 * must be initialized before use
 *
 * \param [in] pool the amqp_pool_t structure to initialize.
 *              Calling this function on a pool a pool that has
 *              already been initialized will result in undefined
 *              behavior
 * \param [in] pagesize the unit size that the pool will allocate
 *              memory chunks in. Anything allocated against the pool
 *              with a requested size will be carved out of a block
 *              this size. Allocations larger than this will be
 *              allocated individually
 *
 * \sa recycle_amqp_pool(), empty_amqp_pool(), amqp_pool_alloc(),
 *     amqp_pool_alloc_bytes(), amqp_pool_t
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
void AMQP_CALL init_amqp_pool(amqp_pool_t *pool, size_t pagesize);

/**
 * Recycles an amqp_pool_t memory allocation pool
 *
 * Recycles the space allocate by the pool
 *
 * This invalidates all allocations made against the pool before this call is
 * made, any use of any allocations made before recycle_amqp_pool() is called
 * will result in undefined behavior.
 *
 * Note: this may or may not release memory, to force memory to be released
 * call empty_amqp_pool().
 *
 * \param [in] pool the amqp_pool_t to recycle
 *
 * \sa recycle_amqp_pool(), empty_amqp_pool(), amqp_pool_alloc(),
 *      amqp_pool_alloc_bytes()
 *
 * \since v0.1
 *
 */
AMQP_PUBLIC_FUNCTION
void AMQP_CALL recycle_amqp_pool(amqp_pool_t *pool);

/**
 * Empties an amqp memory pool
 *
 * Releases all memory associated with an allocation pool
 *
 * \param [in] pool the amqp_pool_t to empty
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
void AMQP_CALL empty_amqp_pool(amqp_pool_t *pool);

/**
 * Allocates a block of memory from an amqp_pool_t memory pool
 *
 * Memory will be aligned on a 8-byte boundary. If a 0-length allocation is
 * requested, a NULL pointer will be returned.
 *
 * \param [in] pool the allocation pool to allocate the memory from
 * \param [in] amount the size of the allocation in bytes.
 * \return a pointer to the memory block, or NULL if the allocation cannot
 *          be satisfied.
 *
 * \sa init_amqp_pool(), recycle_amqp_pool(), empty_amqp_pool(),
 *     amqp_pool_alloc_bytes()
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
void *AMQP_CALL amqp_pool_alloc(amqp_pool_t *pool, size_t amount);

/**
 * Allocates a block of memory from an amqp_pool_t to an amqp_bytes_t
 *
 * Memory will be aligned on a 8-byte boundary. If a 0-length allocation is
 * requested, output.bytes = NULL.
 *
 * \param [in] pool the allocation pool to allocate the memory from
 * \param [in] amount the size of the allocation in bytes
 * \param [in] output the location to store the pointer. On success
 *              output.bytes will be set to the beginning of the buffer
 *              output.len will be set to amount
 *              On error output.bytes will be set to NULL and output.len
 *              set to 0
 *
 * \sa init_amqp_pool(), recycle_amqp_pool(), empty_amqp_pool(),
 *     amqp_pool_alloc()
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
void AMQP_CALL amqp_pool_alloc_bytes(amqp_pool_t *pool, size_t amount,
                                     amqp_bytes_t *output);

/**
 * Wraps a c string in an amqp_bytes_t
 *
 * Takes a string, calculates its length and creates an
 * amqp_bytes_t that points to it. The string is not duplicated.
 *
 * For a given input cstr, The amqp_bytes_t output.bytes is the
 * same as cstr, output.len is the length of the string not including
 * the \0 terminator
 *
 * This function uses strlen() internally so cstr must be properly
 * terminated
 *
 * \param [in] cstr the c string to wrap
 * \return an amqp_bytes_t that describes the string
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
amqp_bytes_t AMQP_CALL amqp_cstring_bytes(char const *cstr);

/**
 * Duplicates an amqp_bytes_t buffer.
 *
 * The buffer is cloned and the contents copied.
 *
 * The memory associated with the output is allocated
 * with amqp_bytes_malloc() and should be freed with
 * amqp_bytes_free()
 *
 * \param [in] src
 * \return a clone of the src
 *
 * \sa amqp_bytes_free(), amqp_bytes_malloc()
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
amqp_bytes_t AMQP_CALL amqp_bytes_malloc_dup(amqp_bytes_t src);

/**
 * Allocates a amqp_bytes_t buffer
 *
 * Creates an amqp_bytes_t buffer of the specified amount, the buffer should be
 * freed using amqp_bytes_free()
 *
 * \param [in] amount the size of the buffer in bytes
 * \returns an amqp_bytes_t with amount bytes allocated.
 *           output.bytes will be set to NULL on error
 *
 * \sa amqp_bytes_free(), amqp_bytes_malloc_dup()
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
amqp_bytes_t AMQP_CALL amqp_bytes_malloc(size_t amount);

/**
 * Frees an amqp_bytes_t buffer
 *
 * Frees a buffer allocated with amqp_bytes_malloc() or amqp_bytes_malloc_dup()
 *
 * Calling amqp_bytes_free on buffers not allocated with one
 * of those two functions will result in undefined behavior
 *
 * \param [in] bytes the buffer to free
 *
 * \sa amqp_bytes_malloc(), amqp_bytes_malloc_dup()
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
void AMQP_CALL amqp_bytes_free(amqp_bytes_t bytes);

/**
 * Allocate and initialize a new amqp_connection_state_t object
 *
 * amqp_connection_state_t objects created with this function
 * should be freed with amqp_destroy_connection()
 *
 * \returns an opaque pointer on success, NULL or 0 on failure.
 *
 * \sa amqp_destroy_connection()
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
amqp_connection_state_t AMQP_CALL amqp_new_connection(void);

/**
 * Get the underlying socket descriptor for the connection
 *
 * \warning Use the socket returned from this function carefully, incorrect use
 * of the socket outside of the library will lead to undefined behavior.
 * Additionally rabbitmq-c may use the socket differently version-to-version,
 * what may work in one version, may break in the next version. Be sure to
 * throughly test any applications that use the socket returned by this
 * function especially when using a newer version of rabbitmq-c
 *
 * \param [in] state the connection object
 * \returns the socket descriptor if one has been set, -1 otherwise
 *
 * \sa amqp_tcp_socket_new(), amqp_ssl_socket_new(), amqp_socket_open()
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_get_sockfd(amqp_connection_state_t state);

/**
 * Deprecated, use amqp_tcp_socket_new() or amqp_ssl_socket_new()
 *
 * \deprecated Use amqp_tcp_socket_new() or amqp_ssl_socket_new()
 *
 * Sets the socket descriptor associated with the connection. The socket
 * should be connected to a broker, and should not be read to or written from
 * before calling this function.  A socket descriptor can be created and opened
 * using amqp_open_socket()
 *
 * \param [in] state the connection object
 * \param [in] sockfd the socket
 *
 * \sa amqp_open_socket(), amqp_tcp_socket_new(), amqp_ssl_socket_new()
 *
 * \since v0.1
 */
AMQP_DEPRECATED(AMQP_PUBLIC_FUNCTION void AMQP_CALL
                    amqp_set_sockfd(amqp_connection_state_t state, int sockfd));

/**
 * Tune client side parameters
 *
 * \warning This function may call abort() if the connection is in a certain
 *  state. As such it should probably not be called code outside the library.
 *  connection parameters should be specified when calling amqp_login() or
 *  amqp_login_with_properties()
 *
 * This function changes channel_max, frame_max, and heartbeat parameters, on
 * the client side only. It does not try to renegotiate these parameters with
 * the broker. Using this function will lead to unexpected results.
 *
 * \param [in] state the connection object
 * \param [in] channel_max the maximum number of channels.
 *              The largest this can be is 65535
 * \param [in] frame_max the maximum size of an frame.
 *              The smallest this can be is 4096
 *              The largest this can be is 2147483647
 *              Unless you know what you're doing the recommended
 *              size is 131072 or 128KB
 * \param [in] heartbeat the number of seconds between heartbeats
 *
 * \return AMQP_STATUS_OK on success, an amqp_status_enum value otherwise.
 *  Possible error codes include:
 *  - AMQP_STATUS_NO_MEMORY memory allocation failed.
 *  - AMQP_STATUS_TIMER_FAILURE the underlying system timer indicated it
 *    failed.
 *
 * \sa amqp_login(), amqp_login_with_properties()
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_tune_connection(amqp_connection_state_t state,
                                   int channel_max, int frame_max,
                                   int heartbeat);

/**
 * Get the maximum number of channels the connection can handle
 *
 * The maximum number of channels is set when connection negotiation takes
 * place in amqp_login() or amqp_login_with_properties().
 *
 * \param [in] state the connection object
 * \return the maximum number of channels. 0 if there is no limit
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_get_channel_max(amqp_connection_state_t state);

/**
 * Get the maximum size of an frame the connection can handle
 *
 * The maximum size of an frame is set when connection negotiation takes
 * place in amqp_login() or amqp_login_with_properties().
 *
 * \param [in] state the connection object
 * \return the maximum size of an frame.
 *
 * \since v0.6
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_get_frame_max(amqp_connection_state_t state);

/**
 * Get the number of seconds between heartbeats of the connection
 *
 * The number of seconds between heartbeats is set when connection
 * negotiation takes place in amqp_login() or amqp_login_with_properties().
 *
 * \param [in] state the connection object
 * \return the number of seconds between heartbeats.
 *
 * \since v0.6
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_get_heartbeat(amqp_connection_state_t state);

/**
 * Destroys an amqp_connection_state_t object
 *
 * Destroys a amqp_connection_state_t object that was created with
 * amqp_new_connection(). If the connection with the broker is open, it will be
 * implicitly closed with a reply code of 200 (success). Any memory that
 * would be freed with amqp_maybe_release_buffers() or
 * amqp_maybe_release_buffers_on_channel() will be freed, and use of that
 * memory will caused undefined behavior.
 *
 * \param [in] state the connection object
 * \return AMQP_STATUS_OK on success. amqp_status_enum value failure
 *
 * \sa amqp_new_connection()
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_destroy_connection(amqp_connection_state_t state);

/**
 * Process incoming data
 *
 * \warning This is a low-level function intended for those who want to
 *  have greater control over input and output over the socket from the
 *  broker. Correctly using this function requires in-depth knowledge of AMQP
 *  and rabbitmq-c.
 *
 * For a given buffer of data received from the broker, decode the first
 * frame in the buffer. If more than one frame is contained in the input buffer
 * the return value will be less than the received_data size, the caller should
 * adjust received_data buffer descriptor to point to the beginning of the
 * buffer + the return value.
 *
 * \param [in] state the connection object
 * \param [in] received_data a buffer of data received from the broker. The
 *  function will return the number of bytes of the buffer it used. The
 *  function copies these bytes to an internal buffer: this part of the buffer
 *  may be reused after this function successfully completes.
 * \param [in,out] decoded_frame caller should pass in a pointer to an
 *  amqp_frame_t struct. If there is enough data in received_data for a
 *  complete frame, decoded_frame->frame_type will be set to something OTHER
 *  than 0. decoded_frame may contain members pointing to memory owned by
 *  the state object. This memory can be recycled with
 *  amqp_maybe_release_buffers() or amqp_maybe_release_buffers_on_channel().
 * \return number of bytes consumed from received_data or 0 if a 0-length
 *  buffer was passed. A negative return value indicates failure. Possible
 * errors:
 *  - AMQP_STATUS_NO_MEMORY failure in allocating memory. The library is likely
 *    in an indeterminate state making recovery unlikely. Client should note the
 *    error and terminate the application
 *  - AMQP_STATUS_BAD_AMQP_DATA bad AMQP data was received. The connection
 *    should be shutdown immediately
 *  - AMQP_STATUS_UNKNOWN_METHOD: an unknown method was received from the
 *    broker. This is likely a protocol error and the connection should be
 *    shutdown immediately
 *  - AMQP_STATUS_UNKNOWN_CLASS: a properties frame with an unknown class
 *    was received from the broker. This is likely a protocol error and the
 *    connection should be shutdown immediately
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_handle_input(amqp_connection_state_t state,
                                amqp_bytes_t received_data,
                                amqp_frame_t *decoded_frame);

/**
 * Check to see if connection memory can be released
 *
 * \deprecated This function is deprecated in favor of
 *  amqp_maybe_release_buffers() or amqp_maybe_release_buffers_on_channel()
 *
 * Checks the state of an amqp_connection_state_t object to see if
 * amqp_release_buffers() can be called successfully.
 *
 * \param [in] state the connection object
 * \returns TRUE if the buffers can be released FALSE otherwise
 *
 * \sa amqp_release_buffers() amqp_maybe_release_buffers()
 *  amqp_maybe_release_buffers_on_channel()
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
amqp_boolean_t AMQP_CALL amqp_release_buffers_ok(amqp_connection_state_t state);

/**
 * Release amqp_connection_state_t owned memory
 *
 * \deprecated This function is deprecated in favor of
 *  amqp_maybe_release_buffers() or amqp_maybe_release_buffers_on_channel()
 *
 * \warning caller should ensure amqp_release_buffers_ok() returns true before
 *  calling this function. Failure to do so may result in abort() being called.
 *
 * Release memory owned by the amqp_connection_state_t for reuse by the
 * library. Use of any memory returned by the library before this function is
 * called will result in undefined behavior.
 *
 * \note internally rabbitmq-c tries to reuse memory when possible. As a result
 * its possible calling this function may not have a noticeable effect on
 * memory usage.
 *
 * \param [in] state the connection object
 *
 * \sa amqp_release_buffers_ok() amqp_maybe_release_buffers()
 *  amqp_maybe_release_buffers_on_channel()
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
void AMQP_CALL amqp_release_buffers(amqp_connection_state_t state);

/**
 * Release amqp_connection_state_t owned memory
 *
 * Release memory owned by the amqp_connection_state_t object related to any
 * channel, allowing reuse by the library. Use of any memory returned by the
 * library before this function is called with result in undefined behavior.
 *
 * \note internally rabbitmq-c tries to reuse memory when possible. As a result
 * its possible calling this function may not have a noticeable effect on
 * memory usage.
 *
 * \param [in] state the connection object
 *
 * \sa amqp_maybe_release_buffers_on_channel()
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
void AMQP_CALL amqp_maybe_release_buffers(amqp_connection_state_t state);

/**
 * Release amqp_connection_state_t owned memory related to a channel
 *
 * Release memory owned by the amqp_connection_state_t object related to the
 * specified channel, allowing reuse by the library. Use of any memory returned
 * the library for a specific channel will result in undefined behavior.
 *
 * \note internally rabbitmq-c tries to reuse memory when possible. As a result
 * its possible calling this function may not have a noticeable effect on
 * memory usage.
 *
 * \param [in] state the connection object
 * \param [in] channel the channel specifier for which memory should be
 *  released. Note that the library does not care about the state of the
 *  channel when calling this function
 *
 * \sa amqp_maybe_release_buffers()
 *
 * \since v0.4.0
 */
AMQP_PUBLIC_FUNCTION
void AMQP_CALL amqp_maybe_release_buffers_on_channel(
    amqp_connection_state_t state, amqp_channel_t channel);

/**
 * Send a frame to the broker
 *
 * \param [in] state the connection object
 * \param [in] frame the frame to send to the broker
 * \return AMQP_STATUS_OK on success, an amqp_status_enum value on error.
 *  Possible error codes:
 *  - AMQP_STATUS_BAD_AMQP_DATA the serialized form of the method or
 *    properties was too large to fit in a single AMQP frame, or the
 *    method contains an invalid value. The frame was not sent.
 *  - AMQP_STATUS_TABLE_TOO_BIG the serialized form of an amqp_table_t is
 *    too large to fit in a single AMQP frame. Frame was not sent.
 *  - AMQP_STATUS_UNKNOWN_METHOD an invalid method type was passed in
 *  - AMQP_STATUS_UNKNOWN_CLASS an invalid properties type was passed in
 *  - AMQP_STATUS_TIMER_FAILURE system timer indicated failure. The frame
 *    was sent
 *  - AMQP_STATUS_SOCKET_ERROR
 *  - AMQP_STATUS_SSL_ERROR
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_send_frame(amqp_connection_state_t state,
                              amqp_frame_t const *frame);

/**
 * Compare two table entries
 *
 * Works just like strcmp(), comparing two the table keys, datatype, then values
 *
 * \param [in] entry1 the entry on the left
 * \param [in] entry2 the entry on the right
 * \return 0 if entries are equal, 0 < if left is greater, 0 > if right is
 * greater
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_table_entry_cmp(void const *entry1, void const *entry2);

/**
 * Open a socket to a remote host
 *
 * \deprecated This function is deprecated in favor of amqp_socket_open()
 *
 * Looks up the hostname, then attempts to open a socket to the host using
 * the specified portnumber. It also sets various options on the socket to
 * improve performance and correctness.
 *
 * \param [in] hostname this can be a hostname or IP address.
 *              Both IPv4 and IPv6 are acceptable
 * \param [in] portnumber the port to connect on. RabbitMQ brokers
 *              listen on port 5672, and 5671 for SSL
 * \return a positive value indicates success and is the sockfd. A negative
 *  value (see amqp_status_enum)is returned on failure. Possible error codes:
 *  - AMQP_STATUS_TCP_SOCKETLIB_INIT_ERROR Initialization of underlying socket
 *    library failed.
 *  - AMQP_STATUS_HOSTNAME_RESOLUTION_FAILED hostname lookup failed.
 *  - AMQP_STATUS_SOCKET_ERROR a socket error occurred. errno or
 *    WSAGetLastError() may return more useful information.
 *
 * \note IPv6 support was added in v0.3
 *
 * \sa amqp_socket_open() amqp_set_sockfd()
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_open_socket(char const *hostname, int portnumber);

/**
 * Send initial AMQP header to the broker
 *
 * \warning this is a low level function intended for those who want to
 * interact with the broker at a very low level. Use of this function without
 * understanding what it does will result in AMQP protocol errors.
 *
 * This function sends the AMQP protocol header to the broker.
 *
 * \param [in] state the connection object
 * \return AMQP_STATUS_OK on success, a negative value on failure. Possible
 *  error codes:
 * - AMQP_STATUS_CONNECTION_CLOSED the connection to the broker was closed.
 * - AMQP_STATUS_SOCKET_ERROR a socket error occurred. It is likely the
 *   underlying socket has been closed. errno or WSAGetLastError() may provide
 *   further information.
 * - AMQP_STATUS_SSL_ERROR a SSL error occurred. The connection to the broker
 *   was closed.
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_send_header(amqp_connection_state_t state);

/**
 * Checks to see if there are any incoming frames ready to be read
 *
 * Checks to see if there are any amqp_frame_t objects buffered by the
 * amqp_connection_state_t object. Having one or more frames buffered means
 * that amqp_simple_wait_frame() or amqp_simple_wait_frame_noblock() will
 * return a frame without potentially blocking on a read() call.
 *
 * \param [in] state the connection object
 * \return TRUE if there are frames enqueued, FALSE otherwise
 *
 * \sa amqp_simple_wait_frame() amqp_simple_wait_frame_noblock()
 *  amqp_data_in_buffer()
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
amqp_boolean_t AMQP_CALL amqp_frames_enqueued(amqp_connection_state_t state);

/**
 * Read a single amqp_frame_t
 *
 * Waits for the next amqp_frame_t frame to be read from the broker.
 * This function has the potential to block for a long time in the case of
 * waiting for a basic.deliver method frame from the broker.
 *
 * The library may buffer frames. When an amqp_connection_state_t object
 * has frames buffered calling amqp_simple_wait_frame() will return an
 * amqp_frame_t without entering a blocking read(). You can test to see if
 * an amqp_connection_state_t object has frames buffered by calling the
 * amqp_frames_enqueued() function.
 *
 * The library has a socket read buffer. When there is data in an
 * amqp_connection_state_t read buffer, amqp_simple_wait_frame() may return an
 * amqp_frame_t without entering a blocking read(). You can test to see if an
 * amqp_connection_state_t object has data in its read buffer by calling the
 * amqp_data_in_buffer() function.
 *
 * \param [in] state the connection object
 * \param [out] decoded_frame the frame
 * \return AMQP_STATUS_OK on success, an amqp_status_enum value
 *  is returned otherwise. Possible errors include:
 *  - AMQP_STATUS_NO_MEMORY failure in allocating memory. The library is likely
 *    in an indeterminate state making recovery unlikely. Client should note the
 *    error and terminate the application
 *  - AMQP_STATUS_BAD_AMQP_DATA bad AMQP data was received. The connection
 *    should be shutdown immediately
 *  - AMQP_STATUS_UNKNOWN_METHOD: an unknown method was received from the
 *    broker. This is likely a protocol error and the connection should be
 *    shutdown immediately
 *  - AMQP_STATUS_UNKNOWN_CLASS: a properties frame with an unknown class
 *    was received from the broker. This is likely a protocol error and the
 *    connection should be shutdown immediately
 *  - AMQP_STATUS_HEARTBEAT_TIMEOUT timed out while waiting for heartbeat
 *    from the broker. The connection has been closed.
 *  - AMQP_STATUS_TIMER_FAILURE system timer indicated failure.
 *  - AMQP_STATUS_SOCKET_ERROR a socket error occurred. The connection has
 *    been closed
 *  - AMQP_STATUS_SSL_ERROR a SSL socket error occurred. The connection has
 *    been closed.
 *
 * \sa amqp_simple_wait_frame_noblock() amqp_frames_enqueued()
 *  amqp_data_in_buffer()
 *
 * \note as of v0.4.0 this function will no longer return heartbeat frames
 *  when enabled by specifying a non-zero heartbeat value in amqp_login().
 *  Heartbeating is handled internally by the library.
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_simple_wait_frame(amqp_connection_state_t state,
                                     amqp_frame_t *decoded_frame);

/**
 * Read a single amqp_frame_t with a timeout.
 *
 * Waits for the next amqp_frame_t frame to be read from the broker, up to
 * a timespan specified by tv. The function will return AMQP_STATUS_TIMEOUT
 * if the timeout is reached. The tv value is not modified by the function.
 *
 * If a 0 timeval is specified, the function behaves as if its non-blocking: it
 * will test to see if a frame can be read from the broker, and return
 * immediately.
 *
 * If NULL is passed in for tv, the function will behave like
 * amqp_simple_wait_frame() and block until a frame is received from the broker
 *
 * The library may buffer frames.  When an amqp_connection_state_t object
 * has frames buffered calling amqp_simple_wait_frame_noblock() will return an
 * amqp_frame_t without entering a blocking read(). You can test to see if an
 * amqp_connection_state_t object has frames buffered by calling the
 * amqp_frames_enqueued() function.
 *
 * The library has a socket read buffer. When there is data in an
 * amqp_connection_state_t read buffer, amqp_simple_wait_frame_noblock() may
 * return
 * an amqp_frame_t without entering a blocking read(). You can test to see if an
 * amqp_connection_state_t object has data in its read buffer by calling the
 * amqp_data_in_buffer() function.
 *
 * \note This function does not return heartbeat frames. When enabled,
 *  heartbeating is handed internally internally by the library.
 *
 * \param [in,out] state the connection object
 * \param [out] decoded_frame the frame
 * \param [in] tv the maximum time to wait for a frame to be read. Setting
 * tv->tv_sec = 0 and tv->tv_usec = 0 will do a non-blocking read. Specifying
 * NULL for tv will make the function block until a frame is read.
 * \return AMQP_STATUS_OK on success. An amqp_status_enum value is returned
 *  otherwise. Possible errors include:
 *  - AMQP_STATUS_TIMEOUT the timeout was reached while waiting for a frame
 *    from the broker.
 *  - AMQP_STATUS_INVALID_PARAMETER the tv parameter contains an invalid value.
 *  - AMQP_STATUS_NO_MEMORY failure in allocating memory. The library is likely
 *    in an indeterminate state making recovery unlikely. Client should note the
 *    error and terminate the application
 *  - AMQP_STATUS_BAD_AMQP_DATA bad AMQP data was received. The connection
 *    should be shutdown immediately
 *  - AMQP_STATUS_UNKNOWN_METHOD: an unknown method was received from the
 *    broker. This is likely a protocol error and the connection should be
 *    shutdown immediately
 *  - AMQP_STATUS_UNKNOWN_CLASS: a properties frame with an unknown class
 *    was received from the broker. This is likely a protocol error and the
 *    connection should be shutdown immediately
 *  - AMQP_STATUS_HEARTBEAT_TIMEOUT timed out while waiting for heartbeat
 *    from the broker. The connection has been closed.
 *  - AMQP_STATUS_TIMER_FAILURE system timer indicated failure.
 *  - AMQP_STATUS_SOCKET_ERROR a socket error occurred. The connection has
 *    been closed
 *  - AMQP_STATUS_SSL_ERROR a SSL socket error occurred. The connection has
 *    been closed.
 *
 * \sa amqp_simple_wait_frame() amqp_frames_enqueued() amqp_data_in_buffer()
 *
 * \since v0.4.0
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_simple_wait_frame_noblock(amqp_connection_state_t state,
                                             amqp_frame_t *decoded_frame,
                                             struct timeval *tv);

/**
 * Waits for a specific method from the broker
 *
 * \warning You probably don't want to use this function. If this function
 *  doesn't receive exactly the frame requested it closes the whole connection.
 *
 * Waits for a single method on a channel from the broker.
 * If a frame is received that does not match expected_channel
 * or expected_method the program will abort
 *
 * \param [in] state the connection object
 * \param [in] expected_channel the channel that the method should be delivered
 *  on
 * \param [in] expected_method the method to wait for
 * \param [out] output the method
 * \returns AMQP_STATUS_OK on success. An amqp_status_enum value is returned
 *  otherwise. Possible errors include:
 *  - AMQP_STATUS_WRONG_METHOD a frame containing the wrong method, wrong frame
 *    type or wrong channel was received. The connection is closed.
 *  - AMQP_STATUS_NO_MEMORY failure in allocating memory. The library is likely
 *    in an indeterminate state making recovery unlikely. Client should note the
 *    error and terminate the application
 *  - AMQP_STATUS_BAD_AMQP_DATA bad AMQP data was received. The connection
 *    should be shutdown immediately
 *  - AMQP_STATUS_UNKNOWN_METHOD: an unknown method was received from the
 *    broker. This is likely a protocol error and the connection should be
 *    shutdown immediately
 *  - AMQP_STATUS_UNKNOWN_CLASS: a properties frame with an unknown class
 *    was received from the broker. This is likely a protocol error and the
 *    connection should be shutdown immediately
 *  - AMQP_STATUS_HEARTBEAT_TIMEOUT timed out while waiting for heartbeat
 *    from the broker. The connection has been closed.
 *  - AMQP_STATUS_TIMER_FAILURE system timer indicated failure.
 *  - AMQP_STATUS_SOCKET_ERROR a socket error occurred. The connection has
 *    been closed
 *  - AMQP_STATUS_SSL_ERROR a SSL socket error occurred. The connection has
 *    been closed.
 *
 * \since v0.1
 */

AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_simple_wait_method(amqp_connection_state_t state,
                                      amqp_channel_t expected_channel,
                                      amqp_method_number_t expected_method,
                                      amqp_method_t *output);

/**
 * Sends a method to the broker
 *
 * This is a thin wrapper around amqp_send_frame(), providing a way to send
 * a method to the broker on a specified channel.
 *
 * \param [in] state the connection object
 * \param [in] channel the channel object
 * \param [in] id the method number
 * \param [in] decoded the method object
 * \returns AMQP_STATUS_OK on success, an amqp_status_enum value otherwise.
 *  Possible errors include:
 *  - AMQP_STATUS_BAD_AMQP_DATA the serialized form of the method or
 *    properties was too large to fit in a single AMQP frame, or the
 *    method contains an invalid value. The frame was not sent.
 *  - AMQP_STATUS_TABLE_TOO_BIG the serialized form of an amqp_table_t is
 *    too large to fit in a single AMQP frame. Frame was not sent.
 *  - AMQP_STATUS_UNKNOWN_METHOD an invalid method type was passed in
 *  - AMQP_STATUS_UNKNOWN_CLASS an invalid properties type was passed in
 *  - AMQP_STATUS_TIMER_FAILURE system timer indicated failure. The frame
 *    was sent
 *  - AMQP_STATUS_SOCKET_ERROR
 *  - AMQP_STATUS_SSL_ERROR
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_send_method(amqp_connection_state_t state,
                               amqp_channel_t channel, amqp_method_number_t id,
                               void *decoded);

/**
 * Sends a method to the broker and waits for a method response
 *
 * \param [in] state the connection object
 * \param [in] channel the channel object
 * \param [in] request_id the method number of the request
 * \param [in] expected_reply_ids a 0 terminated array of expected response
 *             method numbers
 * \param [in] decoded_request_method the method to be sent to the broker
 * \return a amqp_rpc_reply_t:
 *  - r.reply_type == AMQP_RESPONSE_NORMAL. RPC completed successfully
 *  - r.reply_type == AMQP_RESPONSE_SERVER_EXCEPTION. The broker returned an
 *    exception:
 *    - If r.reply.id == AMQP_CHANNEL_CLOSE_METHOD a channel exception
 *      occurred, cast r.reply.decoded to amqp_channel_close_t* to see details
 *      of the exception. The client should amqp_send_method() a
 *      amqp_channel_close_ok_t. The channel must be re-opened before it
 *      can be used again. Any resources associated with the channel
 *      (auto-delete exchanges, auto-delete queues, consumers) are invalid
 *      and must be recreated before attempting to use them again.
 *    - If r.reply.id == AMQP_CONNECTION_CLOSE_METHOD a connection exception
 *      occurred, cast r.reply.decoded to amqp_connection_close_t* to see
 *      details of the exception. The client amqp_send_method() a
 *      amqp_connection_close_ok_t and disconnect from the broker.
 *  - r.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION. An exception occurred
 *    within the library. Examine r.library_error and compare it against
 *    amqp_status_enum values to determine the error.
 *
 * \sa amqp_simple_rpc_decoded()
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
amqp_rpc_reply_t AMQP_CALL amqp_simple_rpc(
    amqp_connection_state_t state, amqp_channel_t channel,
    amqp_method_number_t request_id, amqp_method_number_t *expected_reply_ids,
    void *decoded_request_method);

/**
 * Sends a method to the broker and waits for a method response
 *
 * \param [in] state the connection object
 * \param [in] channel the channel object
 * \param [in] request_id the method number of the request
 * \param [in] reply_id the method number expected in response
 * \param [in] decoded_request_method the request method
 * \return a pointer to the method returned from the broker, or NULL on error.
 *  On error amqp_get_rpc_reply() will return an amqp_rpc_reply_t with
 *  details on the error that occurred.
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
void *AMQP_CALL amqp_simple_rpc_decoded(amqp_connection_state_t state,
                                        amqp_channel_t channel,
                                        amqp_method_number_t request_id,
                                        amqp_method_number_t reply_id,
                                        void *decoded_request_method);

/**
 * Get the last global amqp_rpc_reply
 *
 * The API methods corresponding to most synchronous AMQP methods
 * return a pointer to the decoded method result.  Upon error, they
 * return NULL, and we need some way of discovering what, if anything,
 * went wrong. amqp_get_rpc_reply() returns the most recent
 * amqp_rpc_reply_t instance corresponding to such an API operation
 * for the given connection.
 *
 * Only use it for operations that do not themselves return
 * amqp_rpc_reply_t; operations that do return amqp_rpc_reply_t
 * generally do NOT update this per-connection-global amqp_rpc_reply_t
 * instance.
 *
 * \param [in] state the connection object
 * \return the most recent amqp_rpc_reply_t:
 *  - r.reply_type == AMQP_RESPONSE_NORMAL. RPC completed successfully
 *  - r.reply_type == AMQP_RESPONSE_SERVER_EXCEPTION. The broker returned an
 *    exception:
 *    - If r.reply.id == AMQP_CHANNEL_CLOSE_METHOD a channel exception
 *      occurred, cast r.reply.decoded to amqp_channel_close_t* to see details
 *      of the exception. The client should amqp_send_method() a
 *      amqp_channel_close_ok_t. The channel must be re-opened before it
 *      can be used again. Any resources associated with the channel
 *      (auto-delete exchanges, auto-delete queues, consumers) are invalid
 *      and must be recreated before attempting to use them again.
 *    - If r.reply.id == AMQP_CONNECTION_CLOSE_METHOD a connection exception
 *      occurred, cast r.reply.decoded to amqp_connection_close_t* to see
 *      details of the exception. The client amqp_send_method() a
 *      amqp_connection_close_ok_t and disconnect from the broker.
 *  - r.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION. An exception occurred
 *    within the library. Examine r.library_error and compare it against
 *    amqp_status_enum values to determine the error.
 *
 * \sa amqp_simple_rpc_decoded()
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
amqp_rpc_reply_t AMQP_CALL amqp_get_rpc_reply(amqp_connection_state_t state);

/**
 * Login to the broker
 *
 * After using amqp_open_socket and amqp_set_sockfd, call
 * amqp_login to complete connecting to the broker
 *
 * \param [in] state the connection object
 * \param [in] vhost the virtual host to connect to on the broker. The default
 *              on most brokers is "/"
 * \param [in] channel_max the limit for number of channels for the connection.
 *              0 means no limit, and is a good default
 *              (AMQP_DEFAULT_MAX_CHANNELS)
 *              Note that the maximum number of channels the protocol supports
 *              is 65535 (2^16, with the 0-channel reserved). The server can
 *              set a lower channel_max and then the client will use the lowest
 *              of the two
 * \param [in] frame_max the maximum size of an AMQP frame on the wire to
 *              request of the broker for this connection. 4096 is the minimum
 *              size, 2^31-1 is the maximum, a good default is 131072 (128KB),
 *              or AMQP_DEFAULT_FRAME_SIZE
 * \param [in] heartbeat the number of seconds between heartbeat frames to
 *              request of the broker. A value of 0 disables heartbeats.
 *              Note rabbitmq-c only has partial support for heartbeats, as of
 *              v0.4.0 they are only serviced during amqp_basic_publish() and
 *              amqp_simple_wait_frame()/amqp_simple_wait_frame_noblock()
 * \param [in] sasl_method the SASL method to authenticate with the broker.
 *              followed by the authentication information. The following SASL
 *              methods are implemented:
 *              -  AMQP_SASL_METHOD_PLAIN, the AMQP_SASL_METHOD_PLAIN argument
 *                 should be followed by two arguments in this order:
 *                 const char* username, and const char* password.
 *              -  AMQP_SASL_METHOD_EXTERNAL, the AMQP_SASL_METHOD_EXTERNAL
 *                 argument should be followed one argument:
 *                 const char* identity.
 * \return amqp_rpc_reply_t indicating success or failure.
 *  - r.reply_type == AMQP_RESPONSE_NORMAL. Login completed successfully
 *  - r.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION. In most cases errors
 *    from the broker when logging in will be represented by the broker closing
 *    the socket. In this case r.library_error will be set to
 *    AMQP_STATUS_CONNECTION_CLOSED. This error can represent a number of
 *    error conditions including: invalid vhost, authentication failure.
 *  - r.reply_type == AMQP_RESPONSE_SERVER_EXCEPTION. The broker returned an
 *    exception:
 *    - If r.reply.id == AMQP_CHANNEL_CLOSE_METHOD a channel exception
 *      occurred, cast r.reply.decoded to amqp_channel_close_t* to see details
 *      of the exception. The client should amqp_send_method() a
 *      amqp_channel_close_ok_t. The channel must be re-opened before it
 *      can be used again. Any resources associated with the channel
 *      (auto-delete exchanges, auto-delete queues, consumers) are invalid
 *      and must be recreated before attempting to use them again.
 *    - If r.reply.id == AMQP_CONNECTION_CLOSE_METHOD a connection exception
 *      occurred, cast r.reply.decoded to amqp_connection_close_t* to see
 *      details of the exception. The client amqp_send_method() a
 *      amqp_connection_close_ok_t and disconnect from the broker.
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
amqp_rpc_reply_t AMQP_CALL amqp_login(amqp_connection_state_t state,
                                      char const *vhost, int channel_max,
                                      int frame_max, int heartbeat,
                                      amqp_sasl_method_enum sasl_method, ...);

/**
 * Login to the broker passing a properties table
 *
 * This function is similar to amqp_login() and differs in that it provides a
 * way to pass client properties to the broker. This is commonly used to
 * negotiate newer protocol features as they are supported by the broker.
 *
 * \param [in] state the connection object
 * \param [in] vhost the virtual host to connect to on the broker. The default
 *              on most brokers is "/"
 * \param [in] channel_max the limit for the number of channels for the
 *             connection.
 *             0 means no limit, and is a good default
 *             (AMQP_DEFAULT_MAX_CHANNELS)
 *             Note that the maximum number of channels the protocol supports
 *             is 65535 (2^16, with the 0-channel reserved). The server can
 *             set a lower channel_max and then the client will use the lowest
 *             of the two
 * \param [in] frame_max the maximum size of an AMQP frame ont he wire to
 *              request of the broker for this connection. 4096 is the minimum
 *              size, 2^31-1 is the maximum, a good default is 131072 (128KB),
 *              or AMQP_DEFAULT_FRAME_SIZE
 * \param [in] heartbeat the number of seconds between heartbeat frame to
 *             request of the broker. A value of 0 disables heartbeats.
 *             Note rabbitmq-c only has partial support for hearts, as of
 *             v0.4.0 heartbeats are only serviced during amqp_basic_publish(),
 *             and amqp_simple_wait_frame()/amqp_simple_wait_frame_noblock()
 * \param [in] properties a table of properties to send the broker.
 * \param [in] sasl_method the SASL method to authenticate with the broker
 *             followed by the authentication information. The following SASL
 *             methods are implemented:
 *             -  AMQP_SASL_METHOD_PLAIN, the AMQP_SASL_METHOD_PLAIN argument
 *                should be followed by two arguments in this order:
 *                const char* username, and const char* password.
 *             -  AMQP_SASL_METHOD_EXTERNAL, the AMQP_SASL_METHOD_EXTERNAL
 *                argument should be followed one argument:
 *                const char* identity.
 * \return amqp_rpc_reply_t indicating success or failure.
 *  - r.reply_type == AMQP_RESPONSE_NORMAL. Login completed successfully
 *  - r.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION. In most cases errors
 *    from the broker when logging in will be represented by the broker closing
 *    the socket. In this case r.library_error will be set to
 *    AMQP_STATUS_CONNECTION_CLOSED. This error can represent a number of
 *    error conditions including: invalid vhost, authentication failure.
 *  - r.reply_type == AMQP_RESPONSE_SERVER_EXCEPTION. The broker returned an
 *    exception:
 *    - If r.reply.id == AMQP_CHANNEL_CLOSE_METHOD a channel exception
 *      occurred, cast r.reply.decoded to amqp_channel_close_t* to see details
 *      of the exception. The client should amqp_send_method() a
 *      amqp_channel_close_ok_t. The channel must be re-opened before it
 *      can be used again. Any resources associated with the channel
 *      (auto-delete exchanges, auto-delete queues, consumers) are invalid
 *      and must be recreated before attempting to use them again.
 *    - If r.reply.id == AMQP_CONNECTION_CLOSE_METHOD a connection exception
 *      occurred, cast r.reply.decoded to amqp_connection_close_t* to see
 *      details of the exception. The client amqp_send_method() a
 *      amqp_connection_close_ok_t and disconnect from the broker.
 *
 * \since v0.4.0
 */
AMQP_PUBLIC_FUNCTION
amqp_rpc_reply_t AMQP_CALL amqp_login_with_properties(
    amqp_connection_state_t state, char const *vhost, int channel_max,
    int frame_max, int heartbeat, const amqp_table_t *properties,
    amqp_sasl_method_enum sasl_method, ...);

struct amqp_basic_properties_t_;

/**
 * Publish a message to the broker
 *
 * Publish a message on an exchange with a routing key.
 *
 * Note that at the AMQ protocol level basic.publish is an async method:
 * this means error conditions that occur on the broker (such as publishing to
 * a non-existent exchange) will not be reflected in the return value of this
 * function.
 *
 * \param [in] state the connection object
 * \param [in] channel the channel identifier
 * \param [in] exchange the exchange on the broker to publish to
 * \param [in] routing_key the routing key to use when publishing the message
 * \param [in] mandatory indicate to the broker that the message MUST be routed
 *              to a queue. If the broker cannot do this it should respond with
 *              a basic.return method.
 * \param [in] immediate indicate to the broker that the message MUST be
 *             delivered to a consumer immediately. If the broker cannot do this
 *             it should respond with a basic.return method.
 * \param [in] properties the properties associated with the message
 * \param [in] body the message body
 * \return AMQP_STATUS_OK on success, amqp_status_enum value on failure. Note
 *         that basic.publish is an async method, the return value from this
 *         function only indicates that the message data was successfully
 *         transmitted to the broker. It does not indicate failures that occur
 *         on the broker, such as publishing to a non-existent exchange.
 *         Possible error values:
 *         - AMQP_STATUS_TIMER_FAILURE: system timer facility returned an error
 *           the message was not sent.
 *         - AMQP_STATUS_HEARTBEAT_TIMEOUT: connection timed out waiting for a
 *           heartbeat from the broker. The message was not sent.
 *         - AMQP_STATUS_NO_MEMORY: memory allocation failed. The message was
 *           not sent.
 *         - AMQP_STATUS_TABLE_TOO_BIG: a table in the properties was too large
 *           to fit in a single frame. Message was not sent.
 *         - AMQP_STATUS_CONNECTION_CLOSED: the connection was closed.
 *         - AMQP_STATUS_SSL_ERROR: a SSL error occurred.
 *         - AMQP_STATUS_TCP_ERROR: a TCP error occurred. errno or
 *           WSAGetLastError() may provide more information
 *
 * Note: this function does heartbeat processing as of v0.4.0
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_basic_publish(
    amqp_connection_state_t state, amqp_channel_t channel,
    amqp_bytes_t exchange, amqp_bytes_t routing_key, amqp_boolean_t mandatory,
    amqp_boolean_t immediate, struct amqp_basic_properties_t_ const *properties,
    amqp_bytes_t body);

/**
 * Closes an channel
 *
 * \param [in] state the connection object
 * \param [in] channel the channel identifier
 * \param [in] code the reason for closing the channel, AMQP_REPLY_SUCCESS is a
 *             good default
 * \return amqp_rpc_reply_t indicating success or failure
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
amqp_rpc_reply_t AMQP_CALL amqp_channel_close(amqp_connection_state_t state,
                                              amqp_channel_t channel, int code);

/**
 * Closes the entire connection
 *
 * Implicitly closes all channels and informs the broker the connection
 * is being closed, after receiving acknowledgment from the broker it closes
 * the socket.
 *
 * \param [in] state the connection object
 * \param [in] code the reason code for closing the connection.
 *             AMQP_REPLY_SUCCESS is a good default.
 * \return amqp_rpc_reply_t indicating the result
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
amqp_rpc_reply_t AMQP_CALL amqp_connection_close(amqp_connection_state_t state,
                                                 int code);

/**
 * Acknowledges a message
 *
 * Does a basic.ack on a received message
 *
 * \param [in] state the connection object
 * \param [in] channel the channel identifier
 * \param [in] delivery_tag the delivery tag of the message to be ack'd
 * \param [in] multiple if true, ack all messages up to this delivery tag, if
 *              false ack only this delivery tag
 * \return 0 on success,  0 > on failing to send the ack to the broker.
 *            this will not indicate failure if something goes wrong on the
 *            broker
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_basic_ack(amqp_connection_state_t state,
                             amqp_channel_t channel, uint64_t delivery_tag,
                             amqp_boolean_t multiple);

/**
 * Do a basic.get
 *
 * Synchonously polls the broker for a message in a queue, and
 * retrieves the message if a message is in the queue.
 *
 * \param [in] state the connection object
 * \param [in] channel the channel identifier to use
 * \param [in] queue the queue name to retrieve from
 * \param [in] no_ack if true the message is automatically ack'ed
 *              if false amqp_basic_ack should be called once the message
 *              retrieved has been processed
 * \return amqp_rpc_reply indicating success or failure
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
amqp_rpc_reply_t AMQP_CALL amqp_basic_get(amqp_connection_state_t state,
                                          amqp_channel_t channel,
                                          amqp_bytes_t queue,
                                          amqp_boolean_t no_ack);

/**
 * Do a basic.reject
 *
 * Actively reject a message that has been delivered
 *
 * \param [in] state the connection object
 * \param [in] channel the channel identifier
 * \param [in] delivery_tag the delivery tag of the message to reject
 * \param [in] requeue indicate to the broker whether it should requeue the
 *              message or just discard it.
 * \return 0 on success, 0 > on failing to send the reject method to the broker.
 *          This will not indicate failure if something goes wrong on the
 * broker.
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_basic_reject(amqp_connection_state_t state,
                                amqp_channel_t channel, uint64_t delivery_tag,
                                amqp_boolean_t requeue);

/**
 * Do a basic.nack
 *
 * Actively reject a message, this has the same effect as amqp_basic_reject()
 * however, amqp_basic_nack() can negatively acknowledge multiple messages with
 * one call much like amqp_basic_ack() can acknowledge mutliple messages with
 * one call.
 *
 * \param [in] state the connection object
 * \param [in] channel the channel identifier
 * \param [in] delivery_tag the delivery tag of the message to reject
 * \param [in] multiple if set to 1 negatively acknowledge all unacknowledged
 *              messages on this channel.
 * \param [in] requeue indicate to the broker whether it should requeue the
 *              message or dead-letter it.
 * \return AMQP_STATUS_OK on success, an amqp_status_enum value otherwise.
 *
 * \since v0.5.0
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_basic_nack(amqp_connection_state_t state,
                              amqp_channel_t channel, uint64_t delivery_tag,
                              amqp_boolean_t multiple, amqp_boolean_t requeue);
/**
 * Check to see if there is data left in the receive buffer
 *
 * Can be used to see if there is data still in the buffer, if so
 * calling amqp_simple_wait_frame will not immediately enter a
 * blocking read.
 *
 * \param [in] state the connection object
 * \return true if there is data in the recieve buffer, false otherwise
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
amqp_boolean_t AMQP_CALL amqp_data_in_buffer(amqp_connection_state_t state);

/**
 * Get the error string for the given error code.
 *
 * \deprecated This function has been deprecated in favor of
 *  \ref amqp_error_string2() which returns statically allocated
 *  string which do not need to be freed by the caller.
 *
 * The returned string resides on the heap; the caller is responsible
 * for freeing it.
 *
 * \param [in] err return error code
 * \return the error string
 *
 * \since v0.1
 */
AMQP_DEPRECATED(
    AMQP_PUBLIC_FUNCTION char *AMQP_CALL amqp_error_string(int err));

/**
 * Get the error string for the given error code.
 *
 * Get an error string associated with an error code. The string is statically
 * allocated and does not need to be freed
 *
 * \param [in] err the error code
 * \return the error string
 *
 * \since v0.4.0
 */
AMQP_PUBLIC_FUNCTION
const char *AMQP_CALL amqp_error_string2(int err);

/**
 * Deserialize an amqp_table_t from AMQP wireformat
 *
 * This is an internal function and is not typically used by
 * client applications
 *
 * \param [in] encoded the buffer containing the serialized data
 * \param [in] pool memory pool used to allocate the table entries from
 * \param [in] output the amqp_table_t structure to fill in. Any existing
 *             entries will be erased
 * \param [in,out] offset The offset into the encoded buffer to start
 *                 reading the serialized table. It will be updated
 *                 by this function to end of the table
 * \return AMQP_STATUS_OK on success, an amqp_status_enum value on failure
 *  Possible error codes:
 *  - AMQP_STATUS_NO_MEMORY out of memory
 *  - AMQP_STATUS_BAD_AMQP_DATA invalid wireformat
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_decode_table(amqp_bytes_t encoded, amqp_pool_t *pool,
                                amqp_table_t *output, size_t *offset);

/**
 * Serializes an amqp_table_t to the AMQP wireformat
 *
 * This is an internal function and is not typically used by
 * client applications
 *
 * \param [in] encoded the buffer where to serialize the table to
 * \param [in] input the amqp_table_t to serialize
 * \param [in,out] offset The offset into the encoded buffer to start
 *                 writing the serialized table. It will be updated
 *                 by this function to where writing left off
 * \return AMQP_STATUS_OK on success, an amqp_status_enum value on failure
 *  Possible error codes:
 *  - AMQP_STATUS_TABLE_TOO_BIG the serialized form is too large for the
 *    buffer
 *  - AMQP_STATUS_BAD_AMQP_DATA invalid table
 *
 * \since v0.1
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_encode_table(amqp_bytes_t encoded, amqp_table_t *input,
                                size_t *offset);

/**
 * Create a deep-copy of an amqp_table_t object
 *
 * Creates a deep-copy of an amqp_table_t object, using the provided pool
 * object to allocate the necessary memory. This memory can be freed later by
 * call recycle_amqp_pool(), or empty_amqp_pool()
 *
 * \param [in] original the table to copy
 * \param [in,out] clone the table to copy to
 * \param [in] pool the initialized memory pool to do allocations for the table
 *             from
 * \return AMQP_STATUS_OK on success, amqp_status_enum value on failure.
 *  Possible error values:
 *  - AMQP_STATUS_NO_MEMORY - memory allocation failure.
 *  - AMQP_STATUS_INVALID_PARAMETER - invalid table (e.g., no key name)
 *
 * \since v0.4.0
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_table_clone(const amqp_table_t *original,
                               amqp_table_t *clone, amqp_pool_t *pool);

/**
 * A message object
 *
 * \since v0.4.0
 */
typedef struct amqp_message_t_ {
  amqp_basic_properties_t properties; /**< message properties */
  amqp_bytes_t body;                  /**< message body */
  amqp_pool_t pool;                   /**< pool used to allocate properties */
} amqp_message_t;

/**
 * Reads the next message on a channel
 *
 * Reads a complete message (header + body) on a specified channel. This
 * function is intended to be used with amqp_basic_get() or when an
 * AMQP_BASIC_DELIVERY_METHOD method is received.
 *
 * \param [in,out] state the connection object
 * \param [in] channel the channel on which to read the message from
 * \param [in,out] message a pointer to a amqp_message_t object. Caller should
 *                 call amqp_message_destroy() when it is done using the
 *                 fields in the message object.  The caller is responsible for
 *                 allocating/destroying the amqp_message_t object itself.
 * \param [in] flags pass in 0. Currently unused.
 * \returns a amqp_rpc_reply_t object. ret.reply_type == AMQP_RESPONSE_NORMAL on
 * success.
 *
 * \since v0.4.0
 */
AMQP_PUBLIC_FUNCTION
amqp_rpc_reply_t AMQP_CALL amqp_read_message(amqp_connection_state_t state,
                                             amqp_channel_t channel,
                                             amqp_message_t *message,
                                             int flags);

/**
 * Frees memory associated with a amqp_message_t allocated in amqp_read_message
 *
 * \param [in] message
 *
 * \since v0.4.0
 */
AMQP_PUBLIC_FUNCTION
void AMQP_CALL amqp_destroy_message(amqp_message_t *message);

/**
 * Envelope object
 *
 * \since v0.4.0
 */
typedef struct amqp_envelope_t_ {
  amqp_channel_t channel; /**< channel message was delivered on */
  amqp_bytes_t
      consumer_tag;      /**< the consumer tag the message was delivered to */
  uint64_t delivery_tag; /**< the messages delivery tag */
  amqp_boolean_t redelivered; /**< flag indicating whether this message is being
                                 redelivered */
  amqp_bytes_t exchange;      /**< exchange this message was published to */
  amqp_bytes_t
      routing_key; /**< the routing key this message was published with */
  amqp_message_t message; /**< the message */
} amqp_envelope_t;

/**
 * Wait for and consume a message
 *
 * Waits for a basic.deliver method on any channel, upon receipt of
 * basic.deliver it reads that message, and returns. If any other method is
 * received before basic.deliver, this function will return an amqp_rpc_reply_t
 * with ret.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION, and
 * ret.library_error == AMQP_STATUS_UNEXPECTED_STATE. The caller should then
 * call amqp_simple_wait_frame() to read this frame and take appropriate action.
 *
 * This function should be used after starting a consumer with the
 * amqp_basic_consume() function
 *
 * \param [in,out] state the connection object
 * \param [in,out] envelope a pointer to a amqp_envelope_t object. Caller
 *                 should call #amqp_destroy_envelope() when it is done using
 *                 the fields in the envelope object. The caller is responsible
 *                 for allocating/destroying the amqp_envelope_t object itself.
 * \param [in] timeout a timeout to wait for a message delivery. Passing in
 *             NULL will result in blocking behavior.
 * \param [in] flags pass in 0. Currently unused.
 * \returns a amqp_rpc_reply_t object.  ret.reply_type == AMQP_RESPONSE_NORMAL
 *          on success. If ret.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION,
 *          and ret.library_error == AMQP_STATUS_UNEXPECTED_STATE, a frame other
 *          than AMQP_BASIC_DELIVER_METHOD was received, the caller should call
 *          amqp_simple_wait_frame() to read this frame and take appropriate
 *          action.
 *
 * \since v0.4.0
 */
AMQP_PUBLIC_FUNCTION
amqp_rpc_reply_t AMQP_CALL amqp_consume_message(amqp_connection_state_t state,
                                                amqp_envelope_t *envelope,
                                                struct timeval *timeout,
                                                int flags);

/**
 * Frees memory associated with a amqp_envelope_t allocated in
 * amqp_consume_message()
 *
 * \param [in] envelope
 *
 * \since v0.4.0
 */
AMQP_PUBLIC_FUNCTION
void AMQP_CALL amqp_destroy_envelope(amqp_envelope_t *envelope);

/**
 * Parameters used to connect to the RabbitMQ broker
 *
 * \since v0.2
 */
struct amqp_connection_info {
  char *user; /**< the username to authenticate with the broker, default on most
                 broker is 'guest' */
  char *password; /**< the password to authenticate with the broker, default on
                     most brokers is 'guest' */
  char *host;     /**< the hostname of the broker */
  char *vhost; /**< the virtual host on the broker to connect to, a good default
                  is "/" */
  int port;    /**< the port that the broker is listening on, default on most
                  brokers is 5672 */
  amqp_boolean_t ssl;
};

/**
 * Initialze an amqp_connection_info to default values
 *
 * The default values are:
 * - user: "guest"
 * - password: "guest"
 * - host: "localhost"
 * - vhost: "/"
 * - port: 5672
 *
 * \param [out] parsed the connection info to set defaults on
 *
 * \since v0.2
 */
AMQP_PUBLIC_FUNCTION
void AMQP_CALL
    amqp_default_connection_info(struct amqp_connection_info *parsed);

/**
 * Parse a connection URL
 *
 * An amqp connection url takes the form:
 *
 * amqp://[$USERNAME[:$PASSWORD]\@]$HOST[:$PORT]/[$VHOST]
 *
 * Examples:
 *  amqp://guest:guest\@localhost:5672//
 *  amqp://guest:guest\@localhost/myvhost
 *
 *  Any missing parts of the URL will be set to the defaults specified in
 *  amqp_default_connection_info. For amqps: URLs the default port will be set
 *  to 5671 instead of 5672 for non-SSL URLs.
 *
 * \note This function modifies url parameter.
 *
 * \param [in] url URI to parse, note that this parameter is modified by the
 *             function.
 * \param [out] parsed the connection info gleaned from the URI. The char*
 *              members will point to parts of the url input parameter.
 *              Memory management will depend on how the url is allocated.
 * \returns AMQP_STATUS_OK on success, AMQP_STATUS_BAD_URL on failure
 *
 * \since v0.2
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_parse_url(char *url, struct amqp_connection_info *parsed);

/* socket API */

/**
 * Open a socket connection.
 *
 * This function opens a socket connection returned from amqp_tcp_socket_new()
 * or amqp_ssl_socket_new(). This function should be called after setting
 * socket options and prior to assigning the socket to an AMQP connection with
 * amqp_set_socket().
 *
 * \param [in,out] self A socket object.
 * \param [in] host Connect to this host.
 * \param [in] port Connect on this remote port.
 *
 * \return AMQP_STATUS_OK on success, an amqp_status_enum on failure
 *
 * \since v0.4.0
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_socket_open(amqp_socket_t *self, const char *host, int port);

/**
 * Open a socket connection.
 *
 * This function opens a socket connection returned from amqp_tcp_socket_new()
 * or amqp_ssl_socket_new(). This function should be called after setting
 * socket options and prior to assigning the socket to an AMQP connection with
 * amqp_set_socket().
 *
 * \param [in,out] self A socket object.
 * \param [in] host Connect to this host.
 * \param [in] port Connect on this remote port.
 * \param [in] timeout Max allowed time to spent on opening. If NULL - run in
 *             blocking mode
 *
 * \return AMQP_STATUS_OK on success, an amqp_status_enum on failure.
 *
 * \since v0.4.0
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_socket_open_noblock(amqp_socket_t *self, const char *host,
                                       int port, struct timeval *timeout);

/**
 * Get the socket descriptor in use by a socket object.
 *
 * Retrieve the underlying socket descriptor. This function can be used to
 * perform low-level socket operations that aren't supported by the socket
 * interface. Use with caution!
 *
 * \param [in,out] self A socket object.
 *
 * \return The underlying socket descriptor, or -1 if there is no socket
 *  descriptor associated with
 *
 * \since v0.4.0
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_socket_get_sockfd(amqp_socket_t *self);

/**
 * Get the socket object associated with a amqp_connection_state_t
 *
 * \param [in] state the connection object to get the socket from
 * \return a pointer to the socket object, or NULL if one has not been assigned
 *
 * \since v0.4.0
 */
AMQP_PUBLIC_FUNCTION
amqp_socket_t *AMQP_CALL amqp_get_socket(amqp_connection_state_t state);

/**
 * Get the broker properties table
 *
 * \param [in] state the connection object
 * \return a pointer to an amqp_table_t containing the properties advertised
 *  by the broker on connection. The connection object owns the table, it
 *  should not be modified.
 *
 * \since v0.5.0
 */
AMQP_PUBLIC_FUNCTION
amqp_table_t *AMQP_CALL
    amqp_get_server_properties(amqp_connection_state_t state);

/**
 * Get the client properties table
 *
 * Get the properties that were passed to the broker on connection.
 *
 * \param [in] state the connection object
 * \return a pointer to an amqp_table_t containing the properties advertised
 *  by the client on connection. The connection object owns the table, it
 *  should not be modified.
 *
 * \since v0.7.0
 */
AMQP_PUBLIC_FUNCTION
amqp_table_t *AMQP_CALL
    amqp_get_client_properties(amqp_connection_state_t state);

/**
 * Get the login handshake timeout.
 *
 * amqp_login and amqp_login_with_properties perform the login handshake with
 * the broker.  This function returns the timeout associated with completing
 * this operation from the client side. This value can be set by using the
 * amqp_set_handshake_timeout.
 *
 * Note that the RabbitMQ broker has configurable timeout for completing the
 * login handshake, the default is 10 seconds.  rabbitmq-c has a default of 12
 * seconds.
 *
 * \param [in] state the connection object
 * \return a struct timeval representing the current login timeout for the state
 *  object. A NULL value represents an infinite timeout. The memory returned is
 *  owned by the connection object.
 *
 * \since v0.9.0
 */
AMQP_PUBLIC_FUNCTION
struct timeval *AMQP_CALL
    amqp_get_handshake_timeout(amqp_connection_state_t state);

/**
 * Set the login handshake timeout.
 *
 * amqp_login and amqp_login_with_properties perform the login handshake with
 * the broker. This function sets the timeout associated with completing this
 * operation from the client side.
 *
 * The timeout must be set before amqp_login or amqp_login_with_properties is
 * called to change from the default timeout.
 *
 * Note that the RabbitMQ broker has a configurable timeout for completing the
 * login handshake, the default is 10 seconds. rabbitmq-c has a default of 12
 * seconds.
 *
 * \param [in] state the connection object
 * \param [in] timeout a struct timeval* representing new login timeout for the
 *  state object. NULL represents an infinite timeout. The value of timeout is
 *  copied internally, the caller is responsible for ownership of the passed in
 *  pointer, it does not need to remain valid after this function is called.
 * \return AMQP_STATUS_OK on success.
 *
 * \since v0.9.0
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_set_handshake_timeout(amqp_connection_state_t state,
                                         struct timeval *timeout);

/**
 * Get the RPC timeout
 *
 * Gets the timeout for any RPC-style AMQP command (e.g., amqp_queue_declare).
 * This timeout may be changed at any time by calling \amqp_set_rpc_timeout
 * function with a new timeout. The timeout applies individually to each RPC
 * that is made.
 *
 * The default value is NULL, or an infinite timeout.
 *
 * When an RPC times out, the function will return an error AMQP_STATUS_TIMEOUT,
 * and the connection will be closed.
 *
 *\warning RPC-timeouts are an advanced feature intended to be used to detect
 * dead connections quickly when the rabbitmq-c implementation of heartbeats
 * does not work. Do not use RPC timeouts unless you understand the implications
 * of doing so.
 *
 * \param [in] state the connection object
 * \return a struct timeval representing the current RPC timeout for the state
 * object. A NULL value represents an infinite timeout. The memory returned is
 * owned by the connection object.
 *
 * \since v0.9.0
 */
AMQP_PUBLIC_FUNCTION
struct timeval *AMQP_CALL amqp_get_rpc_timeout(amqp_connection_state_t state);

/**
 * Set the RPC timeout
 *
 * Sets the timeout for any RPC-style AMQP command (e.g., amqp_queue_declare).
 * This timeout may be changed at any time by calling this function with a new
 * timeout. The timeout applies individually to each RPC that is made.
 *
 * The default value is NULL, or an infinite timeout.
 *
 * When an RPC times out, the function will return an error AMQP_STATUS_TIMEOUT,
 * and the connection will be closed.
 *
 *\warning RPC-timeouts are an advanced feature intended to be used to detect
 * dead connections quickly when the rabbitmq-c implementation of heartbeats
 * does not work. Do not use RPC timeouts unless you understand the implications
 * of doing so.
 *
 * \param [in] state the connection object
 * \param [in] timeout a struct timeval* representing new RPC timeout for the
 * state object. NULL represents an infinite timeout. The value of timeout is
 * copied internally, the caller is responsible for ownership of the passed
 * pointer, it does not need to remain valid after this function is called.
 * \return AMQP_STATUS_SUCCESS on success.
 *
 * \since v0.9.0
 */
AMQP_PUBLIC_FUNCTION
int AMQP_CALL amqp_set_rpc_timeout(amqp_connection_state_t state,
                                   struct timeval *timeout);

AMQP_END_DECLS

#endif /* AMQP_H */
