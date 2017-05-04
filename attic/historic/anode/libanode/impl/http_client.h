/* libanode: the Anode C reference implementation
 * Copyright (C) 2009-2010 Adam Ierymenko <adam.ierymenko@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef _ANODE_HTTP_CLIENT_H
#define _ANODE_HTTP_CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include "dictionary.h"
#include "../anode.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * HTTP request type
 */
enum AnodeHttpClientRequestMethod
{
  ANODE_HTTP_GET = 0,
  ANODE_HTTP_HEAD = 1,
  ANODE_HTTP_POST = 2
};

/*
 * Special response codes to indicate I/O errors
 */
#define ANODE_HTTP_SPECIAL_RESPONSE_DNS_RESOLVE_FAILED -1
#define ANODE_HTTP_SPECIAL_RESPONSE_CONNECT_FAILED -2
#define ANODE_HTTP_SPECIAL_RESPONSE_HEADERS_TOO_LARGE -3
#define ANODE_HTTP_SPECIAL_RESPONSE_SERVER_CLOSED_CONNECTION -4
#define ANODE_HTTP_SPECIAL_RESPONSE_INVALID_RESPONSE -5

/**
 * Simple HTTP client
 */
struct AnodeHttpClient
{
  /**
   * Request URI
   */
  AnodeURI uri;

  /**
   * Request method: GET, PUT, HEAD, or POST
   */
  enum AnodeHttpClientRequestMethod method;

  /**
   * Data for POST requests
   *
   * It is your responsibility to manage and/or free this pointer. The HTTP
   * client only reads from it.
   */
  const void *data;
  unsigned int data_length;

  /**
   * Content type for data, or null for application/x-www-form-urlencoded
   */
  const char *data_content_type;

  /**
   * Set to non-zero to use HTTP connection keepalive
   *
   * If keepalive is enabled, this request can be modified and re-used and
   * its associated connection will stay open (being reopened if needed)
   * until it is freed.
   *
   * Note that this client is too dumb to pool connections and pick them on
   * the basis of host. Keepalive mode should only be set if the next request
   * will be from the same host and port, otherwise you will get a '404'.
   */
  int keepalive;

  /**
   * Function pointer to be called when request is complete (or fails)
   */
  void (*handler)(struct AnodeHttpClient *);

  /**
   * Two arbitrary pointers that can be stored here for use by the handler.
   * These are not accessed or modified by the client.
   */
  void *ptr[2];

  /**
   * Request headers
   */
  struct AnodeDictionary headers;

  struct {
    /**
     * Response code, set on completion or failure before handler is called
     *
     * Also check for the special response codes defined in http_client.h as
     * these negative codes indicate network or other errors.
     */
    int code;

    /**
     * Response data, for GET and POST requests
     */
    void *data;

    /**
     * Length of response data
     */
    unsigned int data_length;

    /**
     * Response headers
     */
    struct AnodeDictionary headers;
  } response;

  /**
   * Internal fields used by implementation
   */
  struct {
    /* Transport engine being used by request */
    AnodeTransportEngine *transport_engine;

    /* Connection to which request has been sent, or null if none */
    struct AnodeHttpConnection *connection;

    /* Buffer for reading chunked mode chunk lines (can't use data buf) */
    char header_line_buf[256];
    unsigned int header_line_buf_ptr;

    /* Where are we in sending request data? */
    unsigned int request_data_ptr;

    /* Capacity of response_data buffer */
    unsigned int response_data_capacity;

    /* How much response data are we currently expecting? */
    /* This is content-length in block mode or chunk length in chunked mode */
    unsigned int expecting_response_length;

    /* Read mode */
    enum {
      ANODE_HTTP_READ_MODE_WAITING = 0,
      ANODE_HTTP_READ_MODE_HEADERS = 1,
      ANODE_HTTP_READ_MODE_BLOCK = 2,
      ANODE_HTTP_READ_MODE_CHUNKED_CHUNK_SIZE = 3,
      ANODE_HTTP_READ_MODE_CHUNKED_DATA = 4,
      ANODE_HTTP_READ_MODE_CHUNKED_FOOTER = 5
    } read_mode;

    /* Connection from transport engine */
    AnodeTransportTcpConnection *tcp_connection;

    /* Write buffer */
    unsigned char outbuf[16384];
    unsigned int outbuf_len;

    /* Phase of request state machine */
    enum {
      ANODE_HTTP_REQUEST_PHASE_RESOLVE = 0,
      ANODE_HTTP_REQUEST_PHASE_CONNECT = 1,
      ANODE_HTTP_REQUEST_PHASE_SEND = 2,
      ANODE_HTTP_REQUEST_PHASE_RECEIVE = 3,
      ANODE_HTTP_REQUEST_PHASE_KEEPALIVE = 4,
      ANODE_HTTP_REQUEST_PHASE_CLOSED = 5
    } phase;

    /* Has request object been freed? */
    int freed;

    /**
     * Pointer used internally for putting requests into linked lists
     */
    struct AnodeHttpClient *next;
  } impl;
};

struct AnodeHttpClient *AnodeHttpClient_new(AnodeTransportEngine *transport_engine);
void AnodeHttpClient_send(struct AnodeHttpClient *client);
void AnodeHttpClient_free(struct AnodeHttpClient *client);

#ifdef __cplusplus
}
#endif

#endif
